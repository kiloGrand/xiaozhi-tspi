#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <string>
#include <functional>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <atomic>

// 现代 C++ 回调类型（替代 C 语言函数指针）
using RecvCallback = std::function<void(const char* buffer, size_t size)>;
using CloseCallback = std::function<void(short close_code)>;

// WebSocket 客户端类（全封装、无全局变量、面向对象）
class WebSocketClient {
public:
    /**
     * @brief 构造函数
     * @param host 服务器主机名
     * @param port 端口
     * @param path 路径
     * @param hello_msg 握手消息
     * @param headers HTTP 头(JSON字符串)
     */
    WebSocketClient(
        std::string host,
        std::string port,
        std::string path,
        std::string hello_msg,
        std::string headers
    );

    // 析构函数：自动释放资源、断开连接、停止线程
    ~WebSocketClient();

    // 禁用拷贝（套接字/线程不可拷贝）
    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

    /**
     * @brief 设置回调
     * @param bin_cb 二进制数据回调
     * @param txt_cb 文本数据回调
     * @param close_cb 连接关闭回调
     */
    void set_callbacks(
        RecvCallback bin_cb,
        RecvCallback txt_cb,
        CloseCallback close_cb = nullptr
    );

    /**
     * @brief 启动客户端（后台线程运行）
     */
    void start();

    /**
     * @brief 发送二进制数据
     */
    int send_binary(const char* data, int size);

    /**
     * @brief 发送文本数据
     */
    int send_text(const char* data, int size);

    /**
     * @brief 获取连接状态
     */
    bool is_connected() const;

private:
    // 类型别名
    using Client = websocketpp::client<websocketpp::config::asio_tls_client>;
    using ConnectionHandle = websocketpp::connection_hdl;
    using ContextPtr = websocketpp::lib::shared_ptr<boost::asio::ssl::context>;

    // ========== 私有成员函数（原全局静态函数） ==========
    void on_message(ConnectionHandle hdl, Client::message_ptr msg);
    void on_open(ConnectionHandle hdl);
    void on_close(ConnectionHandle hdl);
    ContextPtr on_tls_init(ConnectionHandle hdl);
    bool verify_certificate(bool preverified, boost::asio::ssl::verify_context& ctx);
    bool verify_common_name(const char* hostname, X509* cert);
    bool verify_subject_alternative_name(const char* hostname, X509* cert);

    int connect();
    void run_thread();

private:
    // ========== 所有原全局变量 → 类私有成员（封装） ==========
    Client* m_client;                // WebSocket 客户端实例
    ConnectionHandle m_hdl;          // 连接句柄

    // 连接配置
    std::string m_host;
    std::string m_port;
    std::string m_path;
    std::string m_hello_msg;
    std::string m_headers;

    // 回调函数
    RecvCallback m_bin_cb;
    RecvCallback m_txt_cb;
    CloseCallback m_close_cb;

    // 连接状态
    std::atomic<bool> m_is_connected{false};
    std::atomic<bool> m_is_shaked{false};
    std::atomic<bool> m_running{true};
};

#endif