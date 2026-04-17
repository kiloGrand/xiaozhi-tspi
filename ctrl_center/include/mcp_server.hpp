#ifndef MCP_SERVER_H
#define MCP_SERVER_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <utility>
#include <algorithm>
#include <mutex>
#include <iostream>

// nlohmann json
#include <json.hpp>
using json = nlohmann::json;

// 工具返回值
class ReturnValue {
private:
    enum Type { TYPE_BOOL, TYPE_INT, TYPE_FLOAT, TYPE_STRING };
    Type type_;
    bool bool_val_{};
    int int_val_{};
    float float_val_{};
    std::string str_val_;

public:
    ReturnValue(bool b) : type_(TYPE_BOOL), bool_val_(b) {}
    ReturnValue(int i) : type_(TYPE_INT), int_val_(i) {}
    ReturnValue(float f) : type_(TYPE_FLOAT), float_val_(f) {}
    ReturnValue(const std::string& s) : type_(TYPE_STRING), str_val_(s) {}
    ReturnValue(const char* s) : type_(TYPE_STRING), str_val_(s) {}

    bool is_bool()    const { return type_ == TYPE_BOOL; }
    bool is_int()     const { return type_ == TYPE_INT; }
    bool is_float()   const { return type_ == TYPE_FLOAT; }
    bool is_string()  const { return type_ == TYPE_STRING; }

    bool get_bool()   const { return bool_val_; }
    int get_int()     const { return int_val_; }
    float get_float() const { return float_val_; }
    const std::string& get_string() const { return str_val_; }
};

// 参数类型
enum PropertyType {
    kPropertyTypeBoolean,
    kPropertyTypeInteger,
    kPropertyTypeFloat,
    kPropertyTypeString
};

// 参数类
class Property {
private:
    std::string name_;
    PropertyType type_;
    bool bool_val_{};
    int int_val_{};
    float float_val_{};
    std::string str_val_;
    int value_type_{0};

    bool has_default_value_{false};
    bool has_min_{false};
    bool has_max_{false};
    float min_value_{0};
    float max_value_{0};

public:
    Property(const std::string& name, PropertyType type)
        : name_(name), type_(type) {}

    Property(const std::string& name, PropertyType type, bool default_value)
        : name_(name), type_(type), has_default_value_(true), bool_val_(default_value) { value_type_ = 1; }
    Property(const std::string& name, PropertyType type, int default_value)
        : name_(name), type_(type), has_default_value_(true), int_val_(default_value) { value_type_ = 2; }
    Property(const std::string& name, PropertyType type, float default_value)
        : name_(name), type_(type), has_default_value_(true), float_val_(default_value) { value_type_ = 3; }
    Property(const std::string& name, PropertyType type, const std::string& default_value)
        : name_(name), type_(type), has_default_value_(true), str_val_(default_value) { value_type_ = 4; }

    Property(const std::string& name, PropertyType type, float min_value, float max_value)
        : name_(name), type_(type), has_min_(true), has_max_(true), min_value_(min_value), max_value_(max_value) {
        if (type != kPropertyTypeInteger && type != kPropertyTypeFloat)
            throw std::invalid_argument("仅数值支持范围限制");
    }

    Property(const std::string& name, PropertyType type, float default_value, float min_value, float max_value)
        : name_(name), type_(type), has_default_value_(true), has_min_(true), has_max_(true),
          min_value_(min_value), max_value_(max_value), float_val_(default_value) {
        if (type != kPropertyTypeInteger && type != kPropertyTypeFloat)
            throw std::invalid_argument("仅数值支持范围限制");
        if (default_value < min_value || default_value > max_value)
            throw std::invalid_argument("默认值超出范围");
        value_type_ = (type == kPropertyTypeInteger) ? 2 : 3;
    }

    const std::string& name() const { return name_; }
    PropertyType type() const { return type_; }
    bool has_default_value() const { return has_default_value_; }
    bool has_range() const { return has_min_ && has_max_; }
    float min_value() const { return min_value_; }
    float max_value() const { return max_value_; }

    bool value_bool()    const { return bool_val_; }
    int value_int()      const { return int_val_; }
    float value_float()  const { return float_val_; }
    std::string value_string() const { return str_val_; }

    void set_bool(bool v)    { bool_val_ = v; value_type_ = 1; }
    void set_int(int v)      { int_val_ = v; value_type_ = 2; }
    void set_float(float v)  { float_val_ = v; value_type_ = 3; }
    void set_string(const std::string& v) { str_val_ = v; value_type_ = 4; }

