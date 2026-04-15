#include <gtest/gtest.h>
#include "AlsaCapture.hpp"
#include "AlsaPlayback.hpp"
#include "OpusWrapper.hpp"
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <utility>

using namespace AlsaAudio;

// ===================== 工程参数 =====================
constexpr size_t SAMPLE_RATE        = 16000;
constexpr size_t CHANNELS           = 1;
constexpr snd_pcm_format_t FORMAT   = SND_PCM_FORMAT_S16_LE;
constexpr size_t OPUS_FRAME_MS      = 60;
constexpr size_t PCM_BUFFER_SIZE    = (1024*30);
constexpr size_t OPUS_BUFFER_SIZE   = (1024*5);

// 存储【单帧Opus数据 + 单帧长度】（核心修复：不破坏帧边界）
using OpusFrame = std::pair<std::vector<uint8_t>, int>;

TEST(OpusTest, Opus60ms) {
    // ===================== 1. 录音3秒 =====================
    std::vector<uint8_t> raw_pcm;
    AlsaCapture capture(DEFAULT_DEVICE, SAMPLE_RATE, CHANNELS, FORMAT);

    bool rec_start = capture.start([&](std::vector<uint8_t>& buf, size_t size) {
        raw_pcm.insert(raw_pcm.end(), buf.begin(), buf.begin() + size);
    });
    ASSERT_TRUE(rec_start) << "录音启动失败";
    std::cout << "===== 开始录音3秒（60ms Opus帧）=====\n";
    std::this_thread::sleep_for(std::chrono::seconds(3));
    capture.stop();

    ASSERT_GT(raw_pcm.size(), 0) << "未录制到音频数据";
    std::cout << "录音完成 | 原始PCM大小: " << raw_pcm.size() << " 字节\n";

    // ===================== 2. 初始化编解码器 =====================
    OpusEncoder encoder((uint32_t)SAMPLE_RATE, (uint32_t)CHANNELS, OPUS_FRAME_MS,
                        (uint32_t)SAMPLE_RATE, (uint32_t)CHANNELS);
    OpusDecoder decoder((int)SAMPLE_RATE, (int)CHANNELS, (int)OPUS_FRAME_MS,
                        (int)SAMPLE_RATE, (int)CHANNELS);
    ASSERT_TRUE(encoder.isValid()) << "编码器初始化失败";
    ASSERT_TRUE(decoder.isValid()) << "解码器初始化失败";

    // 60ms 单帧PCM大小（和你源码完全一致）
    const size_t frame_60ms_bytes = SAMPLE_RATE * OPUS_FRAME_MS / 1000 * CHANNELS * sizeof(opus_int16);
    std::cout << "60ms单帧PCM: " << frame_60ms_bytes << " 字节\n";

    // ===================== 3. 逐帧编码（保存每帧完整数据，不拼接） =====================
    std::vector<OpusFrame> opus_frames;
    uint8_t record_buf[PCM_BUFFER_SIZE] = {0};
    size_t rec_off = 0;

    for (size_t i = 0; i < raw_pcm.size(); ) {
        size_t free_space = PCM_BUFFER_SIZE - rec_off;
        size_t remain = raw_pcm.size() - i;
        size_t copy_len = std::min(free_space, remain);

        memcpy(record_buf + rec_off, raw_pcm.data() + i, copy_len);
        rec_off += copy_len;
        i += copy_len;

        // 满60ms → 编码1帧
        if (rec_off >= frame_60ms_bytes) {
            std::vector<uint8_t> opus_buf(OPUS_BUFFER_SIZE);
            int opus_len = 0;

            // 编码【完整60ms单帧】
            encoder.encode(record_buf, (int)frame_60ms_bytes, opus_buf.data(), &opus_len);

            if (opus_len > 0) {
                // 保存：单帧数据 + 单帧真实长度（绝对不破坏帧）
                opus_buf.resize(opus_len);
                opus_frames.emplace_back(opus_buf, opus_len);
            }

            // 移除已编码数据
            memmove(record_buf, record_buf + frame_60ms_bytes, rec_off - frame_60ms_bytes);
            rec_off -= frame_60ms_bytes;
        }
    }

    ASSERT_GT(opus_frames.size(), 0) << "无有效Opus帧";
    std::cout << "编码完成 | 总帧数: " << opus_frames.size() << "\n";

    // ===================== 4. 逐帧解码（使用原始帧长度，100%不损坏） =====================
    std::vector<uint8_t> decoded_pcm;

    for (const auto& frame : opus_frames) {
        const uint8_t* opus_data = frame.first.data();
        const int opus_len = frame.second;

        std::vector<uint8_t> pcm_buf(PCM_BUFFER_SIZE);
        int pcm_len = 0;

        // 核心修复：解码【完整单帧】+【原始长度】
        int ret = decoder.decode((uint8_t*)opus_data, opus_len, pcm_buf.data(), &pcm_len);
        ASSERT_EQ(ret, 0) << "单帧解码失败（已修复帧边界）";

        if (pcm_len > 0) {
            decoded_pcm.insert(decoded_pcm.end(), pcm_buf.data(), pcm_buf.data() + pcm_len);
        }
    }

    ASSERT_GT(decoded_pcm.size(), 0) << "解码PCM为空";
    std::cout << "解码完成 | 输出PCM大小: " << decoded_pcm.size() << " 字节\n";

    // ===================== 5. 播放 =====================
    size_t play_pos = 0;
    bool finish = false;
    AlsaPlayback player(DEFAULT_DEVICE, SAMPLE_RATE, CHANNELS, FORMAT);

    player.start([&](std::vector<uint8_t>& buf, size_t max) -> int {
        if (play_pos >= decoded_pcm.size()) { finish = true; return 0; }
        size_t copy = std::min(max, decoded_pcm.size() - play_pos);
        memcpy(buf.data(), decoded_pcm.data() + play_pos, copy);
        play_pos += copy;
        return (int)copy;
    });

    std::cout << "===== 开始播放 =====\n";
    std::this_thread::sleep_for(std::chrono::seconds(4));
    player.stop();

    ASSERT_TRUE(finish);
    std::cout << "===== 测试PASS！=====\n";
}