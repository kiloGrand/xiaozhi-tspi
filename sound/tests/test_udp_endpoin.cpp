#include <gtest/gtest.h>
#include "udp_endpoint.h"

#include <atomic>
#include <cstring>
#include <thread>
#include <string>
#include <vector>

// 全局标志：标记服务端已收到并回发消息
static std::atomic<bool> g_server_replied(false);

// ===================== 测试用例 =====================
// 服务端：回调接收 + 自动回发数据 | 客户端：无回调，主动收发
TEST(UdpIpcTest, ServerCallbackEchoToClient) {
    g_server_replied = false;

    // ========== 服务端：监听8888端口，目标端口8889（客户端端口） ==========
    UdpEndpoint server(8888, 8889);
    // 服务端设置回调：接收数据后，立即回发给客户端
    server.setRecvCallback([&server](const std::vector<uint8_t>& data) {
        // 核心逻辑：服务端收到数据，直接回传给客户端
        server.send(data.data(), data.size());
        g_server_replied = true;
    });
    // 启动服务端接收线程
    server.startRecvThread();

    // ========== 客户端：监听8889端口，目标端口8888（服务端端口） ==========
    // 客户端【无任何回调】，纯主动收发
    UdpEndpoint client(8889, 8888);

    // 1. 客户端主动发送测试消息
    const char* send_msg = "UDP Echo Test: Server Callback Reply";
    size_t msg_len = strlen(send_msg);
    client.send(reinterpret_cast<const uint8_t*>(send_msg), msg_len);

    // 等待服务端接收并回发
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 2. 客户端【主动调用recv接收回传数据】（无回调）
    uint8_t recv_buffer[1024] = {0};
    int recv_len = 0;
    // 循环接收，超时2秒
    for (int i = 0; i < 20; ++i) {
        recv_len = client.recv(recv_buffer, sizeof(recv_buffer));
        if (recv_len > 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 3. 结果验证
    EXPECT_TRUE(g_server_replied) << "服务端未收到消息";
    EXPECT_GT(recv_len, 0) << "客户端未收到服务端回传消息";
    EXPECT_EQ(memcmp(send_msg, recv_buffer, msg_len), 0) 
        << "发送数据与服务端回传数据不一致";
}