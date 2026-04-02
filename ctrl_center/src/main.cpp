#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <dirent.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <string>

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

#include "json.hpp"
#include "websocket_client.h"
#include "http.h"
#include "ipc_udp.h"
#include "uuid.h"

#define AUDIO_PORT_UP    5676   /* sound_app向control_center的这个端口上传音频 */
#define AUDIO_PORT_DOWN  5677   /* control_center向sound_app的这个端口下发音频 */
#define UI_PORT_UP    5678      /* GUI向control_center的这个端口上传UI信息 */
#define UI_PORT_DOWN  5679      /* control_center向GUI的这个端口下发UI信息 */
// #define CFG_FILE "/home/grand/xiaozhi-desktop/cfg/xiaozhi.cfg"
#define CFG_FILE "./xiaozhi.cfg"

using json = nlohmann::json;
static int g_audio_upload_enable = 1;
static std::string g_session_id;

typedef enum ListeningMode {
    kListeningModeAutoStop,
    kListeningModeManualStop,
    kListeningModeAlwaysOn // 需要 AEC 支持
} ListeningMode;

// 定义设备状态枚举类型
typedef enum DeviceState {
    kDeviceStateUnknown,
    kDeviceStateStarting,
    kDeviceStateWifiConfiguring,
    kDeviceStateIdle,
    kDeviceStateConnecting,
    kDeviceStateListening,
    kDeviceStateSpeaking,
    kDeviceStateUpgrading,
    kDeviceStateActivating,
    kDeviceStateFatalError
} DeviceState;

static p_ipc_endpoint_t g_ipc_ep_audio;
static p_ipc_endpoint_t g_ipc_ep_ui;
static DeviceState g_device_state = kDeviceStateUnknown;

static void set_device_state(DeviceState state)
{
    g_device_state = state;
}

static void send_device_state(void)
{
    std::string stateString = "{\"state\":" + std::to_string(g_device_state) + "}";
    g_ipc_ep_ui->send(g_ipc_ep_ui, stateString.data(), stateString.size());
}

static void send_stt(const std::string& text)
{
    if (!g_ipc_ep_ui) {
        std::cerr << "Error: g_ipc_ep_ui is nullptr" << std::endl;
        return;
    }

    try {
        // json j;
        // j["text"] = text;
        // std::string textString = j.dump();
        // g_ipc_ep_ui->send(g_ipc_ep_ui, textString.data(), textString.size());
        // 直接发送原始text字符串
        g_ipc_ep_ui->send(g_ipc_ep_ui, text.data(), text.size());
    } catch (const std::exception& e) {
        std::cerr << "Error creating JSON string: " << e.what() << std::endl;
    }
}

static void process_opus_data_downloaded(const char *buffer, size_t size)
{
#if 0    
    std::cout << "Received opus data: " << size << " bytes" << std::endl;
    static int file_number = 1;
    // 构造文件名
    char filename[20];
    snprintf(filename, sizeof(filename), "test%03d.opus", file_number);

    // 打开文件
    FILE *file = fopen(filename, "wb");
    if (file) {
        // 写入Opus数据
        fwrite(buffer, 1, size, file);
        fclose(file);
        file_number++; // 增加文件编号
    } else {
        fprintf(stderr, "Failed to open file %s for writing\n", filename);
    }     
#endif    
    g_ipc_ep_audio->send(g_ipc_ep_audio, buffer, size);
}

static void send_start_listening_req(ListeningMode mode)
{
    std::string startString = "{\"session_id\":\"" + g_session_id + "\"";

    startString += ",\"type\":\"listen\",\"state\":\"start\"";

    if (mode == kListeningModeAutoStop) {
        startString += ",\"mode\":\"auto\"}";
    } else if (mode == kListeningModeManualStop) {
        startString += ",\"mode\":\"manual\"}";
    } else if (mode == kListeningModeAlwaysOn) {
        startString += ",\"mode\":\"realtime\"}";
    }

    try {
        //c->send(hdl, startString, websocketpp::frame::opcode::text);
        websocket_send_text(startString.data(), startString.size());
        std::cout << "Send: " << startString << std::endl;    
    } catch (const websocketpp::lib::error_code& e) {
        std::cout << "Error sending message: " << e << " (" << e.message() << ")" << std::endl;
    }     
}

