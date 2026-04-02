#include "ipc_udp.h"
#include "json.hpp"
#include <iostream>
#include <string>
#include <cstdlib>

// 启用nlohmann json命名空间
using json = nlohmann::json;

#define UI_PORT_UP    5678      /* GUI向control_center的这个端口上传UI信息 */
#define UI_PORT_DOWN  5679      /* control_center向GUI的这个端口下发UI信息 */

// 静态变量：存储IPC端点
static p_ipc_endpoint_t g_ipc_ep;

/*
 * 处理从IPC接收到的UI数据（仅解析+输出，无UI操作）
 * 支持解析字段：state/text/emotion/wifi/battery
 * @param buffer  JSON格式数据缓冲区
 * @param size    缓冲区大小
 * @param user_data  未使用
 * @return 0=成功，-1=解析失败
 */
static int process_ui_data(char *buffer, size_t size, void *user_data)
{
    // 安全处理：构造字符串（确保\0结尾）
    std::string json_str(buffer, size);
    
    json j;
    try {
        // 解析JSON字符串
        j = json::parse(json_str);
    } catch (const json::parse_error& e) {
        // 解析失败：输出错误信息
        std::cerr << "[错误] JSON解析失败 | 原始数据: " << json_str 
                  << " | 错误详情: " << e.what() << std::endl;
        return -1;
    }

    // ========== 仅输出解析后的字段（无任何UI操作） ==========
    std::cout << "\n[接收数据] 解析结果：" << std::endl;

    // 1. 输出state字段
    if (j.contains("state") && j["state"].is_number_integer()) {
        int state = j["state"].get<int>();
        std::cout << "  - state: " << state << std::endl;
    }

    // 2. 输出text字段
    if (j.contains("text") && j["text"].is_string()) {
        std::string text = j["text"].get<std::string>();
        std::cout << "  - text: " << text << std::endl;
    }

    // 3. 输出emotion字段
    if (j.contains("emotion") && j["emotion"].is_string()) {
        std::string emotion = j["emotion"].get<std::string>();
        std::cout << "  - emotion: " << emotion << std::endl;
    }

    // 4. 输出wifi强度字段
    if (j.contains("wifi") && j["wifi"].is_string()) {
        std::string wifi = j["wifi"].get<std::string>();
        std::cout << "  - wifi强度: " << wifi << "%" << std::endl;
    }

    // 5. 输出电量字段
    if (j.contains("battery") && j["battery"].is_string()) {
        std::string battery = j["battery"].get<std::string>();
        std::cout << "  - 电量: " << battery << "%" << std::endl;
    }

    return 0;
}

int main(void)
{
    // 创建UDP IPC端点（下行端口接收数据）
    g_ipc_ep = ipc_endpoint_create_udp(UI_PORT_DOWN, UI_PORT_UP, process_ui_data, NULL);
    if (!g_ipc_ep) {
        std::cerr << "[错误] 创建UDP IPC端点失败" << std::endl;
        return -1;
    }

    // 启动成功提示
    std::cout << "[信息] UDP IPC服务启动成功" << std::endl;
    std::cout << "[信息] 下行接收端口：" << UI_PORT_DOWN << std::endl;
    std::cout << "[信息] 上行发送端口：" << UI_PORT_UP << std::endl;
    std::cout << "[信息] 等待接收数据...\n" << std::endl;

    // 阻塞主循环（保持程序运行，持续接收数据）
    while (1) {
        // 微睡眠减少CPU占用（需包含<unistd.h>）
        // usleep(10000); // 10ms
    }

    // 理论上不会执行到此处，如需退出可添加资源释放
    ipc_endpoint_destroy_udp(g_ipc_ep);
    return 0;
}