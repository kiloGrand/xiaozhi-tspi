#include "xiaozhi_control.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <cstring>
#include <algorithm>
#include "json.hpp"
#include "uuid.h"
#include "mcp_server.hpp"

using json = nlohmann::json;

// 单例实例
XiaozhiControlCenter& XiaozhiControlCenter::instance() {
    static XiaozhiControlCenter inst;
    return inst;
}

// 构造函数
XiaozhiControlCenter::XiaozhiControlCenter() {
    init_mac();
    init_uuid();
    McpServer::GetInstance().AddCommonTools();
    std::cout << "MCP Server 初始化完成" << std::endl;
}

// 析构函数：先停音频线程再销毁其他组件
XiaozhiControlCenter::~XiaozhiControlCenter() {
    if (capture_) capture_->stop();
    if (player_) player_->stop();
}

// ==================== 初始化硬件信息 ====================
void XiaozhiControlCenter::init_mac() {
    mac_ = get_wireless_mac_address();
    if (mac_.empty()) {
        mac_ = "00:00:00:00:00:00";
    }
    std::cout << "MAC: " << mac_ << std::endl;
}

void XiaozhiControlCenter::init_uuid() {
    uuid_ = read_uuid_from_config(CFG_FILE);
    if (uuid_.empty()) {
        uuid_ = generate_uuid();
        write_uuid_to_config(uuid_, CFG_FILE);
    }
    std::cout << "UUID: " << uuid_ << std::endl;
}

// ==================== 初始化音频（替代原UDP音频端点） ====================
void XiaozhiControlCenter::init_audio() {
    using namespace AlsaAudio;

    const auto frame_60ms_bytes = kSampleRateRec * kOpusFrameMs / 1000 * kChannels * sizeof(opus_int16);

    // 编码器：录音16k → Opus
    encoder_ = std::make_unique<OpusEncoder>(
        kSampleRateRec, kChannels, kOpusFrameMs, kSampleRateRec, kChannels);
    // 解码器：Opus → 播放24k
    decoder_ = std::make_unique<OpusDecoder>(
        kSampleRatePlay, kChannels, kOpusFrameMs, kSampleRatePlay, kChannels);

    if (!encoder_->isValid() || !decoder_->isValid()) {
        std::cerr << "Opus编解码器初始化失败" << std::endl;
        return;
    }

    // 缓冲
    record_buffer_.resize(kPcmBufferSize);
    record_offset_ = 0;
    play_buffer_.resize(kPcmBufferSize);
    play_offset_ = 0;

    // === 启动录音 ===
    // 录音回调：累积60ms → 编码 → 直接调ws_client发送（无队列、无UDP）
    capture_ = std::make_unique<AlsaCapture>(
        DEFAULT_DEVICE, kSampleRateRec, kChannels, SND_PCM_FORMAT_S16_LE);
    if (!capture_->start([this, frame_60ms_bytes](std::vector<uint8_t>& buffer, size_t size) {
        if (!capture_->is_running()) return;

        if (record_offset_ + size > record_buffer_.size()) return;

        memcpy(record_buffer_.data() + record_offset_, buffer.data(), size);
        record_offset_ += size;

        if (record_offset_ >= frame_60ms_bytes) {
            size_t i = 0;
            while (i < record_offset_) {
                size_t pcmsize = frame_60ms_bytes;
                if (i + pcmsize > record_offset_) break;

                std::vector<uint8_t> opus_buf(kOpusBufferSize);
                int opus_len = 0;
                encoder_->encode(record_buffer_.data() + i, (int)pcmsize,
                                  opus_buf.data(), &opus_len);

                if (opus_len > 0 && audio_upload_enable_ && ws_client_
                    && ws_client_->is_connected()) {
                    ws_client_->send_binary((const char*)opus_buf.data(), opus_len);
                }
                i += pcmsize;
            }
            if (record_offset_ > i) {
                memmove(record_buffer_.data(), record_buffer_.data() + i,
                        record_offset_ - i);
            }
            record_offset_ -= i;
        }
    })) {
        std::cerr << "录音启动失败" << std::endl;
        return;
    }

    // === 启动播放 ===
    // 播放回调：从下行队列取Opus → 解码 → 填PCM给ALSA
    player_ = std::make_unique<AlsaPlayback>(
        DEFAULT_DEVICE, kSampleRatePlay, kChannels, SND_PCM_FORMAT_S16_LE);
    if (!player_->start([this](std::vector<uint8_t>& buffer, size_t max_size) -> int {
        if (!player_->is_running()) return 0;

        // 先消耗 play_buffer_ 中上次剩余的 PCM 数据
        if (play_offset_ > 0) {
            size_t copy_size = std::min(play_offset_, max_size);
            memcpy(buffer.data(), play_buffer_.data(), copy_size);
            if (play_offset_ > copy_size) {
                memmove(play_buffer_.data(), play_buffer_.data() + copy_size,
                        play_offset_ - copy_size);
            }
            play_offset_ -= copy_size;
            return (int)copy_size;
        }

        // play_buffer_ 已空，从下行队列取 Opus 帧解码
        std::vector<uint8_t> opus_buf;
        {
            std::unique_lock<std::mutex> lock(opus_downlink_mutex_);
            if (opus_downlink_queue_.empty()) {
                opus_downlink_cv_.wait_for(lock, std::chrono::milliseconds(100), [this] {
                    return !opus_downlink_queue_.empty() || !player_->is_running();
                });
                if (!player_->is_running() || opus_downlink_queue_.empty()) return 0;
            }
            opus_buf = std::move(opus_downlink_queue_.front());
            opus_downlink_queue_.pop();
        }

        int pcm_data_size = 0;
        decoder_->decode(opus_buf.data(), (int)opus_buf.size(),
                          play_buffer_.data(), &pcm_data_size);
        if (pcm_data_size <= 0) return 0;

        play_offset_ = pcm_data_size;

        // 本次最多输出 max_size 字节
        size_t copy_size = std::min(play_offset_, max_size);
        memcpy(buffer.data(), play_buffer_.data(), copy_size);
        if (play_offset_ > copy_size) {
            memmove(play_buffer_.data(), play_buffer_.data() + copy_size,
                    play_offset_ - copy_size);
        }
        play_offset_ -= copy_size;
        return (int)copy_size;
    })) {
        capture_->stop();
        std::cerr << "播放启动失败" << std::endl;
        return;
    }

    std::cout << "音频模块初始化成功" << std::endl;
}