static void process_hello_json(const char *buffer, size_t size)
{
    json j = json::parse(buffer);
    int sample_rate = j["audio_params"]["sample_rate"];
    int channels = j["audio_params"]["channels"];
    std::cout << "Received valid 'hello' message with sample_rate: " << sample_rate << " and channels: " << channels << std::endl;     

    g_session_id = j["session_id"];

    std::string desc = R"(
    {"session_id":"","type":"iot","update":true,"descriptors":[{"name":"Speaker","description":"扬声器","properties":{"volume":{"description":"当前音量值","type":"number"}},"methods":{"SetVolume":{"description":"设置音量","parameters":{"volume":{"description":"0到100之间的整数","type":"number"}}}}}]}
    )";

    // Send the new message              
    try {
        //c->send(hdl, desc, websocketpp::frame::opcode::text);
        websocket_send_text(desc.data(), desc.size());
        std::cout << "Send: " << desc << std::endl;    
    } catch (const websocketpp::lib::error_code& e) {
        std::cout << "Error sending message: " << e << " (" << e.message() << ")" << std::endl;
    }

    std::string desc2 = R"(
    {"session_id":"","type":"iot","update":true,"descriptors":[{"name":"Backlight","description":"屏幕背光","properties":{"brightness":{"description":"当前亮度百分比","type":"number"}},"methods":{"SetBrightness":{"description":"设置亮度","parameters":{"brightness":{"description":"0到100之间的整数","type":"number"}}}}}]}
)";

    // Send the new message
      
    try {
        //c->send(hdl, desc2, websocketpp::frame::opcode::text);
        websocket_send_text(desc2.data(), desc2.size());
        std::cout << "Send: " << desc2 << std::endl;    
    } catch (const websocketpp::lib::error_code& e) {
        std::cout << "Error sending message: " << e << " (" << e.message() << ")" << std::endl;
    }	

    std::string desc3 = R"(
    {"session_id":"","type":"iot","update":true,"descriptors":[{"name":"Battery","description":"电池管理","properties":{"level":{"description":"当前电量百分比","type":"number"},"charging":{"description":"是否充电中","type":"boolean"}},"methods":{}}]}
)";

    // Send the new message
      
    try {
        //c->send(hdl, desc3, websocketpp::frame::opcode::text);
        websocket_send_text(desc3.data(), desc3.size());
        std::cout << "Send: " << desc3 << std::endl;    
    } catch (const websocketpp::lib::error_code& e) {
        std::cout << "Error sending message: " << e << " (" << e.message() << ")" << std::endl;
    }			

    std::string startString = R"(
        {"session_id":"","type":"listen","state":"start","mode":"auto"}
    )";
    
    try {
        //c->send(hdl, startString, websocketpp::frame::opcode::text);
        websocket_send_text(startString.data(), startString.size());
        std::cout << "Send: " << startString << std::endl;    
    } catch (const websocketpp::lib::error_code& e) {
        std::cout << "Error sending message: " << e << " (" << e.message() << ")" << std::endl;
    }            

    std::string state = R"(
        {"session_id":"","type":"iot","update":true,"states":[{"name":"Speaker","state":{"volume":80}},{"name":"Backlight","state":{"brightness":75}},{"name":"Battery","state":{"level":0,"charging":false}}]}
    )";
    
    g_audio_upload_enable = 1;

    try {
        //c->send(hdl, state, websocketpp::frame::opcode::text);
        websocket_send_text(state.data(), state.size());
        std::cout << "Send: " << state << std::endl;    
    } catch (const websocketpp::lib::error_code& e) {
        std::cout << "Error sending message: " << e << " (" << e.message() << ")" << std::endl;
    }
}

