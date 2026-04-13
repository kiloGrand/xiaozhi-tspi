#ifndef XIAOZHI_CONTROL_H
#define XIAOZHI_CONTROL_H

#include <string>
#include <atomic>
#include <memory>
#include "ws_client.h"
#include "udp_endpoint.h"
#include "http.h"

// 常量定义
#define AUDIO_PORT_UP    5676   /* sound_app向control_center的这个端口上传音频 */
#define AUDIO_PORT_DOWN  5677   /* control_center向sound_app的这个端口下发音频 */
#define UI_PORT_UP    5678      /* GUI向control_center的这个端口上传UI信息 */
#define UI_PORT_DOWN  5679      /* control_center向GUI的这个端口下发UI信息 */
#define CFG_FILE "./xiaozhi.cfg"

// 枚举类型（原全局枚举）
enum class ListeningMode {
    AutoStop,
    ManualStop,
    AlwaysOn
};

enum class DeviceState {
    Unknown,
    Starting,
    WifiConfiguring,
    Idle,
    Connecting,
    Listening,
    Speaking,
    Upgrading,
    Activating,
    FatalError
};

class XiaozhiControlCenter {
public:
    // 单例模式（适合中控程序）
    static XiaozhiControlCenter& instance();

    // 禁用拷贝/移动
    XiaozhiControlCenter(const XiaozhiControlCenter&) = delete;
    XiaozhiControlCenter& operator=(const XiaozhiControlCenter&) = delete;
    XiaozhiControlCenter(XiaozhiControlCenter&&) = delete;
    XiaozhiControlCenter& operator=(XiaozhiControlCenter&&) = delete;

    // 主入口：启动整个小智中控
    void run();

private:
    // 私有构造（单例）
    XiaozhiControlCenter();
    ~XiaozhiControlCenter();

    // ==================== 初始化 ====================
    void init_mac();
    void init_uuid();
    void init_udp_endpoints();
    void init_websocket();
    void device_activate();

    // ==================== 设备状态 ====================
    void set_device_state(DeviceState state);
    void send_device_state();
    void send_stt(const std::string& text);

    // ==================== 业务回调 ====================
    // UDP音频上传回调
    int on_audio_uploaded(char* buffer, size_t size);
    // UDP UI数据回调
    int on_ui_data(char* buffer, size_t size);
    // WebSocket二进制回调(音频下发)
    void on_audio_downloaded(const char* buffer, size_t size);
    // WebSocket文本回调
    void on_text_downloaded(const char* buffer, size_t size);
    // WebSocket关闭回调
    void on_websocket_closed(short close_code);

    // ==================== 协议处理 ====================
    void send_start_listening_req(ListeningMode mode);
    void process_hello_json(const char* buffer, size_t size);
    void process_other_json(const char* buffer, size_t size);

private:
    // 硬件标识
    std::string mac_;
    std::string uuid_;

    // 通信组件
    std::unique_ptr<UdpEndpoint> audio_ep_;  // 音频UDP
    std::unique_ptr<UdpEndpoint> ui_ep_;     // UI UDP
    std::unique_ptr<WebSocketClient> ws_client_; // WebSocket客户端

    // 运行状态
    std::atomic<bool> audio_upload_enable_{true};
    std::atomic<DeviceState> device_state_{DeviceState::Unknown};
    std::string session_id_;
};

#endif // XIAOZHI_CONTROL_H