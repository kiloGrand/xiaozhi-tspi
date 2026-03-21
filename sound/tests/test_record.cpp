#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include <sys/stat.h>
#include "record.h"

// 全局变量（测试专用）
static FILE *g_test_record_file = NULL;
static volatile int g_test_stop_record = 0;

// 录音回调函数：写入数据到文件
static void test_audio_record_cb(unsigned char *data, size_t len, void *user_data) {
    if (g_test_record_file && !g_test_stop_record) {
        fwrite(data, 1, len, g_test_record_file);
        fflush(g_test_record_file);
    }
}

// 测试用例：录制5秒音频并保存为文件
TEST(RecordTest, RecordToFileFor5Seconds) {
    // 1. 初始化变量
    g_test_stop_record = 0;
    const char *record_file = "./record.pcm";  // 测试生成的PCM文件

    // 2. 打开录音文件（tests目录下）
    g_test_record_file = fopen(record_file, "wb");
    ASSERT_NE(g_test_record_file, nullptr) << "打开录音文件失败: " << record_file;

    // 3. 创建录音线程
    pthread_t record_thread = create_record_thread(test_audio_record_cb, NULL);
    ASSERT_NE(record_thread, (pthread_t)0) << "创建录音线程失败";

    // 4. 录制5秒（核心：自动停止，无需Ctrl+C）
    printf("开始录制音频，时长5秒...\n");
    sleep(5);
    g_test_stop_record = 1;  // 触发停止录音

    // 5. 回收线程资源
    pthread_cancel(record_thread);
    pthread_join(record_thread, NULL);

    // 6. 关闭文件
    fclose(g_test_record_file);
    g_test_record_file = NULL;

    // 7. 验证文件是否生成且非空
    struct stat file_stat;
    ASSERT_EQ(stat(record_file, &file_stat), 0) << "录音文件未生成: " << record_file;
    ASSERT_GT(file_stat.st_size, 0) << "录音文件为空: " << record_file;

    // 8. 打印播放提示
    unsigned int sample_rate, channels;
    snd_pcm_format_t format;
    get_actual_record_settings(&sample_rate, &channels, &format);
    printf("录音完成！文件路径: %s\n", record_file);
    printf("播放命令: ffplay -f s16le -ar %d -ac %d %s\n", sample_rate, channels, record_file);
}
