#include <iostream>
#include <string>
#include <sstream>
#include <gtest/gtest.h>
#include "uuid.h"
#include "http.h"

// 仅测试：正常参数调用 active_device（完全复用主程序逻辑）
TEST(ActiveDeviceTest, NormalParams) {
    // 1. 构造无线网卡MAC（无网卡时用模拟值）
    std::string wireless_mac = get_wireless_mac_address();
    if (wireless_mac.empty()) {
        wireless_mac = "00:11:22:33:44:55"; // 模拟MAC，避免环境问题导致测试失败
        std::cout << "测试提示：无无线网卡，使用模拟MAC: " << wireless_mac << std::endl;
    }

    // 2. 生成UUID（和主程序逻辑一致）
    std::string uuid = generate_uuid();
    ASSERT_FALSE(uuid.empty()) << "UUID生成失败，测试无法继续";

    // 3. 构造http_data_t参数（完全复用主程序逻辑）
    http_data_t http_data{}; // 初始化结构体，避免野指针
    http_data.url = "https://api.tenclass.net/xiaozhi/ota/";

    // 填充post字段（替换uuid）
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

    // 填充headers字段（替换Device-Id）
    std::ostringstream headers_stream;
    headers_stream << R"(
        {
            "Content-Type": "application/json",
            "Device-Id": ")" << wireless_mac << R"(",
            "User-Agent": "weidongshan1",
            "Accept-Language": "zh-CN"
        }
    )";
    http_data.headers = headers_stream.str();

    // 4. 调用active_device函数
    char active_code[20] = {0}; // 初始化数组，避免脏数据
    int ret = active_device(&http_data, active_code);

    // 5. 核心断言（根据实际业务逻辑调整）
    std::cout << "测试结果：active_device返回值=" << ret << "，Active-Code=" << active_code << std::endl;
    
    // ret 为 1表示未激活，为 0 表示激活，为 -1 表示解析出错
    EXPECT_NE(ret, -1) << "active_device调用异常（返回-1，致命错误）"; // 非致命错误则继续
    EXPECT_FALSE(strlen(active_code) == 0) << "active_code为空，不符合预期"; // 至少返回提示码
    EXPECT_LE(strlen(active_code), sizeof(active_code)-1) << "active_code超出缓冲区，存在溢出风险";
}
