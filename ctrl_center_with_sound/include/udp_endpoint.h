#ifndef UDP_ENDPOINT_H
#define UDP_ENDPOINT_H

#include "ipc_endpoint.h"
#include <string>
#include <thread>
#include <atomic>
#include <netinet/in.h>

/**
 * @class UdpEndpoint
 * @brief UDP协议实现的IPC通信端点类
 * @details 【架构设计：双Socket收发分离】
 *          本类采用 发送专用Socket + 接收专用Socket 的分离架构设计：
 *          1. sock_send_：仅负责数据发送，无需绑定固定端口
 *          2. sock_recv_：仅负责数据接收，必须绑定本地监听端口
 *
 *          【分离设计优势】
 *          1. 职责解耦：收发功能完全独立，逻辑更清晰
 *          2. 配置隔离：接收端可设置非阻塞模式，不影响发送端行为
 *          3. 线程安全：接收线程仅操作接收Socket，主线程仅操作发送Socket
 *          4. 稳定性高：避免单Socket收发互相干扰，提升通信可靠性
 */
class UdpEndpoint : public IpcEndpoint {
public:
    /**
     * @brief 构造函数
     * @param listen_port 本地接收监听端口（接收Socket专用绑定端口）
     * @param send_target_port 远程发送目标端口（发送Socket专用目标端口）
     * @param ip 远程目标IP地址，默认本地回环127.0.0.1
     */
    UdpEndpoint(int listen_port, int send_target_port, const std::string& ip = "127.0.0.1");

    /**
     * @brief 析构函数
     * @details 线程安全释放资源，关闭套接字、停止接收线程
     */
    ~UdpEndpoint() noexcept;

    /**
     * @brief 重写：发送数据接口
     * @param data 待发送数据指针
     * @param len 数据长度
     * @return 成功返回0，失败抛出异常
     */
    int send(const uint8_t* data, size_t len) override;

    /**
     * @brief 重写：接收数据接口
     * @param buffer 接收缓冲区
     * @param maxlen 缓冲区最大长度
     * @return 实际接收字节数，无数据返回0，失败抛出异常
     */
    int recv(uint8_t* buffer, size_t maxlen) override;

private:
    /**
     * @brief 初始化UDP双套接字
     * @details 分别创建发送/接收套接字，配置目标地址、本地绑定、非阻塞模式
     */
    void initSocket();

    /**
     * @brief 启动后台接收线程
     * @details 独立线程循环接收数据，收到数据后触发上层回调函数
     */
    void startRecvThread();

private:
    // 发送专用套接字：仅用于发送UDP数据
    int sock_send_ = -1;
    // 接收专用套接字：仅用于接收UDP数据（已绑定监听端口+非阻塞模式）
    int sock_recv_ = -1;

    int listen_port_;               // 本地接收监听端口（绑定接收Socket）
    int send_target_port_;          // 远程发送目标端口（发送Socket的目标端口）
    std::string ip_;                // 远程目标IP地址

    // 发送目标网络地址：仅用于发送Socket，存储目标IP+端口信息
    struct sockaddr_in send_target_addr_{};

    std::thread recv_thread_;           // 后台数据接收线程
    std::atomic<bool> running_{true};   // 线程运行控制标志（线程安全）
};

#endif