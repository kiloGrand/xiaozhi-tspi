#include <gtest/gtest.h>
#include "AlsaCapture.hpp"
#include "AlsaPlayback.hpp"
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>

using namespace AlsaAudio;

// 音频参数配置（录音/播放保持一致，标准PCM格式）
constexpr const char* TEST_PCM_FILE = "test_3s.pcm";
constexpr unsigned int SAMPLE_RATE = 16000;
constexpr unsigned int CHANNELS = 1;
constexpr snd_pcm_format_t FORMAT = SND_PCM_FORMAT_S16_LE;

/**
 * @brief GTest测试用例：录音3秒 → 保存PCM → 读取并播放
 * 测试流程：
 * 1. 启动录音，录制3秒音频
 * 2. 将录音数据保存为 .pcm 原始音频文件
 * 3. 读取PCM文件数据
 * 4. 启动播放，回放音频
 * 5. 断言校验：录音数据有效、文件读写成功
 */
TEST(AlsaAudioTest, Record3Seconds_SaveAndPlayPcm) {
    // ===================== 1. 录音3秒 =====================
    std::vector<uint8_t> record_data;  // 存储录音数据

    // 创建录音对象（16k采样率、单声道、16位）
    AlsaCapture capture(DEFAULT_DEVICE, SAMPLE_RATE, CHANNELS, FORMAT);
    
    // 启动录音：回调函数将数据追加到record_data
    bool start_record = capture.start([&](std::vector<uint8_t>& buffer, size_t size) {
        if (size > 0) {
            record_data.insert(record_data.end(), buffer.begin(), buffer.begin() + size);
        }
    });

    // 断言：录音启动成功
    ASSERT_TRUE(start_record) << "录音启动失败！";
    std::cout << "===== 开始录音（3秒）=====\n";

    // 持续录音3秒
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // 停止录音
    capture.stop();
    std::cout << "===== 录音完成，数据大小：" << record_data.size() << " 字节 =====\n";

    // 断言：录音数据非空（有效音频）
    ASSERT_GT(record_data.size(), 0) << "未录制到任何音频数据！";

    // ===================== 2. 保存为PCM文件 =====================
    std::ofstream pcm_file(TEST_PCM_FILE, std::ios::binary);
    ASSERT_TRUE(pcm_file.is_open()) << "PCM文件创建失败！";

    // 写入原始PCM数据
    pcm_file.write(reinterpret_cast<const char*>(record_data.data()), record_data.size());
    pcm_file.close();
    std::cout << "===== PCM文件已保存：" << TEST_PCM_FILE << " =====\n";
    std::cout << "播放命令: ffplay -f s16le -ar 16000 -ac 1 test_3s.pcm" << std::endl;

    // ===================== 3. 读取PCM文件 =====================
    std::vector<uint8_t> play_data;
    std::ifstream read_file(TEST_PCM_FILE, std::ios::binary);
    ASSERT_TRUE(read_file.is_open()) << "PCM文件读取失败！";

    // 读取文件全部数据
    play_data.assign(std::istreambuf_iterator<char>(read_file), std::istreambuf_iterator<char>());
    read_file.close();
    ASSERT_EQ(play_data.size(), record_data.size()) << "文件读写数据不一致！";

    // ===================== 4. 播放PCM音频 =====================
    size_t play_offset = 0;  // 播放数据偏移量
    bool play_finished = false;

    // 创建播放对象（参数与录音完全一致）
    AlsaPlayback player(DEFAULT_DEVICE, SAMPLE_RATE, CHANNELS, FORMAT);
    
    // 启动播放：回调函数从文件数据中读取音频
    bool start_play = player.start([&](std::vector<uint8_t>& buffer, size_t max_size) -> int {
        // 数据已播放完毕
        if (play_offset >= play_data.size()) {
            play_finished = true;
            return 0;
        }

        // 计算本次可播放的数据长度
        size_t copy_size = std::min(max_size, play_data.size() - play_offset);
        memcpy(buffer.data(), play_data.data() + play_offset, copy_size);

        // 偏移量递增
        play_offset += copy_size;
        return static_cast<int>(copy_size);
    });

    // 断言：播放启动成功
    ASSERT_TRUE(start_play) << "播放启动失败！";
    std::cout << "===== 开始播放PCM文件 =====\n";

    // 等待播放完成
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // 停止播放
    player.stop();
    std::cout << "===== 播放完成！=====\n";

    // 最终断言：播放流程正常完成
    ASSERT_TRUE(play_finished) << "音频播放未正常完成！";
}

TEST(AlsaAudioTest, PlayPcm) {
    // ===================== 3. 读取PCM文件 =====================
    std::vector<uint8_t> play_data;
    std::ifstream read_file(TEST_PCM_FILE, std::ios::binary);
    ASSERT_TRUE(read_file.is_open()) << "PCM文件读取失败！";

    // 读取文件全部数据
    play_data.assign(std::istreambuf_iterator<char>(read_file), std::istreambuf_iterator<char>());
    read_file.close();
    
    // ===================== 4. 播放PCM音频 =====================
    size_t play_offset = 0;  // 播放数据偏移量
    bool play_finished = false;

    // 创建播放对象（参数与录音完全一致）
    AlsaPlayback player(DEFAULT_DEVICE, SAMPLE_RATE, CHANNELS, FORMAT);
    
    // 启动播放：回调函数从文件数据中读取音频
    bool start_play = player.start([&](std::vector<uint8_t>& buffer, size_t max_size) -> int {
        // 数据已播放完毕
        if (play_offset >= play_data.size()) {
            play_finished = true;
            return 0;
        }

        // 计算本次可播放的数据长度
        size_t copy_size = std::min(max_size, play_data.size() - play_offset);
        memcpy(buffer.data(), play_data.data() + play_offset, copy_size);

        // 偏移量递增
        play_offset += copy_size;
        return static_cast<int>(copy_size);
    });

    // 断言：播放启动成功
    ASSERT_TRUE(start_play) << "播放启动失败！";
    std::cout << "===== 开始播放PCM文件 =====\n";

    // 等待播放完成
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // 停止播放
    player.stop();
    std::cout << "===== 播放完成！=====\n";

    // 最终断言：播放流程正常完成
    ASSERT_TRUE(play_finished) << "音频播放未正常完成！";
}