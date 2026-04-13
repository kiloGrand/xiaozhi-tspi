#include "ws_client.h"

#include <gtest/gtest.h>
#include "ws_client.h"
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>

// ==================== 模拟业务回调函数（与原代码完全一致） ====================
/// @brief 模拟：处理下载的Opus二进制数据
void process_opus_data_downloaded(const char* buffer, size_t size) {
    std::cout << "[TEST Callback] 收到二进制Opus数据 | 大小: " << size << " 字节" << std::endl;
}

/// @brief 模拟：处理下载的文本数据
void process_txt_data_downloaded(const char* buffer, size_t size) {
    std::string recv_msg(buffer, size);
    std::cout << "[TEST Callback] 收到文本数据: " << recv_msg << std::endl;
}

/// @brief 模拟：WebSocket连接关闭回调
void on_websocket_closed(short close_code) {
    std::cout << "[TEST Callback] WebSocket连接关闭 | 关闭码: " << close_code << std::endl;
}

// ==================== 模拟设备状态逻辑（与原代码完全一致） ====================
// 原代码设备状态宏定义
const int kDeviceStateListening = 1;

/// @brief 模拟：设置设备状态
void set_device_state(int state) {
    std::cout << "[TEST] 设置设备状态: " << state << std::endl;
}

/// @brief 模拟：发送设备状态
void send_device_state() {
    std::cout << "[TEST] 发送设备状态成功" << std::endl;
}

// ==================== Google Test 测试用例 ====================
/**
 * @brief WebSocket客户端核心功能测试
 * 测试内容：初始化、连接、回调、发送数据、状态查询
 */
TEST(WebSocketClientTest, FullFunctionTest) {
    // 1. 模拟硬件唯一标识（MAC地址 + UUID，与原代码一致）
    std::string mac = "00:00:00:00:00:00";
    std::string uuid = "d560294c-01d9-47d0-b538-085f38744b05";

    // 2. 拼接WebSocket请求头（**完全复用你提供的代码**）
    std::ostringstream ws_headers_stream;
    ws_headers_stream << R"(
        {
            "Authorization": "Bearer test-token",
            "Protocol-Version": "1",
            "Device-Id": ")" << mac << R"(",
            "Client-Id": ")" << uuid << R"("
        }
    )";
    std::string headers = ws_headers_stream.str();

    // 3. Hello握手消息（**完全复用你提供的代码**）
    std::string hello_msg = R"(
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

    // 4. 服务器配置（**完全复用你提供的地址**）
    std::string host = "api.tenclass.net";
    std::string port = "443";
    std::string path = "/xiaozhi/v1/";

    // ==================== 面向对象版本调用 ====================
    // 5. 创建WebSocket客户端实例
    WebSocketClient ws_client(host, port, path, hello_msg, headers);

    // 6. 设置业务回调（替代原 websocket_set_callbacks）
    ws_client.set_callbacks(
        process_opus_data_downloaded,
        process_txt_data_downloaded,
        on_websocket_closed
    );

    // 7. 执行业务逻辑（设备状态）
    set_device_state(kDeviceStateListening);
    send_device_state();

    // 8. 启动WebSocket客户端（替代原 websocket_start()）
    ws_client.start();

    // ==================== 异步等待连接（WebSocket后台线程运行） ====================
    std::cout << "\n[TEST] 等待WebSocket后台线程建立连接...\n" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // ==================== GTest 断言测试 ====================
    // 测试1：验证连接状态
    bool is_connected = ws_client.is_connected();
    std::cout << "[TEST] 当前连接状态: " << (is_connected ? "✅ 已连接" : "❌ 未连接") << std::endl;
    EXPECT_TRUE(is_connected) << "WebSocket 连接服务器失败！";

    // 测试2：发送测试文本数据
    const char* test_text = R"({"type":"test","msg":"hello from gtest"})";
    int text_ret = ws_client.send_text(test_text, strlen(test_text));
    EXPECT_EQ(text_ret, 0) << "发送文本数据失败！";
    std::cout << "[TEST] 发送文本数据返回值: " << text_ret << std::endl;

    // 等待握手完成
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 测试3：发送测试二进制数据
    const uint8_t test_binary[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
    int bin_ret = ws_client.send_binary(reinterpret_cast<const char*>(test_binary), sizeof(test_binary));
    EXPECT_EQ(bin_ret, 0) << "发送二进制数据失败！";
    std::cout << "[TEST] 发送二进制数据返回值: " << bin_ret << std::endl;

    // 保持运行，观察服务器回调
    std::cout << "\n[TEST] 持续监听服务器数据 30 秒...\n" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(30));

    std::cout << "\n[TEST] WebSocket 测试完成！\n" << std::endl;
}
