#include <gtest/gtest.h>
#include "udp_endpoint.h"

#include <atomic>
#include <cstring>
#include <thread>
#include <ostream>


static std::atomic<bool> g_received(false);
static std::string g_msg;

// ===================== 测试用例 =====================
TEST(UdpIpcTest, SendAndReceive) {
    g_received = false;
    g_msg.clear();

    UdpEndpoint server(8888, 8889);

    server.setRecvCallback([](const std::vector<uint8_t>& data) {
        g_msg = std::string(data.begin(), data.end());
        g_received = true;
    });

    UdpEndpoint client(8889, 8888);

    const char* msg = "Hello IPC UDP (gtest)";
    client.send(reinterpret_cast<const uint8_t*>(msg), strlen(msg));

    // 等待接收（最多2秒）
    for (int i = 0; i < 20; i++) {
        if (g_received) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    EXPECT_TRUE(g_received) << "未接收到数据";
    EXPECT_EQ(g_msg, msg) << "发送与接收的不一致！" 
        << "发送：" << msg 
        << " 接收：" << g_msg;
}