static void process_other_json(const char *buffer, size_t size)
{
    try {
        // Parse JSON data
        json j = json::parse(buffer);
        
        if (!j.contains("type"))
            return;
        
        if (j["type"] == "tts") {
            auto state = j["state"];
            if (state == "start") {
                // 下发语音, 可以关闭录音
                g_audio_upload_enable = 0;
                set_device_state(kDeviceStateListening);
                send_device_state();
            } else if (state == "stop") {
                // 本次交互结束, 可以继续上传声音
                // 等待一会以免她听到自己的话误以为再次对话
                sleep(2);
                send_start_listening_req(kListeningModeAutoStop);
                set_device_state(kDeviceStateListening);
                send_device_state();
                g_audio_upload_enable = 1;
            } else if (state == "sentence_start") {
                // 取出"text", 通知GUI
                // {"type":"tts","state":"sentence_start","text":"1加1等于2啦~","session_id":"eae53ada"}
                // auto text = j["text"];
                // send_stt(text.get<std::string>());
                std::string textString = j.dump();
                send_stt(textString);
                send_start_listening_req(kListeningModeAutoStop);
                set_device_state(kDeviceStateSpeaking);
                send_device_state();
            }
        } else if (j["type"] == "stt") {
            // 表示服务器端识别到了用户语音, 取出"text", 通知GUI
            // auto text = j["text"];
            // send_stt(text.get<std::string>());
            std::string textString = j.dump();
            send_stt(textString);
        } else if (j["type"] == "llm") {
            // {"type":"llm","text":"😆","emotion":"laughing","session_id":"5e22d559"}
        /*
            "neutral",
            "happy",
            "laughing",
            "funny",
            "sad",
            "angry",
            "crying",
            "loving",
            "embarrassed",
            "surprised",
            "shocked",
            "thinking",
            "winking",
            "cool",
            "relaxed",
            "delicious",
            "kissy",
            "confident",
            "sleepy",
            "silly",
            "confused"
        */          
            std::string textString = j.dump();
            send_stt(textString);
        } else if (j["type"] == "iot") {
            
        }
    } catch (json::parse_error& e) {
        std::cout << "Failed to parse JSON message: " << e.what() << std::endl;
    } catch (std::exception& e) {
        std::cout << "Error processing message: " << e.what() << std::endl;
    }
}

static void process_txt_data_downloaded(const char *buffer, size_t size)
{
    try {
        // Parse the JSON message
        json j = json::parse(buffer);

        // Check if the message matches the expected structure
        if (j.contains("type") && j["type"] == "hello") {
            process_hello_json(buffer, size);
        } else {
            process_other_json(buffer, size);
        }
         
    } catch (json::parse_error& e) {
        std::cout << "Failed to parse JSON message: " << e.what() << std::endl;
    } catch (std::exception& e) {
        std::cout << "Error processing message: " << e.what() << std::endl;
    }
}

int process_opus_data_uploaded(char *buffer, size_t size, void *user_data)
{
#if 0    
    static int file_number = 1;
    // 构造文件名
    char filename[20];
    snprintf(filename, sizeof(filename), "test%03d.opus", file_number);

    // 打开文件
    FILE *file = fopen(filename, "wb");
    if (file) {
        // 写入Opus数据
        fwrite(buffer, 1, size, file);
        fclose(file);
        file_number++; // 增加文件编号
    } else {
        fprintf(stderr, "Failed to open file %s for writing\n", filename);
    }   
#endif
    if (g_audio_upload_enable  && websocket_is_connected()) {
        static int cnt = 0;
        if ((cnt++ % 100) == 0)
            std::cout << "Send opus data to server: " << size <<" count: "<< cnt << std::endl;
        websocket_send_binary(buffer, size);
    }
    return 0;
}

// 接受 GUI->control_center 的数据
int process_ui_data(char *buffer, size_t size, void *user_data)
{
    std::cout << "websocket start, connect ai-xiaozhi" << std::endl;
    
    set_device_state(kDeviceStateListening);
    send_device_state();
    websocket_start();
    return 0;
}

static void on_websocket_closed(short close_code)
{
    std::cout << "【主程序】连接关闭：code=" << close_code << std::endl;

    // 1. 停止音频上传（解决 invalid state 报错）
    g_audio_upload_enable = 0;

    // 2. 判断：服务器长时间不说话 → 主动关闭（码 1005）
    if (close_code == 1005) {
        std::cout << "【判定】长时间未说话，服务器主动超时关闭连接！" << std::endl;
    }

    // 3. 更新设备状态
    set_device_state(kDeviceStateIdle);
    send_device_state();
}