// ==================== 初始化UI UDP（保留，qt_gui独立进程走UDP） ====================
void XiaozhiControlCenter::init_ui_udp() {
    ui_ep_ = std::make_unique<UdpEndpoint>(UI_PORT_UP, UI_PORT_DOWN);
    ui_ep_->setRecvCallback([this](const std::vector<uint8_t>& data) {
        this->on_ui_data((char*)data.data(), data.size());
    });
    std::cout << "初始化 UI UDP 成功" << std::endl;
}

// ==================== 设备状态管理 ====================
void XiaozhiControlCenter::set_device_state(DeviceState state) {
    device_state_ = state;
}

void XiaozhiControlCenter::send_device_state() {
    auto current_state = device_state_.load();
    std::string state_str = "{\"state\":" + std::to_string(static_cast<int>(current_state)) + "}";
    try {
        ui_ep_->send((const uint8_t*)state_str.data(), state_str.size());
    } catch(const std::exception& e) {
        std::cout << "send_device_state error: " << e.what() << '\n';
    }
}

void XiaozhiControlCenter::send_stt(const std::string& text) {
    try {
        ui_ep_->send((const uint8_t*)text.data(), text.size());
    } catch (const std::exception& e) {
        std::cout << "send_stt error: " << e.what() << std::endl;
    }
}

// ==================== UI UDP回调 ====================
int XiaozhiControlCenter::on_ui_data(char* buffer, size_t size) {
    std::cout << "UI command received, start websocket..." << std::endl;
    set_device_state(DeviceState::Listening);
    send_device_state();
    ws_client_->start();
    return 0;
}

// ==================== WebSocket回调 ====================
void XiaozhiControlCenter::on_audio_downloaded(const char* buffer, size_t size) {
    // 非阻塞推入下行队列（在 ASIO 线程中执行，不可阻塞）
    {
        std::unique_lock<std::mutex> lock(opus_downlink_mutex_);
        if (opus_downlink_queue_.size() >= kMaxDownlinkQueueSize) {
            opus_downlink_queue_.pop(); // 丢弃最旧的帧，避免阻塞 ASIO 线程
        }
        opus_downlink_queue_.emplace(buffer, buffer + size);
    }
    opus_downlink_cv_.notify_one();
}

