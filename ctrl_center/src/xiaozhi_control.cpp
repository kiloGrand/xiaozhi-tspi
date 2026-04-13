#include "xiaozhi_control.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <cstring>
#include "json.hpp"
#include "uuid.h"

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
}

// 析构函数
XiaozhiControlCenter::~XiaozhiControlCenter() = default;

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

// ==================== 初始化UDP端点 ====================
void XiaozhiControlCenter::init_udp_endpoints() {
    // 音频UDP端点
    audio_ep_ = std::make_unique<UdpEndpoint>(AUDIO_PORT_UP, AUDIO_PORT_DOWN);
    audio_ep_->setRecvCallback([this](const std::vector<uint8_t>& data) {
        this->on_audio_uploaded((char*)data.data(), data.size());
    });

    // UI UDP端点
    ui_ep_ = std::make_unique<UdpEndpoint>(UI_PORT_UP, UI_PORT_DOWN);
    ui_ep_->setRecvCallback([this](const std::vector<uint8_t>& data) {
        this->on_ui_data((char*)data.data(), data.size());
    });
}

// ==================== 设备状态管理 ====================
void XiaozhiControlCenter::set_device_state(DeviceState state) {
    device_state_ = state;
}

void XiaozhiControlCenter::send_device_state() {
    // 修复：先从原子变量加载值，再转换为int
    auto current_state = device_state_.load();
    std::string state_str = "{\"state\":" + std::to_string(static_cast<int>(current_state)) + "}";
    try
    {
        ui_ep_->send((const uint8_t*)state_str.data(), state_str.size());
    }
    catch(const std::exception& e)
    {
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

// ==================== UDP回调 ====================
int XiaozhiControlCenter::on_audio_uploaded(char* buffer, size_t size) {
    if (ws_client_ && audio_upload_enable_ && ws_client_->is_connected()) {
        static int cnt = 0;
        if ((cnt++ % 100) == 0) {
            std::cout << "Send opus to server: " << size << " count: " << cnt << std::endl;
        }
        ws_client_->send_binary(buffer, (int)size);
    }
    return 0;
}

int XiaozhiControlCenter::on_ui_data(char* buffer, size_t size) {
    std::cout << "UI command received, start websocket..." << std::endl;
    set_device_state(DeviceState::Listening);
    send_device_state();
    ws_client_->start();
    return 0;
}

// ==================== WebSocket回调 ====================
void XiaozhiControlCenter::on_audio_downloaded(const char* buffer, size_t size) {
    audio_ep_->send((const uint8_t*)buffer, size);
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

    try
    {
        ws_client_->send_text(startString.data(), (int)startString.size());
        std::cout << "Send: " << startString << std::endl;
    }
    catch(const websocketpp::lib::error_code& e)
    {
        std::cout << "Error sending message: " << e << " (" << e.message() << ")" << std::endl;
    }
}

// ==================== Hello协议处理 ====================
void XiaozhiControlCenter::process_hello_json(const char* buffer, size_t size) {
    json j = json::parse(buffer);
    int sample_rate = j["audio_params"]["sample_rate"];
    int channels = j["audio_params"]["channels"];
    std::cout << "Received valid 'hello' message with sample_rate: " << sample_rate << " and channels: " << channels << std::endl;     

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
        }
    } catch (const std::exception& e) {
        std::cout << "process json error: " << e.what() << std::endl;
    }
}

// ==================== 设备激活 ====================
void XiaozhiControlCenter::device_activate() {
    http_data_t http_data{};
    http_data.url = "https://api.tenclass.net/xiaozhi/ota/";

    // 替换 http_data.post 中的 uuid
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

    // 替换 http_data.headers 中的 Device-Id
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
            // 直接构造目标JSON字符串：{"type":"tts","text":"Active-Code: 数字"}
            std::string auth_code = R"({"type":"tts","text":"Active-Code: )" 
                                    + std::string(active_code) 
                                    + R"("})";
            set_device_state(DeviceState::Activating);
            send_device_state();
            // 发送给GUI聊天信息框显示
            send_stt(auth_code);
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

// ==================== 初始化WebSocket ====================
void XiaozhiControlCenter::init_websocket() {
    // WebSocket头JSON    
    std::ostringstream ws_header_ss;
    ws_header_ss << R"(
        {
            "Authorization": "Bearer test-token",
            "Protocol-Version": "1",
            "Device-Id": ")" << mac_ << R"(",
            "Client-Id": ")" << uuid_ << R"("
        }
    )";

    // hello
    std::string hello = R"(
        {
            "type": "hello",
            "version": 1,
            "transport": "websocket",
            "audio_params": {
                "format": "opus",
                "sample_rate": 16000,
                "channels": 1,
                "frame_duration": 60
            }
        })";
    
    // 创建WebSocket客户端
    ws_client_ = std::make_unique<WebSocketClient>(
        "api.tenclass.net",
        "443",
        "/xiaozhi/v1/",
        hello,
        ws_header_ss.str()
    );

    // 设置回调
    ws_client_->set_callbacks(
        [this](const char* buf, size_t sz) { this->on_audio_downloaded(buf, sz); },
        [this](const char* buf, size_t sz) { this->on_text_downloaded(buf, sz); },
        [this](short code) { this->on_websocket_closed(code); }
    );
}

// ==================== 主运行入口 ====================
void XiaozhiControlCenter::run() {
    // 1. 初始化UDP
    init_udp_endpoints();
    std::cout << "初始化 IPC UDP 成功" << std::endl;

    // 2. 设备激活
    device_activate();
    std::cout << "设备激活成功" << std::endl;

    // 3. 初始化WebSocket
    init_websocket();
    std::cout << "初始化 WebSocket 成功" << std::endl;

    // 4. 设置初始状态
    set_device_state(DeviceState::Idle);
    send_device_state();

    // 5. 保持运行
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}