    std::string to_json() const {
        json j;
        switch (type_) {
            case kPropertyTypeBoolean: j["type"] = "boolean"; if (has_default_value_) j["default"] = bool_val_; break;
            case kPropertyTypeInteger: j["type"] = "integer"; if (has_default_value_) j["default"] = int_val_; break;
            case kPropertyTypeFloat: j["type"] = "number"; if (has_default_value_) j["default"] = float_val_; break;
            case kPropertyTypeString: j["type"] = "string"; if (has_default_value_) j["default"] = str_val_; break;
        }
        if (has_min_) j["minimum"] = min_value_;
        if (has_max_) j["maximum"] = max_value_;
        return j.dump(-1);
    }
};

// 参数列表
class PropertyList {
private:
    std::vector<Property> properties_;

public:
    PropertyList() = default;
    PropertyList(const std::vector<Property>& properties) : properties_(properties) {}

    void AddProperty(const Property& prop) { properties_.push_back(prop); }

    const Property& operator[](const std::string& name) const {
        for (const auto& p : properties_) if (p.name() == name) return p;
        throw std::runtime_error("参数不存在: " + name);
    }

    std::vector<Property>::iterator begin() { return properties_.begin(); }
    std::vector<Property>::iterator end() { return properties_.end(); }
    std::vector<Property>::const_iterator begin() const { return properties_.begin(); }
    std::vector<Property>::const_iterator end() const { return properties_.end(); }

    std::vector<std::string> GetRequired() const {
        std::vector<std::string> req;
        for (const auto& p : properties_) if (!p.has_default_value()) req.push_back(p.name());
        return req;
    }

    std::string to_json() const {
        json j;
        for (const auto& p : properties_) j[p.name()] = json::parse(p.to_json());
        return j.dump(-1);
    }
};

// MCP工具
class McpTool {
private:
    std::string name_;
    std::string description_;
    PropertyList properties_;
    std::function<ReturnValue(const PropertyList&)> callback_;
    bool user_only_{false};

public:
    McpTool(const std::string& name, const std::string& desc,
            const PropertyList& props, std::function<ReturnValue(const PropertyList&)> cb)
        : name_(name), description_(desc), properties_(props), callback_(cb) {}

    void set_user_only(bool v) { user_only_ = v; }
    const std::string& name() const { return name_; }
    const std::string& description() const { return description_; }
    const PropertyList& properties() const { return properties_; }
    bool user_only() const { return user_only_; }

    std::string to_json() const {
        json j;
        j["name"] = name_;
        j["description"] = description_;
        json input_schema;
        input_schema["type"] = "object";
        input_schema["properties"] = json::parse(properties_.to_json());
        auto req = properties_.GetRequired();
        if (!req.empty()) input_schema["required"] = req;
        j["inputSchema"] = input_schema;
        if (user_only_) {
            json anno;
            anno["audience"] = {"user"};
            j["annotations"] = anno;
        }
        return j.dump(-1);
    }

    std::string Call(const PropertyList& params) {
        ReturnValue ret = callback_(params);
        json j, item;
        item["type"] = "text";
        if (ret.is_bool())      item["text"] = ret.get_bool() ? "true" : "false";
        else if (ret.is_int())  item["text"] = std::to_string(ret.get_int());
        else if (ret.is_float())item["text"] = std::to_string(ret.get_float());
        else if (ret.is_string())item["text"] = ret.get_string();
        j["content"] = {item};
        j["isError"] = false;
        return j.dump(-1);
    }
};

// MCP服务单例
class McpServer {
private:
    std::vector<McpTool*> tools_;
    std::mutex mutex_;
    std::string last_response_; // 存储最后一次响应

    McpServer() = default;
    ~McpServer() {
        for (auto* tool : tools_) delete tool;
        tools_.clear();
    }

    void ReplyResult(int id, const std::string& result);
    void ReplyError(int id, const std::string& msg);
    void GetToolsList(int id, const std::string& cursor);
    void DoToolCall(int id, const std::string& tool_name, const json& args);
    PropertyList ParseJsonToParams(const PropertyList& proto, const json& args);

public:
    McpServer(const McpServer&) = delete;
    McpServer& operator=(const McpServer&) = delete;
    static McpServer& GetInstance() { static McpServer instance; return instance; }

    // 获取最后一次MCP响应（核心：回传云端）
    std::string GetLastResponse() {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_response_;
    }

