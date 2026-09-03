#include <iostream>
#include <cstring>
#include <unistd.h>
#include "AlsaPlayback.hpp"

namespace AlsaAudio {
    
/**
 * @brief 音频播放工作线程函数
 * @details 线程核心逻辑：初始化设备 -> 分配缓冲区 -> 循环获取音频数据 -> 写入声卡播放
 * @note 该函数在独立的std::thread中运行，通过m_is_running原子变量控制线程启停
 */
void AlsaPlayback::play_worker() {

    // ===================== 1. 初始化PCM播放设备 =====================
    // 调用设备初始化函数，指定为播放流(SND_PCM_STREAM_PLAYBACK)
    // 初始化失败则设置运行标志为false，直接退出线程
    if (open_pcm_device(SND_PCM_STREAM_PLAYBACK) != 0) {
        m_is_running = false;
        return;
    }

    // ===================== 2. 计算音频缓冲区大小 =====================
    // snd_pcm_format_width：获取音频格式的位宽(bit)，除以8转换为字节数
    // frame_size：单个音频帧(单声道)的字节大小
    const size_t frame_size = snd_pcm_format_width(m_format) / 8;
    // 总缓冲区大小 = 周期帧数 × 单帧字节数 × 声道数
    // 对应硬件一次处理的完整音频数据大小
    const size_t buffer_size = m_period_frames * frame_size * m_channels;
    // 使用std::vector自动分配内存，RAII机制管理，无需手动malloc/free
    m_audio_buffer.resize(buffer_size);

    // ===================== 3. 打印实际生效的播放配置 =====================
    std::cout << "\n===== 播放配置 =====\n"
              << "设备: " << m_device << '\n'
              << "采样率: " << m_sample_rate << " Hz\n"
              << "声道数: " << m_channels << '\n'
              << "格式: " << snd_pcm_format_name(m_format) << '\n'
              << "周期帧: " << m_period_frames << '\n'
              << "缓冲区大小: " << buffer_size << " B\n"
              << "====================\n";

    // ===================== 4. 循环播放音频 =====================
    // 线程运行标志(原子变量，线程安全) && 回调函数有效时，持续播放
    std::cout << "播放开始...\n";
    while (m_is_running && m_callback) {
        // 调用用户注册的回调函数，填充音频数据到缓冲区
        // 支持Lambda/普通函数，参数：音频缓冲区、最大数据长度
        // 返回值：实际填充的字节数
        const int read_size = m_callback(m_audio_buffer, buffer_size);
        // 无有效数据时跳过本次循环
        if (read_size <= 0) continue;

        // ===================== 5. 计算写入声卡的音频帧数 =====================
        // 音频总字节数 → 转换为ALSA需要的帧数(帧 = 所有声道的一个采样点)
        const auto frames = static_cast<snd_pcm_uframes_t>(read_size / frame_size / m_channels);

        // ===================== 6. 将音频数据写入PCM设备播放 =====================
        // snd_pcm_writei：向声卡写入交错模式的音频数据
        // 参数1：PCM设备句柄；参数2：音频数据缓冲区；参数3：要写入的帧数
        int err = snd_pcm_writei(m_pcm_handle.get(), m_audio_buffer.data(), frames);

        // ===================== 7. 播放错误处理与恢复 =====================
        if (err < 0) {
            std::cerr << "播放错误: " << snd_strerror(err) << "，尝试恢复\n";
            // snd_pcm_prepare：恢复PCM设备到准备状态，修复缓冲区溢出/下溢等错误
            snd_pcm_prepare(m_pcm_handle.get());
        }
    }

    // ===================== 8. 线程退出清理 =====================
    std::cout << "播放已停止\n";
    // 原子变量标记线程停止
    m_is_running = false;
}

bool AlsaPlayback::start(PlayCallback callback) {
    if (m_is_running) {
        std::cerr << "播放器已运行\n";
        return false;
    }

    m_callback = std::move(callback);
    m_is_running = true;

    // 启动 C++ 线程（绑定 this 指针，无需静态函数）
    m_play_thread = std::thread(&AlsaPlayback::play_worker, this);
    return true;
}

void AlsaPlayback::stop() noexcept {
    if (!m_is_running) return;

    // 线程安全停止
    m_is_running = false;
    if (m_play_thread.joinable()) {
        m_play_thread.join(); // 等待线程退出
    }

    // 智能指针自动释放 PCM 设备
    m_pcm_handle.reset();
    m_audio_buffer.clear();
}

// 析构函数自动调用 stop，绝对安全
AlsaPlayback::~AlsaPlayback() {
    stop();
}

} // namespace AlsaAudio