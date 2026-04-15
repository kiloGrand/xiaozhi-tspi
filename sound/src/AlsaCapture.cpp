#include "AlsaCapture.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>

namespace AlsaAudio {

// ====================== 录音线程核心函数 ======================
void AlsaCapture::record_worker() {
    // 1. 初始化录音PCM设备
    if (open_pcm_device(SND_PCM_STREAM_CAPTURE) != 0) {
        m_is_running = false;
        return;
    }

    // 2. 计算录音缓冲区大小
    // 获取单帧位宽并转换为字节数
    const size_t frame_size = snd_pcm_format_width(m_format) / 8;
    // 总缓冲区 = 周期帧 * 单帧字节数 * 声道数
    const size_t buffer_size = m_period_frames * frame_size * m_channels;
    // vector自动分配内存，RAII管理
    m_audio_buffer.resize(buffer_size);

    // 3. 打印实际生效的录音配置
    std::cout << "\n===== 录音配置 =====\n"
              << "设备: " << m_device << '\n'
              << "采样率: " << m_sample_rate << " Hz\n"
              << "声道数: " << m_channels << '\n'
              << "格式: " << snd_pcm_format_name(m_format) << '\n'
              << "周期帧: " << m_period_frames << '\n'
              << "缓冲区大小: " << buffer_size << " B\n"
              << "====================\n";

    // 4. 录音主循环
    std::cout << "录音开始...\n";
    while (m_is_running) {
        // 从声卡读取音频数据
        // 每次读取新的一帧，都会从缓冲区起始位置从头写，覆盖上一帧的数据
        snd_pcm_sframes_t rc = snd_pcm_readi(
            m_pcm_handle.get(), 
            m_audio_buffer.data(),   // 起始地址：缓冲区头部
            m_period_frames          // 读取的帧数 = 硬件周期帧
        );

        // 缓冲区溢出错误处理
        if (rc == -EPIPE) {
            std::cerr << "缓冲区溢出，尝试恢复...\n";
            snd_pcm_prepare(m_pcm_handle.get());
            continue;
        }
        // 读取失败，退出循环
        if (rc < 0) {
            std::cerr << "录音读取错误: " << snd_strerror(rc) << '\n';
            break;
        }

        // 计算实际读取的音频数据字节数
        const size_t data_size = rc * frame_size * m_channels;

        // 5. 通过回调函数返回录音数据
        if (m_callback) {
            m_callback(m_audio_buffer, data_size);
        }
    }

    // 线程退出清理
    std::cout << "录音已停止\n";
    m_is_running = false;
}

// ====================== 启动录音 ======================
bool AlsaCapture::start(RecordCallback callback) {
    if (m_is_running) {
        std::cerr << "录音已运行\n";
        return false;
    }

    m_callback = std::move(callback);
    m_is_running = true;

    // 创建并启动录音线程
    m_record_thread = std::thread(&AlsaCapture::record_worker, this);
    return true;
}

// ====================== 停止录音 ======================
void AlsaCapture::stop() noexcept {
    if (!m_is_running)
        return;

    // 线程安全停止
    m_is_running = false;
    if (m_record_thread.joinable()) {
        m_record_thread.join();
    }

    // 释放设备和缓冲区
    m_pcm_handle.reset();
    m_audio_buffer.clear();
}

// ====================== 析构函数 ======================
AlsaCapture::~AlsaCapture() {
    stop();
}

} // namespace AlsaAudio