    void AddCommonTools();
    void AddTool(McpTool* tool);
    void AddTool(const std::string& name, const std::string& desc,
                 const PropertyList& props, std::function<ReturnValue(const PropertyList&)> cb);
    void AddUserOnlyTool(const std::string& name, const std::string& desc,
                         const PropertyList& props, std::function<ReturnValue(const PropertyList&)> cb);
    void ParseMessage(const std::string& message);
    void ParseCapabilities(const json& capabilities);
};

// 方法实现
inline void McpServer::AddTool(McpTool* tool) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tool) tools_.push_back(tool);
}

inline void McpServer::AddTool(const std::string& name, const std::string& desc,
                               const PropertyList& props, std::function<ReturnValue(const PropertyList&)> cb) {
    AddTool(new McpTool(name, desc, props, cb));
}

inline void McpServer::AddUserOnlyTool(const std::string& name, const std::string& desc,
                                       const PropertyList& props, std::function<ReturnValue(const PropertyList&)> cb) {
    auto* tool = new McpTool(name, desc, props, cb);
    tool->set_user_only(true);
    AddTool(tool);
}

inline PropertyList McpServer::ParseJsonToParams(const PropertyList& proto, const json& args) {
    PropertyList params;
    for (const auto& prop : proto) {
        const std::string& name = prop.name();
        if (!args.contains(name)) continue;
        switch (prop.type()) {
            case kPropertyTypeBoolean: params.AddProperty(Property(name, kPropertyTypeBoolean, args[name].get<bool>())); break;
            case kPropertyTypeInteger: params.AddProperty(Property(name, kPropertyTypeInteger, args[name].get<int>())); break;
            case kPropertyTypeFloat: params.AddProperty(Property(name, kPropertyTypeFloat, args[name].get<float>())); break;
            case kPropertyTypeString: params.AddProperty(Property(name, kPropertyTypeString, args[name].get<std::string>())); break;
        }
    }
    return params;
}

inline void McpServer::DoToolCall(int id, const std::string& tool_name, const json& args) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto* tool : tools_) {
        if (tool->name() == tool_name) {
            try {
                auto params = ParseJsonToParams(tool->properties(), args);
                std::string result = tool->Call(params);
                ReplyResult(id, result);
                return;
            } catch (const std::exception& e) {
                ReplyError(id, std::string("调用失败: ") + e.what());
                return;
            }
        }
    }
    ReplyError(id, "工具不存在: " + tool_name);
}

inline void McpServer::GetToolsList(int id, const std::string& cursor) {
    std::lock_guard<std::mutex> lock(mutex_); // 保留Linux线程安全
    const int max_payload_size = 8000;
    std::string json = "{\"tools\":[";
    
    bool found_cursor = cursor.empty();
    auto it = tools_.begin();
    std::string next_cursor = "";
    
    while (it != tools_.end()) {
        // 游标查找逻辑：未找到则跳过
        if (!found_cursor) {
            if ((*it)->name() == cursor) {
                found_cursor = true;
            } else {
                ++it;
                continue;
            }
        }
        
        // 生成工具JSON并检查载荷大小
        std::string tool_json = (*it)->to_json() + ",";
        // 预留30字节安全余量
        if (json.length() + tool_json.length() + 30 > max_payload_size) {
            next_cursor = (*it)->name();
            break;
        }
        
        json += tool_json;
        ++it;
    }
    
    // 移除末尾多余逗号
    if (!json.empty() && json.back() == ',') {
        json.pop_back();
    }
    
    // 异常处理：未添加任何工具（超出大小限制）
    if (json == "{\"tools\":[" && !tools_.empty()) {
        std::cerr << "tools/list: Failed to add tool " << next_cursor.c_str() << " because of payload size limit" << std::endl;
        ReplyError(id, "Failed to add tool " + next_cursor + " because of payload size limit");
        return;
    }

    // 拼接结束符与分页游标
    if (next_cursor.empty()) {
        json += "]}";
    } else {
        json += "],\"nextCursor\":\"" + next_cursor + "\"}";
    }
    
    // 回复结果
    ReplyResult(id, json);
}

inline void McpServer::ReplyResult(int id, const std::string& result) {
    json j;
    j["jsonrpc"] = "2.0";
    j["id"] = id;
    j["result"] = json::parse(result);
    last_response_ = j.dump(-1);
    // std::cout << "MCP Response: " << j.dump(2) << std::endl;
}

