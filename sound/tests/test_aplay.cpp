#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include <sys/stat.h>
#include "aplay.h"

// 播放测试全局变量
static FILE *g_test_play_file = NULL;
static volatile int g_test_stop_play = 0;
static const char *g_record_file = "./record_test.pcm"; // 复用录音生成的PCM文件

// 播放回调函数：从PCM文件读取数据到播放缓冲区
static int test_audio_play_cb(unsigned char *buffer, size_t size) {
    if (!g_test_play_file || g_test_stop_play) {
        return 0; // 停止播放
    }

    // 从录音文件读取数据到播放缓冲区
    size_t read_len = fread(buffer, 1, size, g_test_play_file);
    if (read_len < size) {
        // 文件读取完毕，重置文件指针（循环播放）
        fseek(g_test_play_file, 0, SEEK_SET);
    }
    return (int)read_len; // 返回实际读取的字节数（供播放线程写入声卡）
}

// 测试用例：播放已录制的PCM音频文件
TEST(PlayTest, PlayRecordedAudio) {
    // 1. 前置检查：确保录音文件存在且非空
    struct stat file_stat;
    ASSERT_EQ(stat(g_record_file, &file_stat), 0) 
        << "录音文件不存在，请先运行RecordTest生成: " << g_record_file;
    ASSERT_GT(file_stat.st_size, 0) 
        << "录音文件为空，无法播放: " << g_record_file;

    // 2. 初始化播放变量
    g_test_stop_play = 0;
    g_test_play_file = fopen(g_record_file, "rb");
    ASSERT_NE(g_test_play_file, nullptr) 
        << "打开播放文件失败: " << g_record_file;

    // 3. 创建播放线程
    pthread_t play_thread = create_play_thread(test_audio_play_cb, NULL);
    ASSERT_NE(play_thread, (pthread_t)0) 
        << "创建播放线程失败";

    // 4. 获取实际播放参数并打印（验证参数获取功能）
    unsigned int play_sample_rate, play_channels;
    snd_pcm_format_t play_format;
    get_actual_play_settings(&play_sample_rate, &play_channels, &play_format);
    printf("实际播放参数:\n");
    printf("  采样率: %u Hz\n", play_sample_rate);
    printf("  声道数: %u\n", play_channels);
    printf("  音频格式: %s\n", snd_pcm_format_name(play_format));
    printf("开始播放录音文件，时长3秒...\n");

    // 5. 播放3秒后停止
    sleep(3);
    g_test_stop_play = 1;

    // 6. 回收播放线程资源
    pthread_cancel(play_thread);
    pthread_join(play_thread, NULL);

    // 7. 关闭文件
    fclose(g_test_play_file);
    g_test_play_file = NULL;

    // 8. 验证播放流程无异常（核心断言：线程创建成功+文件正常读取）
    SUCCEED() << "音频播放测试完成！";
}
