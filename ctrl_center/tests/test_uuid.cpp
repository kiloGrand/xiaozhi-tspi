#include <gtest/gtest.h>
#include "uuid.h"
#include <regex>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>

// 测试用临时配置文件路径（避免影响系统文件）
#define TEST_CFG_FILE "/tmp/test_xiaozhi.cfg"
// 测试用有效UUID
#define TEST_VALID_UUID "123e4567-e89b-12d3-a456-426614174000"

// 测试：模拟main中“生成5个UUID”的逻辑，验证每个UUID有效
TEST(UUIDTest, Generate5UUIDs) {
    // 匹配UUID标准格式（8-4-4-4-12，大小写不敏感）
    std::regex uuid_regex(R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})");
    
    // 模拟main中的循环，生成5个UUID并逐个验证
    for (int i = 0; i < 5; ++i) {
        std::string uuid = generate_uuid();
        std::cout << "Test Generated UUID " << i + 1 << ": " << uuid << std::endl;
        
        // 断言1：UUID不为空（和main中打印逻辑对应）
        EXPECT_FALSE(uuid.empty()) << "第" << i+1 << "个UUID为空";
        
        // 断言2：UUID格式符合标准（保证生成的是合法UUID）
        EXPECT_TRUE(std::regex_match(uuid, uuid_regex)) << "第" << i+1 << "个UUID格式错误：" << uuid;
    }
}

// 测试：模拟main中“获取无线网卡MAC地址”的逻辑
TEST(MACTest, GetWirelessMACAddress) {
    std::string wireless_mac = get_wireless_mac_address();
    
    // 完全对齐main中的逻辑：有值则打印，无值则提示失败
    if (!wireless_mac.empty()) {
        std::cout << "Test Wireless MAC Address: " << wireless_mac << std::endl;
        // 验证MAC地址格式（xx:xx:xx:xx:xx:xx）
        std::regex mac_regex(R"([0-9a-fA-F]{2}(:[0-9a-fA-F]{2}){5})");
        EXPECT_TRUE(std::regex_match(wireless_mac, mac_regex)) << "MAC地址格式错误：" << wireless_mac;
    } else {
        std::cerr << "Test Failed to get wireless MAC address" << std::endl;
        // 无MAC地址时不报错（兼容无无线网卡的场景）
        SUCCEED() << "无无线网卡，MAC地址为空（非错误）";
    }
}

// UUID配置文件正常读写测试
TEST(UUIDConfigTest, WriteAndReadValidUuid) {
    // 1. 测试前清理残留的临时文件（避免影响测试）
    remove(TEST_CFG_FILE);

    // 2. 写入有效UUID到临时配置文件
    std::cout << "Test Writing UUID to config file: " << TEST_CFG_FILE << std::endl;
    bool write_result = write_uuid_to_config(TEST_VALID_UUID, TEST_CFG_FILE);
    
    // 断言1：写入操作成功
    EXPECT_TRUE(write_result) << "写入UUID到配置文件失败";
    
    // 3. 从临时配置文件读取UUID
    std::cout << "Test Reading UUID from config file: " << TEST_CFG_FILE << std::endl;
    std::string read_uuid = read_uuid_from_config(TEST_CFG_FILE);
    
    // 断言2：读取的UUID不为空
    EXPECT_FALSE(read_uuid.empty()) << "从配置文件读取UUID为空";
    
    // 断言3：读取的UUID与写入的完全一致
    EXPECT_EQ(read_uuid, TEST_VALID_UUID) << "读取的UUID与写入的不一致！" 
                                         << "写入：" << TEST_VALID_UUID 
                                         << " 读取：" << read_uuid;

    // 4. 测试后清理临时文件（保持环境干净）
    remove(TEST_CFG_FILE);
    std::cout << "Test UUID config file read/write success!" << std::endl;
}