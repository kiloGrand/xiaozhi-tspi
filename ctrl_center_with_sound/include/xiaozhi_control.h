#ifndef XIAOZHI_CONTROL_H
#define XIAOZHI_CONTROL_H

#include <string>
#include <atomic>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "ws_client.h"
#include "udp_endpoint.h"
#include "http.h"
#include "json.hpp"
#include "AlsaCapture.hpp"
#include "AlsaPlayback.hpp"
#include "OpusWrapper.hpp"

// 常量定义
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

using json = nlohmann::json;

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
    void init_audio();
    void init_ui_udp();
    void init_websocket();
    void device_activate();

    // ==================== 设备状态 ====================
    void set_device_state(DeviceState state);
    void send_device_state();
    void send_stt(const std::string& text);

    // ==================== 业务回调 ====================
    // WebSocket二进制回调(音频下发)
    void on_audio_downloaded(const char* buffer, size_t size);
    // WebSocket文本回调
    void on_text_downloaded(const char* buffer, size_t size);
    // WebSocket关闭回调
    void on_websocket_closed(short close_code);
    // UI UDP数据回调
    int on_ui_data(char* buffer, size_t size);

    // ==================== 协议处理 ====================
    void send_start_listening_req(ListeningMode mode);
    void process_hello_json(const char* buffer, size_t size);
    void process_other_json(const char* buffer, size_t size);

    // MCP核心功能
    void handle_mcp_request(const json& mcp_json);

private:
    // 硬件标识
    std::string mac_;
    std::string uuid_;

    // 通信组件
    std::unique_ptr<UdpEndpoint> ui_ep_;     // UI UDP（保留，qt_gui独立进程）
    std::unique_ptr<WebSocketClient> ws_client_; // WebSocket客户端

    // 音频组件（替代原audio_ep_的UDP通信）
    std::unique_ptr<AlsaAudio::AlsaCapture> capture_;
    std::unique_ptr<AlsaAudio::AlsaPlayback> player_;
    std::unique_ptr<OpusEncoder> encoder_;
    std::unique_ptr<OpusDecoder> decoder_;

    // 下行队列：云端 → 播放（生产者-消费者，有界队列）
    std::queue<std::vector<uint8_t>> opus_downlink_queue_;
    std::mutex opus_downlink_mutex_;
    std::condition_variable opus_downlink_cv_;
    static constexpr size_t kMaxDownlinkQueueSize = 100;

    // 音频缓冲
    std::vector<uint8_t> record_buffer_;
    size_t record_offset_{0};
    std::vector<uint8_t> play_buffer_;
    size_t play_offset_{0};

    // 音频常数
    static constexpr uint32_t kSampleRateRec = 16000;
    static constexpr uint32_t kSampleRatePlay = 24000;
    static constexpr uint32_t kChannels = 1;
    static constexpr uint32_t kOpusFrameMs = 60;
    static constexpr size_t kOpusBufferSize = 1024 * 5;
    static constexpr size_t kPcmBufferSize = 1024 * 30;

    // 运行状态
    std::atomic<bool> audio_upload_enable_{false};
    std::atomic<DeviceState> device_state_{DeviceState::Unknown};
    std::string session_id_;
};

#endif // XIAOZHI_CONTROL_H