#pragma once

#include <alsa/asoundlib.h>
#include <functional>
#include <vector>
#include <thread>
#include <atomic>
#include <string>
#include <cstdint>

#include "AlsaAudioBase.hpp"

// 命名空间封装，避免命名冲突
namespace AlsaAudio {

// 音频回调类型（std::function 替代 C 函数指针，支持 Lambda）
using PlayCallback = std::function<int(std::vector<uint8_t>& buffer, size_t max_size)>;

/**
 * @brief 功能模块层：ALSA 播放器（继承基类，override 重写）
 */
class AlsaPlayback final : public AlsaAudioBase {
private:
    PlayCallback m_callback;               // 回调函数
    std::atomic<bool> m_is_running{false}; // 原子布尔：线程安全运行标志
    std::thread m_play_thread;             // C++ 标准线程（替代 pthread）
    std::vector<uint8_t> m_audio_buffer;   // 自动管理缓冲区（替代 malloc）

    // 线程工作函数（私有，非静态）
    void play_worker();

public:
    // 继承基类构造函数（C++11 using 继承）
    using AlsaAudioBase::AlsaAudioBase;

    // 析构函数：自动停止线程、释放资源
    ~AlsaPlayback() override;

    /**
     * @brief 启动播放（传入 Lambda/普通函数均可）
     * @param callback 音频数据回调
     * @return 成功 true，失败 false
     */
    bool start(PlayCallback callback);

    /**
     * @brief 停止播放（线程安全）
     */
    void stop() noexcept;

    bool is_running() const noexcept { return m_is_running; }
};

} // namespace AlsaAudio