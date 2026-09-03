#ifndef __UUID_H__
#define __UUID_H__

#include <iostream>

/**
 * 获取无线网卡的 MAC 地址
 *
 * 该函数遍历 /sys/class/net/ 目录下的所有网络接口，
 * 查找名称以 "wlan" 或 "wlp" 开头的无线网卡接口，
 * 并读取其对应的 address 文件以获取 MAC 地址。
 *
 * @return 无线网卡的 MAC 地址，如果未找到则返回空字符串
 */
std::string get_wireless_mac_address();

/**
 * 生成 UUID
 *
 * 该函数使用 std::random_device 和 std::mt19937 生成一个随机的 UUID。
 * UUID 的格式为 8-4-4-4-12 的 32 位十六进制数字。
 *
 * @return 生成的 UUID 字符串
 */
std::string generate_uuid();

/**
 * 从指定配置文件中读取 UUID
 *
 * 该函数尝试从传入的 cfg_file 路径读取 UUID。
 * 如果文件存在且包含有效的 UUID，则返回该 UUID；否则返回空字符串。
 *
 * @param cfg_file 配置文件的完整路径（如 "/etc/xiaozhi.cfg"）
 * @return 从配置文件中读取的 UUID，未找到/失败则返回空字符串
 */
std::string read_uuid_from_config(const std::string& cfg_file);

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
bool write_uuid_to_config(const std::string& uuid, const std::string& cfg_file);

#endif