void XiaozhiControlCenter::on_text_downloaded(const char* buffer, size_t size) {
    try {
        json j = json::parse(buffer);
        if (j.contains("type") && j["type"] == "hello") {
            process_hello_json(buffer, size);
        } else {
            process_other_json(buffer, size);
        }
    } catch (const std::exception& e) {
        std::cout << "parse text error: " << e.what() << std::endl;
    }
}

void XiaozhiControlCenter::on_websocket_closed(short close_code) {
    std::cout << "WebSocket closed, code: " << close_code << std::endl;
    audio_upload_enable_ = false;

    if (close_code == 1005) {
        std::cout << "Server timeout closed" << std::endl;
    }

    set_device_state(DeviceState::Idle);
    send_device_state();
}

// ==================== 协议发送 ====================
void XiaozhiControlCenter::send_start_listening_req(ListeningMode mode) {
    std::string startString = "{\"session_id\":\"" + session_id_ + "\"";
    startString += ",\"type\":\"listen\",\"state\":\"start\"";

    if (mode == ListeningMode::AutoStop) {
        startString += ",\"mode\":\"auto\"}";
    } else if (mode == ListeningMode::ManualStop) {
        startString += ",\"mode\":\"manual\"}";
    } else {
        startString += ",\"mode\":\"realtime\"}";
    }

    try {
        ws_client_->send_text(startString.data(), (int)startString.size());
        std::cout << "Send: " << startString << std::endl;
    } catch(const websocketpp::lib::error_code& e) {
        std::cout << "Error sending message: " << e << " (" << e.message() << ")" << std::endl;
    }
}

// ==================== Hello协议处理 ====================
void XiaozhiControlCenter::process_hello_json(const char* buffer, size_t size) {
    json j = json::parse(buffer);
    int sample_rate = j["audio_params"]["sample_rate"];
    int channels = j["audio_params"]["channels"];
    std::cout << "Received valid 'hello' message with sample_rate: " << sample_rate
              << " and channels: " << channels << std::endl;

    session_id_ = j["session_id"];

    std::string desc = R"(
    {"session_id":"","type":"iot","update":true,"descriptors":[{"name":"Speaker","description":"扬声器","properties":{"volume":{"description":"当前音量值","type":"number"}},"methods":{"SetVolume":{"description":"设置音量","parameters":{"volume":{"description":"0到100之间的整数","type":"number"}}}}}]}
    )";
    try {
        ws_client_->send_text(desc.data(), desc.size());
        std::cout << "Send: " << desc << std::endl;
    } catch (const websocketpp::lib::error_code& e) {
        std::cout << "Error sending message: " << e << " (" << e.message() << ")" << std::endl;
    }

    std::string startString = R"(
        {"session_id":"","type":"listen","state":"start","mode":"auto"}
    )";
    try {
        ws_client_->send_text(startString.data(), startString.size());
        std::cout << "Send: " << startString << std::endl;
    } catch (const websocketpp::lib::error_code& e) {
        std::cout << "Error sending message: " << e << " (" << e.message() << ")" << std::endl;
    }

    audio_upload_enable_ = true;
}

// ===================== MCP 核心处理函数 =====================
void XiaozhiControlCenter::handle_mcp_request(const json& mcp_json) {
    try {
        std::string mcp_request = mcp_json.dump(-1);
        std::cout << "收到MCP请求: " << mcp_request << std::endl;

        McpServer& mcp = McpServer::GetInstance();
        mcp.ParseMessage(mcp_request);

        std::string mcp_response = mcp.GetLastResponse();
        std::cout << "MCP执行结果: " << mcp_response << std::endl;

        json resp;
        resp["session_id"] = session_id_;
        resp["type"] = "mcp";
        resp["payload"] = json::parse(mcp_response);
        std::string message = resp.dump(-1);

        ws_client_->send_text(message.data(), message.size());
        std::cout << "发送MCP响应到云端: " << message << std::endl;
    } catch (const std::exception& e) {
        std::cout << "handle_mcp_request error: " << e.what() << std::endl;
    }
}

