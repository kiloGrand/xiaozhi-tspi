// SPDX-License-Identifier: GPL-3.0-only
#include <iostream>
#include <vector>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <functional>

#include "AlsaCapture.hpp"
#include "AlsaPlayback.hpp"
#include "OpusWrapper.hpp"
#include "udp_endpoint.h"

using namespace AlsaAudio;
using namespace std;

// ===================== 配置常量 =====================
#define AUDIO_PORT_UP      5676
#define AUDIO_PORT_DOWN    5677
#define OPUS_FRAME_MS      60
#define PCM_BUFFER_SIZE    (1024*30)
#define OPUS_BUFFER_SIZE   (1024*5)
#define SAMPLE_RATE_REC    16000
#define SAMPLE_RATE_PLAY   24000
#define CHANNELS           1
#define FORMAT             SND_PCM_FORMAT_S16_LE

// 全局退出标志
static atomic<bool> g_exit_flag(false);

int main() {
    // 注册信号
    signal(SIGINT, [](int sig) {
        cout << "\n📶 收到退出信号，程序退出" << endl;
        g_exit_flag = true;
    });

    // ===================== 完全按照测试代码：局部对象 + 本地变量 =====================
    UdpEndpoint udp(AUDIO_PORT_DOWN, AUDIO_PORT_UP);
    AlsaCapture capture(DEFAULT_DEVICE, SAMPLE_RATE_REC, CHANNELS, FORMAT);
    AlsaPlayback player(DEFAULT_DEVICE, SAMPLE_RATE_PLAY, CHANNELS, FORMAT);

    // 编解码器
    OpusEncoder encoder(SAMPLE_RATE_REC, CHANNELS, OPUS_FRAME_MS, SAMPLE_RATE_REC, CHANNELS);
    OpusDecoder decoder(SAMPLE_RATE_PLAY, CHANNELS, OPUS_FRAME_MS, SAMPLE_RATE_PLAY, CHANNELS);

    // 本地缓冲（和测试代码一致，无this指针！）
    vector<uint8_t> record_buffer(PCM_BUFFER_SIZE);
    size_t record_offset = 0;
    const size_t frame_60ms_bytes = SAMPLE_RATE_REC * OPUS_FRAME_MS / 1000 * CHANNELS * sizeof(opus_int16);
    
    vector<uint8_t> play_buffer(PCM_BUFFER_SIZE);
    size_t play_offset = 0;

    // ===================== 录音回调 =====================
    if (!capture.start([&](vector<uint8_t>& buffer, size_t size) {
        if (!capture.is_running()) return;

        // 缓冲区溢出
        if (record_offset + size > record_buffer.size()) return;
        
        // 拷贝录音数据buffer 到 record_buffer
        memcpy(record_buffer.data() + record_offset, buffer.data(), size);
        record_offset += size;

        // 大于 60ms，压缩这一帧的录音数据为 opus，然后通过 udp 发送出去
        if (record_offset >= frame_60ms_bytes) {        
            size_t i = 0;
            while (i < record_offset) {
                size_t pcmsize = frame_60ms_bytes;
                if (i + pcmsize > record_offset) break;

                vector<uint8_t> opus_buf(OPUS_BUFFER_SIZE);
                int opus_len = 0;
                encoder.encode(record_buffer.data() + i, (int)pcmsize, opus_buf.data(), &opus_len);

                if (opus_len > 0) udp.send(opus_buf.data(), opus_len);
                i += pcmsize;
            }
            record_offset -= i;
        }
    })) {
        cerr << "❌ 录音启动失败" << endl;
        return -1;
    }

    // ===================== 播放回调  =====================
    if (!player.start([&](vector<uint8_t>& buffer, size_t max_size) -> int {
 
        if (!player.is_running())
        {
            return 0;
        }
        
        while (play_offset < max_size && player.is_running()) {
            vector<uint8_t> opus_buf(OPUS_BUFFER_SIZE);
            int opus_data_size = udp.recv(opus_buf.data(), opus_buf.size());

            if (opus_data_size <= 0) {
                // 如果没有接收到数据，等待一段时间后重试
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            int pcm_data_size = 0;
            decoder.decode(opus_buf.data(), opus_data_size, play_buffer.data() + play_offset, &pcm_data_size);
            play_offset += pcm_data_size;
        }

        if (player.is_running())
        {
            // 复制到播放缓冲区
            memcpy(buffer.data(), play_buffer.data(), max_size);

            // 滑动缓冲：剩余数据移到开头（修复卡顿）
            if (play_offset > max_size) {
                // 滑动缓冲：剩余数据移到开头（修复卡顿）
                memmove(play_buffer.data(), play_buffer.data() + max_size, play_offset - max_size);
                // 更新偏移量
                play_offset -= max_size;
            } else {
                // 没有剩余数据，直接清零
                play_offset = 0;
            }

            return (int)max_size;
        } else {
            return 0;
        }
        
    })) {
        capture.stop();
        cerr << "❌ 播放启动失败" << endl;
        return -1;
    }

    cout << "✅ 服务启动成功" << endl;

    // ===================== 主线程等待（和测试代码一致，安全等待） =====================
    while (!g_exit_flag) {
        sleep(1);
    }

    // ===================== 核心：先stop线程(join)，再销毁对象（完全对齐测试代码！） =====================
    capture.stop();
    player.stop();

    cout << "👋 程序安全退出！" << endl;
    return 0;
}