/*******************************************************核心逻辑****************************************************************
    sound_app 采集音频（Opus）→ UDP 5676 → control_center → 触发 process_opus_data_uploaded → 调用 websocket_send_binary 上传云端
    ↓
    云端小智：STT识别 → LLM思考 → TTS合成 → WebSocket下发JSON文本+Opus音频
    ↓
    control_center 接收：
    1. JSON文本 → 触发 process_txt_data_downloaded → 分发到 process_hello_json/process_other_json → 调用 send_stt/set_device_state 等更新GUI/设备状态；
    2. Opus音频 → 触发 process_opus_data_downloaded → 调用 g_ipc_ep_audio->send → UDP 5677 → sound_app 播放音频
    ↓
    交互收尾：调用 send_start_listening_req 恢复监听，g_audio_upload_enable=1 等待下一次交互
*******************************************************************************************************************************/
int main(int argc, char **argv)
{
    char active_code[20] = "";
    std::string cfg_file = CFG_FILE;

    // 获取无线网卡的 MAC 地址
    std::string mac = get_wireless_mac_address();
    if (mac.empty()) {
        std::cerr << "Failed to get wireless MAC address" << std::endl;
        mac = "00:00:00:00:00:00"; // 默认值，可以根据需要修改
    }

    // 读取配置文件中的 UUID
    std::string uuid = read_uuid_from_config(cfg_file);
    if (uuid.empty()) {
        std::cerr << "UUID not found in " CFG_FILE << std::endl;
        // 生成新的 UUID
        uuid = generate_uuid();
        std::cout << "Generated new UUID: " << uuid << std::endl;

        // 将新的 UUID 写入配置文件
        if (!write_uuid_to_config(uuid, cfg_file)) {
            std::cerr << "Failed to write UUID to " CFG_FILE << std::endl;
        } else {
            std::cout << "UUID written to " CFG_FILE << std::endl;
        }
    } else {
        std::cout << "UUID from " CFG_FILE ": " << uuid << std::endl;
    }    

    g_ipc_ep_audio = ipc_endpoint_create_udp(AUDIO_PORT_UP, AUDIO_PORT_DOWN, process_opus_data_uploaded, NULL);
    g_ipc_ep_ui = ipc_endpoint_create_udp(UI_PORT_UP, UI_PORT_DOWN, process_ui_data, NULL);

    http_data_t http_data;
    http_data.url = "https://api.tenclass.net/xiaozhi/ota/";

    // 替换 http_data.post 中的 uuid
    std::ostringstream post_stream;
    post_stream << R"(
        {
            "uuid":")" << uuid << R"(",
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
            "Device-Id": ")" << mac << R"(",
            "User-Agent": "weidongshan1",
            "Accept-Language": "zh-CN"
        }
    )";
    http_data.headers = headers_stream.str();

    while (0 != active_device(&http_data, active_code)) {
        if (active_code[0]) {
            std::string auth_code = "Active-Code: " + std::string(active_code);
            set_device_state(kDeviceStateActivating);
            send_device_state();
            send_stt(auth_code);
        }
        sleep(5);
    }

    set_device_state(kDeviceStateIdle);
    send_device_state();
    send_stt("设备已经激活");

    websocket_data_t ws_data;

    // 替换 ws_data.headers 中的 Device-Id 和 Client-Id
    std::ostringstream ws_headers_stream;
    ws_headers_stream << R"(
        {
            "Authorization": "Bearer test-token",
            "Protocol-Version": "1",
            "Device-Id": ")" << mac << R"(",
            "Client-Id": ")" << uuid << R"("
        }
    )";
    ws_data.headers = ws_headers_stream.str();

    ws_data.hello = R"(
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

    ws_data.hostname = "api.tenclass.net";
    ws_data.port = "443";
    ws_data.path = "/xiaozhi/v1/";    

    websocket_set_callbacks(
        process_opus_data_downloaded, 
        process_txt_data_downloaded, 
        on_websocket_closed,
        &ws_data);
    
    set_device_state(kDeviceStateListening);
    send_device_state();
    websocket_start();

    while (1)
    {
        sleep(1);
    }
}