// ==================== 其他JSON处理 ====================
void XiaozhiControlCenter::process_other_json(const char* buffer, size_t size) {
    try {
        json j = json::parse(buffer);
        if (!j.contains("type")) return;

        std::string type = j["type"];
        if (type == "tts") {
            std::string state = j["state"];
            if (state == "start") {
                audio_upload_enable_ = false;
                set_device_state(DeviceState::Listening);
                send_device_state();
            } else if (state == "stop") {
                std::this_thread::sleep_for(std::chrono::seconds(2));
                send_start_listening_req(ListeningMode::AutoStop);
                set_device_state(DeviceState::Listening);
                send_device_state();
                audio_upload_enable_ = true;
            } else if (state == "sentence_start") {
                send_stt(j.dump());
                send_start_listening_req(ListeningMode::AutoStop);
                set_device_state(DeviceState::Speaking);
                send_device_state();
            }
        } else if (type == "stt" || type == "llm") {
            send_stt(j.dump());
        } else if (type == "mcp") {
            handle_mcp_request(j["payload"]);
        }
    } catch (const std::exception& e) {
        std::cout << "process json error: " << e.what() << std::endl;
    }
}

// ==================== 设备激活 ====================
void XiaozhiControlCenter::device_activate() {
    http_data_t http_data{};
    http_data.url = "https://api.tenclass.net/xiaozhi/ota/";

    std::ostringstream post_stream;
    post_stream << R"(
        {
            "uuid":")" << uuid_ << R"(",
            "application": {
                "name": "xiaozhi_linux_100ask",
                "version": "1.0.0"
            },
            "ota": {
            },
            "board": {
                "type": "100ask_linux_board",
                "name": "100ask_imx6ull_board"
            }
        }
    )";
    http_data.post = post_stream.str();

    std::ostringstream headers_stream;
    headers_stream << R"(
        {
            "Content-Type": "application/json",
            "Device-Id": ")" << mac_ << R"(",
            "User-Agent": "weidongshan1",
            "Accept-Language": "zh-CN"
        }
    )";
    http_data.headers = headers_stream.str();

    char active_code[20] = {0};
    while (0 != active_device(&http_data, active_code)) {
        if (active_code[0]) {
            std::string auth_code = R"({"type":"tts","text":"Active-Code: )"
                                    + std::string(active_code)
                                    + R"("})";
            set_device_state(DeviceState::Activating);
            send_device_state();
            send_stt(auth_code);
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

// ==================== 初始化WebSocket ====================
void XiaozhiControlCenter::init_websocket() {
    std::ostringstream ws_header_ss;
    ws_header_ss << R"(
        {
            "Authorization": "Bearer test-token",
            "Protocol-Version": "1",
            "Device-Id": ")" << mac_ << R"(",
            "Client-Id": ")" << uuid_ << R"("
        }
    )";

    std::string hello = R"(
        {
            "type": "hello",
            "version": 1,
            "transport": "websocket",
            "features": {
                "mcp": true
            },
            "audio_params": {
                "format": "opus",
                "sample_rate": 16000,
                "channels": 1,
                "frame_duration": 60
            }
        })";

    ws_client_ = std::make_unique<WebSocketClient>(
        "api.tenclass.net",
        "443",
        "/xiaozhi/v1/",
        hello,
        ws_header_ss.str()
    );

    ws_client_->set_callbacks(
        [this](const char* buf, size_t sz) { this->on_audio_downloaded(buf, sz); },
        [this](const char* buf, size_t sz) { this->on_text_downloaded(buf, sz); },
        [this](short code) { this->on_websocket_closed(code); }
    );
}

// ==================== 主运行入口 ====================
void XiaozhiControlCenter::run() {
    // 1. 初始化音频（替代原UDP音频端点：录音→编码→直发ws，云端→队列→解码→播放）
    init_audio();

    // 2. 初始化UI UDP（保留，qt_gui独立进程走UDP）
    init_ui_udp();

    // 3. 设备激活
    device_activate();
    std::cout << "设备激活成功" << std::endl;

    // 4. 初始化WebSocket
    init_websocket();
    std::cout << "初始化 WebSocket 成功" << std::endl;

    // 5. 设置初始状态
    set_device_state(DeviceState::Idle);
    send_device_state();

    // 6. 保持运行
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}