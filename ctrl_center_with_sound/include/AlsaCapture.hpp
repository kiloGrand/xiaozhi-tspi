#pragma once

#include "AlsaAudioBase.hpp"  // 复用音频基类
#include <vector>
#include <thread>
#include <atomic>
#include <functional>

namespace AlsaAudio {

/**
 * @brief 录音回调函数类型
 * @param buffer 录音数据缓冲区
 * @param size 数据字节大小
 */
using RecordCallback = std::function<void(std::vector<uint8_t>& buffer, size_t size)>;

/**
 * @brief ALSA 录音类（继承硬件抽象基类）
 * 纯录音功能，无降噪，线程安全，RAII 资源管理
 */
class AlsaCapture final : public AlsaAudioBase {
private:
    RecordCallback m_callback;               // 录音数据回调
    std::atomic<bool> m_is_running{false};   // 线程安全运行标志
    std::thread m_record_thread;             // C++ 标准录音线程
    std::vector<uint8_t> m_audio_buffer;     // 录音数据缓冲区

    /**
     * @brief 录音线程工作函数（核心逻辑）
     */
    void record_worker();

public:
    // 继承基类构造函数
    using AlsaAudioBase::AlsaAudioBase;

    /**
     * @brief 析构函数：自动停止录音，释放所有资源
     */
    ~AlsaCapture() override;

    /**
     * @brief 启动录音
     * @param callback 录音数据回调函数
     * @return 成功 true，失败 false
     */
    bool start(RecordCallback callback);

    /**
     * @brief 停止录音（线程安全）
     */
    void stop() noexcept;

    bool is_running() const noexcept { return m_is_running; }
};

} // namespace AlsaAudio