inline void McpServer::ReplyError(int id, const std::string& msg) {
    json j;
    j["jsonrpc"] = "2.0";
    j["id"] = id;
    j["error"]["code"] = -32000;
    j["error"]["message"] = msg;
    last_response_ = j.dump(-1);
    // std::cout << "MCP Error: " << j.dump(2) << std::endl;
}

inline void McpServer::ParseMessage(const std::string& message) {
    try {
        json j = json::parse(message);

        // 1. 校验 JSONRPC 2.0 版本
        if (!j.contains("jsonrpc") || j["jsonrpc"] != "2.0") {
            ReplyError(0, "Invalid JSONRPC version");
            return;
        }

        // 2. 获取方法名
        if (!j.contains("method") || !j["method"].is_string()) {
            ReplyError(0, "Missing method");
            return;
        }
        std::string method_str = j["method"].get<std::string>();

        // 3. 过滤 notifications 通知（和ESP32一致）
        if (method_str.find("notifications") == 0) {
            return;
        }

        // 4. 获取 params
        json params = j.value("params", json::object());
        if (!params.is_object()) {
            ReplyError(0, "Invalid params");
            return;
        }

        // 5. 校验请求ID（必须是数字）
        if (!j.contains("id") || !j["id"].is_number_integer()) {
            ReplyError(0, "Invalid id");
            return;
        }
        int id_int = j["id"].get<int>();

        // ==================== MCP 标准方法 ====================
        // 1. 初始化（缺失的核心方法！）
        if (method_str == "initialize") {
            // 拍摄识别的能力
            if (params.contains("capabilities") && params["capabilities"].is_object()) {
                ParseCapabilities(params["capabilities"]);
            }

            // 标准初始化响应
            std::string init_resp = R"({
                "protocolVersion":"2024-11-05",
                "capabilities":{"tools":{}},
                "serverInfo":{"name":"Linux_MCP_Server","version":"1.0.0"}
            })";
            ReplyResult(id_int, init_resp);
        }
        // 2. 获取工具列表（标准名：tools/list）
        else if (method_str == "tools/list") {
            std::string cursor_str = params.value("cursor", "");
            GetToolsList(id_int, cursor_str);
        }
        // 3. 调用工具（标准名：tools/call）
        else if (method_str == "tools/call") {
            if (!params.contains("name") || !params["name"].is_string()) {
                ReplyError(id_int, "Missing tool name");
                return;
            }
            std::string tool_name = params["name"].get<std::string>();
            json tool_args = params.value("arguments", json::object());
            DoToolCall(id_int, tool_name, tool_args);
        }
        // 4. 不支持的方法
        else {
            ReplyError(id_int, "Method not implemented: " + method_str);
        }

    } catch (const std::exception& e) {
        ReplyError(0, std::string("消息解析失败: ") + e.what());
    }
}


