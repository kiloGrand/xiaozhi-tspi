#include "udp_endpoint.h"

#include <stdexcept>
#include <system_error>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>

/**
 * @brief 构造函数实现
 * @param listen_port 本地接收监听端口
 * @param send_target_port 远程发送目标端口
 * @param ip 目标IP地址
 */
UdpEndpoint::UdpEndpoint(int listen_port, int send_target_port, const std::string& ip)
    : listen_port_(listen_port), send_target_port_(send_target_port), ip_(ip)
{
    // 初始化UDP收发双套接字
    initSocket();
    // // 启动后台接收线程
    // startRecvThread();
}

/**
 * @brief 析构函数实现：释放所有资源
 */
UdpEndpoint::~UdpEndpoint() noexcept {
    // 停止接收线程循环
    stopRecvThread();

    // 安全关闭接收套接字
    if (sock_recv_ >= 0) {
        close(sock_recv_);
        sock_recv_ = -1;
    }
    // 安全关闭发送套接字
    if (sock_send_ >= 0) {
        close(sock_send_);
        sock_send_ = -1;
    }
}

/**
 * @brief 发送数据实现
 */
int UdpEndpoint::send(const uint8_t* data, size_t len) {
    // 校验输入参数合法性
    if (!data || len == 0) {
        throw std::invalid_argument("send: invalid buffer");
    }

    // 系统调用：向发送目标地址发送UDP数据
    ssize_t ret = sendto(sock_send_, data, len, 0,
                         (struct sockaddr*)&send_target_addr_,
                         sizeof(send_target_addr_));

    // 发送失败抛出系统异常
    if (ret < 0) {
        throw std::system_error(errno, std::generic_category(),
                               "sendto failed");
    }

    // 校验数据是否完整发送
    if ((size_t)ret != len) {
        throw std::runtime_error("partial send");
    }

    return 0;
}

/**
 * @brief 接收数据实现（非阻塞模式）
 */
int UdpEndpoint::recv(uint8_t* buffer, size_t maxlen) {
    // 非阻塞接收UDP数据
    ssize_t ret = recvfrom(sock_recv_, buffer, maxlen, 0, nullptr, nullptr);

    if (ret < 0) {
        // 无数据可用 / 被信号中断，属于正常情况，返回0
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return 0;
        }

        // 其他异常，抛出错误
        throw std::system_error(errno, std::generic_category(),
                               "recvfrom failed");
    }

    // 返回实际接收到的字节数
    return static_cast<int>(ret);
}

/**
 * @brief 初始化发送和接收双套接字
 */
void UdpEndpoint::initSocket() {
    // ========== 初始化发送专用Socket ==========
    sock_send_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_send_ < 0) {
        throw std::system_error(errno, std::generic_category(),
                               "socket(send) failed");
    }

    // 配置发送目标网络地址（仅用于发送数据）
    memset(&send_target_addr_, 0, sizeof(send_target_addr_));
    send_target_addr_.sin_family = AF_INET;
    send_target_addr_.sin_port = htons(send_target_port_);

    // IP地址格式转换
    if (inet_pton(AF_INET, ip_.c_str(), &send_target_addr_.sin_addr) <= 0) {
        throw std::runtime_error("inet_pton remote failed");
    }

    // ========== 初始化接收专用Socket ==========
    sock_recv_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_recv_ < 0) {
        throw std::system_error(errno, std::generic_category(),
                               "socket(recv) failed");
    }

    // 设置接收Socket为非阻塞模式
    int flags = fcntl(sock_recv_, F_GETFL, 0);
    fcntl(sock_recv_, F_SETFL, flags | O_NONBLOCK);

    // 配置本地监听地址（接收用）
    sockaddr_in local_addr{};
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(listen_port_);

    if (inet_pton(AF_INET, ip_.c_str(), &local_addr.sin_addr) <= 0) {
        throw std::runtime_error("inet_pton local failed");
    }

    // 绑定本地监听端口
    if (bind(sock_recv_, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        throw std::system_error(errno, std::generic_category(),
                               "bind failed");
    }
}

/**
 * @brief 启动后台接收线程，循环读取数据并触发回调
 */
void UdpEndpoint::startRecvThread() {
    // 安全防护：线程已启动则直接返回，避免重复创建
    if (recv_thread_.joinable()) {
        return;
    }

    running_ = true;

    recv_thread_ = std::thread([this]() {
        // 接收数据缓冲区（2KB）
        uint8_t buffer[2048];

        // 线程主循环
        while (running_) {
            try {
                // 非阻塞读取数据
                int len = this->recv(buffer, sizeof(buffer));

                // 收到有效数据，触发上层回调
                if (len > 0) {
                    if (recv_callback_) {
                        recv_callback_(std::vector<uint8_t>(buffer, buffer + len));
                    }
                } else {
                    // 无数据时短暂休眠，降低CPU占用
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

            } catch (const std::exception& e) {
                // 线程内捕获异常，防止线程崩溃
                std::cerr << "[Recv Thread Error] " << e.what() << std::endl;
            }
        }
    });
}

void UdpEndpoint::stopRecvThread() {
    // 停止线程循环
    running_ = false;

    // 仅线程已启动时，等待退出
    if (recv_thread_.joinable()) {
        recv_thread_.join();
    }
}