// SPDX-License-Identifier: GPL-3.0-only
/*
 * Copyright (c) 2008-2023 100askTeam : Dongshan WEI <weidongshan@100ask.net> 
 * Discourse:  https://forums.100ask.net
 */
 
/*  Copyright (C) 2008-2023 深圳百问网科技有限公司
 *  All rights reserved
 *
 * 免责声明: 百问网编写的文档, 仅供学员学习使用, 可以转发或引用(请保留作者信息),禁止用于商业用途！
 * 免责声明: 百问网编写的程序, 用于商业用途请遵循GPL许可, 百问网不承担任何后果！
 * 
 * 本程序遵循GPL V3协议, 请遵循协议
 * 百问网学习平台   : https://www.100ask.net
 * 百问网交流社区   : https://forums.100ask.net
 * 百问网官方B站    : https://space.bilibili.com/275908810
 * 本程序所用开发板 : Linux开发板
 * 百问网官方淘宝   : https://100ask.taobao.com
 * 联系我们(E-mail) : weidongshan@100ask.net
 *
 *          版权所有，盗版必究。
 *  
 * 修改历史     版本号           作者        修改内容
 *-----------------------------------------------------
 * 2025.03.20      v01         百问科技      创建文件
 *-----------------------------------------------------
 */
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <dirent.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <string>

// Include nlohmann/json library
#include "json.hpp"
// 简化JSON命名空间，提升代码可读性
using json = nlohmann::json;

/**
* 获取无线网卡的 MAC 地址
*
* 该函数遍历 /sys/class/net/ 目录下的所有网络接口，
* 查找名称以 "wlan" 或 "wlp" 开头的无线网卡接口，
* 并读取其对应的 address 文件以获取 MAC 地址。
* 如果未找到无线网卡接口，则读取第一个可用的网卡接口。
*
* @return 无线网卡的 MAC 地址，如果未找到则返回空字符串
*/
std::string get_wireless_mac_address() {
    DIR *dir;
    struct dirent *entry;
    std::string mac_address;
    std::string first_mac_address;

    // 打开 /sys/class/net/ 目录
    dir = opendir("/sys/class/net/");
    if (dir == nullptr) {
        std::cerr << "Failed to open /sys/class/net/ directory" << std::endl;
        return "";
    }

    // 遍历目录中的所有条目
    while ((entry = readdir(dir)) != nullptr) {
        std::string interface_name = entry->d_name;

        // 检查接口名称是否以 wlan 或 wlp 开头
        if (interface_name.find("wlan") == 0 || interface_name.find("wlp") == 0) {
            std::string address_path = "/sys/class/net/" + interface_name + "/address";

            // 打开 address 文件
            std::ifstream address_file(address_path);
            if (address_file.is_open()) {
                std::getline(address_file, mac_address);
                address_file.close();
                closedir(dir);
                return mac_address;
            }
        } else {
            // 如果不是 wlan 或 wlp 接口，记录第一个可用的接口
            if (first_mac_address.empty()) {
                std::string address_path = "/sys/class/net/" + interface_name + "/address";

                // 打开 address 文件
                std::ifstream address_file(address_path);
                if (address_file.is_open()) {
                    std::getline(address_file, first_mac_address);
                    address_file.close();
                }
            }
        }
    }

    closedir(dir);

    // 如果没有找到 wlan 或 wlp 接口，返回第一个可用的接口的 MAC 地址
    if (!first_mac_address.empty()) {
        return first_mac_address;
    }

    return "";
}

/**
* 生成 UUID
*
* 该函数使用 std::random_device 和 std::mt19937 生成一个随机的 UUID。
* UUID 的格式为 8-4-4-4-12 的 32 位十六进制数字。
*
* @return 生成的 UUID 字符串
*/
std::string generate_uuid() {
    // 使用静态变量确保 random_device 和 mt19937 只被初始化一次
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    int i;

    ss << std::hex;

    // 生成 UUID 的各个部分
    for (i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++) {
        ss << dis(gen);
    };

    return ss.str();
}

/**
 * 从指定配置文件中读取 UUID
 *
 * 该函数尝试从传入的 cfg_file 路径读取 UUID。
 * 如果文件存在且包含有效的 UUID，则返回该 UUID；否则返回空字符串。
 *
 * @param cfg_file 配置文件的完整路径（如 "/etc/xiaozhi.cfg"）
 * @return 从配置文件中读取的 UUID，未找到/失败则返回空字符串
 */
std::string read_uuid_from_config(const std::string& cfg_file) { // 参数改为const std::string&，避免拷贝
    std::ifstream config_file(cfg_file);
    if (!config_file.is_open()) {
        std::cerr << "Failed to open " << cfg_file << " for reading" << std::endl;
        return "";
    }

    try {
        json config_json;
        config_file >> config_json;
        config_file.close(); // 及时关闭文件

        // 检查JSON中是否包含uuid字段
        if (config_json.contains("uuid")) {
            return config_json["uuid"].get<std::string>();
        } else {
            std::cerr << "Config file " << cfg_file << " has no 'uuid' field" << std::endl;
        }
    } catch (const nlohmann::json::parse_error& e) {
        // 解析JSON失败时，输出具体文件名和错误信息
        std::cerr << "Failed to parse " << cfg_file << ": " << e.what() << std::endl;
    } catch (const std::exception& e) { // 补充通用异常捕获，提升健壮性
        std::cerr << "Error reading " << cfg_file << ": " << e.what() << std::endl;
    }

    return "";
}

/**
 * 将 UUID 写入指定配置文件
 *
 * 该函数将给定的 UUID 写入传入的 cfg_file 路径。
 * 如果文件不存在，则创建新文件；如果文件已存在，会覆盖原有内容。
 *
 * @param uuid 要写入配置文件的 UUID
 * @param cfg_file 配置文件的完整路径（如 "/etc/xiaozhi.cfg"）
 * @return 成功写入返回 true，否则返回 false
 */
bool write_uuid_to_config(const std::string& uuid, const std::string& cfg_file) {
    std::ofstream config_file(cfg_file);
    if (!config_file.is_open()) {
        std::cerr << "Failed to open " << cfg_file << " for writing" << std::endl;
        return false;
    }

    try {
        json config_json;
        config_json["uuid"] = uuid;
        // 将JSON格式化写入文件（4个空格缩进）
        config_file << config_json.dump(4);
        config_file.flush(); // 强制刷新缓冲区
        config_file.close(); // 及时关闭文件
        return true;
    } catch (const nlohmann::json::type_error& e) { // 写入时的类型错误（如uuid不是字符串）
        std::cerr << "Failed to serialize UUID to JSON in " << cfg_file << ": " << e.what() << std::endl;
    } catch (const std::exception& e) { // 补充通用异常捕获（如文件写入失败）
        std::cerr << "Error writing to " << cfg_file << ": " << e.what() << std::endl;
    }

    return false;
}