inline void McpServer::AddCommonTools() {
    // 计算器
    AddTool("self.calculator",
        "计算两个浮点数的四则运算。支持加法(+)、减法(-)、乘法(*)、除法(/)运算。\n"
        "Args:\n"
        "  `a`: 第一个操作数，浮点数类型。\n"
        "  `b`: 第二个操作数，浮点数类型。\n"
        "  `operation`: 运算类型，必须是'+'、'-'、'*'或'/'中的一个。\n"
        "Return:\n"
        "  标准化JSON响应，固定包含success、msg、result三个字段。",
        PropertyList({
            Property("a", kPropertyTypeFloat),
            Property("b", kPropertyTypeFloat),
            Property("operation", kPropertyTypeString)
        }),
        [](const PropertyList& properties) -> ReturnValue {
            // 从属性中获取浮点数参数
            float a = properties["a"].value_float();
            float b = properties["b"].value_float();
            std::string operation = properties["operation"].value_string();
                        
            // 验证运算符有效性 统一返回格式
            if (operation != "+" && operation != "-" && operation != "*" && operation != "/") {
                return "{\"success\": false, \"msg\": \"无效的运算符，请使用 +, -, *, / 中的一个\", \"result\": null}";
            }
            
            double result = 0.0;
            std::string operation_name;
            
            // 执行四则运算
            if (operation == "+") {
                result = a + b;
                operation_name = "加法";
            } else if (operation == "-") {
                result = a - b;
                operation_name = "减法";
            } else if (operation == "*") {
                result = a * b;
                operation_name = "乘法";
            } else if (operation == "/") {
                // 除数为0判断
                if (b == 0.0) {
                    return "{\"success\": false, \"msg\": \"错误：除数不能为零\", \"result\": null}";
                }
                result = a / b;
                operation_name = "除法";
            }
            
            // 浮点数精度格式化处理
            std::stringstream result_stream;
            result_stream << std::fixed << std::setprecision(10) << result;
            std::string result_str = result_stream.str();
            
            // 去除末尾多余的0和小数点
            result_str.erase(result_str.find_last_not_of('0') + 1, std::string::npos);
            if (result_str.back() == '.') {
                result_str.pop_back();
            }

            // 格式化输入数字字符串
            std::string a_str = std::to_string(a);
            std::string b_str = std::to_string(b);
            a_str.erase(a_str.find_last_not_of('0') + 1, std::string::npos);
            if (a_str.back() == '.') a_str.pop_back();
            b_str.erase(b_str.find_last_not_of('0') + 1, std::string::npos);
            if (b_str.back() == '.') b_str.pop_back();
            
            // 构建计算结果描述
            std::string equation = a_str + " " + operation + " " + b_str + " = " + result_str;
            
            // 统一返回：success + msg + result 三个字段
            return "{\"success\": true, \"msg\": \"" + equation + 
                "\", \"result\": " + result_str + "}";
        });

    AddTool("self.smart_home.get_temperature_humidity",
        "查询当前室内环境温湿度数据，返回温度和湿度数值。\n"
        "Args:\n"
        "  无输入参数。\n"
        "Return:\n"
        "  标准化JSON响应，固定包含success、msg、result三个字段，result包含温度、湿度数据。",
        PropertyList({}),  // 无参数
        [](const PropertyList& properties) -> ReturnValue {
            // ====================== 预留硬件扩展接口 ======================
            // 后续可替换为真实传感器读取：float temp = ReadTemperature(); float humi = ReadHumidity();
            float temperature = 26.0f;  // 固定温度值
            float humidity = 30.0f;     // 固定湿度值
        
            // 统一返回格式：success + msg + result（result为JSON对象）
            return "{\"success\": true, \"msg\": \"成功获取室内温湿度\", \"result\": {\"temperature\": 26.0, \"humidity\": 30.0}}";
        });

    AddTool("self.smart_home.control_light",
        "控制智能家居灯具的开关状态，支持打开/关闭指定灯具。\n"
        "Args:\n"
        "  `device_id`: 灯具设备ID（字符串，如台灯、客厅灯）。\n"
        "  `status`: 灯具状态，仅支持 on(打开) / off(关闭)。\n"
        "Return:\n"
        "  标准化JSON响应，固定包含success、msg、result三个字段。",
        PropertyList({
            Property("device_id", kPropertyTypeString),  // 灯具ID
            Property("status", kPropertyTypeString)      // 开关状态
        }),
        [](const PropertyList& properties) -> ReturnValue {
            // 获取参数
            std::string device_id = properties["device_id"].value_string();
            std::string status = properties["status"].value_string();
    
            // 参数校验
            if (status != "on" && status != "off") {
                return "{\"success\": false, \"msg\": \"无效的状态值，仅支持 on/off\", \"result\": null}";
            }
    
            // ====================== 预留硬件扩展接口 ======================
            // 后续可替换为真实硬件控制：SetLightStatus(device_id, status);
                
            // 统一返回格式
            return "{\"success\": true, \"msg\": \"灯具控制执行成功\", \"result\": {\"device_id\": \"" + device_id + "\", \"status\": \"" + status + "\"}}";
        });
}

// 解析能力
inline void McpServer::ParseCapabilities(const json& capabilities) {
    // 1. 获取 vision 子对象（对应 cJSON_GetObjectItem）
    if (!capabilities.contains("vision") || !capabilities["vision"].is_object()) {
        return;
    }
    const json& vision = capabilities["vision"];

    // 2. 获取 url 和 token 字段
    if (!vision.contains("url") || !vision["url"].is_string()) {
        return;
    }
    std::string url_str = vision["url"].get<std::string>();
    std::string token_str;
    
    // 3. 安全获取 token（不存在则为空字符串）
    if (vision.contains("token") && vision["token"].is_string()) {
        token_str = vision["token"].get<std::string>();
    }

    // 云端的视觉ai
    std::cout << "url:" << url_str << std::endl << "token: " << token_str << std::endl;
}

#endif // MCP_SERVER_H