#include <gtest/gtest.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <atomic>
#include "ipc_udp.h"

// 测试用端口（避免冲突，与实现中127.0.0.1适配）
#define TEST_SERVER_PORT  8888  // 服务端本地端口（接收）
#define TEST_CLIENT_PORT  8889  // 客户端本地端口（发送）

// 原子变量：线程安全存储接收结果（避免竞态）
std::atomic<bool> g_cb_triggered(false);
std::atomic<int>  g_recv_len(0);
char g_recv_buffer[2048] = {0}; // 匹配实现中的buffer大小（2048）

/**
 * 服务端回调函数：适配ipc_udp.cpp的回调触发逻辑
 * （实现中收到数据后会自动调用该回调，无需手动recv）
 */
int udp_server_callback(char *buffer, size_t size, void *user_data) {
    if (buffer == nullptr || size <= 0) {
        return -1;
    }

    // 存储接收数据（原子操作+内存拷贝）
    g_recv_len = size;
    memset(g_recv_buffer, 0, sizeof(g_recv_buffer));
    memcpy(g_recv_buffer, buffer, size > sizeof(g_recv_buffer) ? sizeof(g_recv_buffer) : size);
    g_cb_triggered = true;

    // 打印接收数据（直观展示）
    std::cout << "\n[服务端回调触发] 收到数据：" << std::endl;
    std::cout << "  → 数据长度：" << g_recv_len << " 字节" << std::endl;
    std::cout << "  → 数据内容：" << g_recv_buffer << std::endl;

    return 0;
}

// 核心测试用例：适配实际ipc_udp.cpp的UDP收发逻辑
TEST(IpcUdpTest, SendAndRecvData) {
    // ===================== 1. 初始化测试数据 =====================
    const char* test_data = "Hello IPC UDP! 100ask Test Data: 20250320";
    int test_data_len = strlen(test_data);
    std::cout << "[测试初始化] 准备发送数据：" << std::endl;
    std::cout << "  → 数据长度：" << test_data_len << " 字节" << std::endl;
    std::cout << "  → 数据内容：" << test_data << std::endl;

    // 重置全局接收变量
    g_cb_triggered = false;
    g_recv_len = 0;
    memset(g_recv_buffer, 0, sizeof(g_recv_buffer));

    // ===================== 2. 创建服务端端点（自动启动接收线程） =====================
    // 关键：传入回调函数后，ipc_udp.cpp会自动创建handle_udp_connection线程接收数据
    p_ipc_endpoint_t server_ep = ipc_endpoint_create_udp(
        TEST_SERVER_PORT,  // 服务端本地端口（绑定接收）
        TEST_CLIENT_PORT,  // 服务端发送目标端口（客户端端口，测试中未使用）
        udp_server_callback, // 回调函数（核心：收到数据自动触发）
        nullptr             // 用户数据（测试中无需传递）
    );
    ASSERT_NE(server_ep, nullptr) << "服务端端点创建失败！检查端口是否被占用";
    std::cout << "[服务端] 端点创建成功，已启动接收线程（监听127.0.0.1:" << TEST_SERVER_PORT << "）" << std::endl;

    // ===================== 3. 创建客户端端点（用于发送数据） =====================
    p_ipc_endpoint_t client_ep = ipc_endpoint_create_udp(
        TEST_CLIENT_PORT,  // 客户端本地端口
        TEST_SERVER_PORT,  // 客户端发送目标端口（服务端接收端口，核心！）
        nullptr,           // 客户端无需回调
        nullptr
    );
    ASSERT_NE(client_ep, nullptr) << "客户端端点创建失败！";
    std::cout << "[客户端] 端点创建成功，准备发送到127.0.0.1:" << TEST_SERVER_PORT << std::endl;

    // ===================== 4. 客户端发送数据 =====================
    std::cout << "[客户端] 开始发送数据..." << std::endl;
    int send_ret = client_ep->send(client_ep, test_data, test_data_len);
    ASSERT_EQ(send_ret, 0) << "客户端发送数据失败！send返回值：" << send_ret;
    std::cout << "[客户端] 数据发送完成，send返回值：" << send_ret << std::endl;

    // ===================== 5. 等待回调触发（适配ipc_udp的线程接收逻辑） =====================
    // 等待最多3秒，确保回调被触发（handle_udp_connection是死循环，需等待数据接收）
    int wait_count = 0;
    while (!g_cb_triggered && wait_count < 30) { // 3秒（每次100ms）
        usleep(100000);
        wait_count++;
    }
    ASSERT_TRUE(g_cb_triggered) << "3秒超时！服务端未收到数据，检查端口/发送逻辑";

    // ===================== 6. 核心验证：收发数据一致性 =====================
    std::cout << "\n[验证结果] 对比发送/接收数据：" << std::endl;
    std::cout << "  → 发送长度：" << test_data_len << " 字节" << std::endl;
    std::cout << "  → 接收长度：" << g_recv_len << " 字节" << std::endl;
    std::cout << "  → 发送内容：" << test_data << std::endl;
    std::cout << "  → 接收内容：" << g_recv_buffer << std::endl;

    // 断言1：长度一致
    EXPECT_EQ(g_recv_len, test_data_len) << "数据长度不一致！发送：" << test_data_len << " 接收：" << g_recv_len;
    // 断言2：内容一致
    EXPECT_STREQ(g_recv_buffer, test_data) << "数据内容不一致！";

    // ===================== 7. 资源清理（注意：ipc_udp.cpp的destroy未close套接字，测试中不影响） =====================
    ipc_endpoint_destroy_udp(server_ep);
    ipc_endpoint_destroy_udp(client_ep);
    std::cout << "\n[测试结束] 资源已清理" << std::endl;
}