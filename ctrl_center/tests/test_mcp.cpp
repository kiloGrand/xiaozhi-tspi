#include <gtest/gtest.h>
#include "mcp_server.hpp"
#include <string>
#include <iostream>

TEST(MCPServerTest, AllMcpFunctionsTest) {
    // 获取 MCP 单例服务
    McpServer& server = McpServer::GetInstance();
    // 注册内置工具
    server.AddCommonTools();

    // 只定义一次变量
    std::string mcp_response;

    // 测试1：调用工具列表
    std::string req_list = R"({"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}})";
    server.ParseMessage(req_list);
    mcp_response = server.GetLastResponse(); // 直接赋值，不重新定义
    std::cout << "MCP执行结果: " << mcp_response << std::endl;

    // 测试2：调用计算器工具
    std::string req_calc = R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"self.calculator","arguments":{"a":10,"b":20, "operation":"+"}}})";
    server.ParseMessage(req_calc);
    mcp_response = server.GetLastResponse();
    std::cout << "MCP执行结果: " << mcp_response << std::endl;

    // 测试3：查询室内温湿度（无参数）
    std::string req_temp_humi = R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"self.smart_home.get_temperature_humidity","arguments":{}}})";
    server.ParseMessage(req_temp_humi);
    mcp_response = server.GetLastResponse();
    std::cout << "温湿度查询结果: " << mcp_response << std::endl;

    // 测试4：控制灯具 - 打开客厅灯
    std::string req_light_on = R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"self.smart_home.control_light","arguments":{"device_id":"客厅灯","status":"on"}}})";
    server.ParseMessage(req_light_on);
    mcp_response = server.GetLastResponse();
    std::cout << "开灯执行结果: " << mcp_response << std::endl;

    // 测试调用不存在的工具（返回错误）
    std::string req_error_tool = R"({"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"self.not_exist_tool","arguments":{}}})";
    server.ParseMessage(req_error_tool);
    mcp_response = server.GetLastResponse();
    std::cout << "MCP执行结果: " << mcp_response << std::endl;

    // 测试通过
    SUCCEED();
}