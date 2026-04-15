#pragma once

#include <alsa/asoundlib.h>
#include <functional>
#include <vector>
#include <thread>
#include <atomic>
#include <string>
#include <cstdint>
#include <memory>

// ALSA音频封装命名空间：隔离全局作用域，避免命名冲突
namespace AlsaAudio {

// ===================== 音频默认配置常量 =====================
// 默认ALSA设备名
constexpr const char* DEFAULT_DEVICE = "default";
// 默认采样率：16000Hz（语音通信标准采样率）
constexpr unsigned int DEFAULT_SAMPLE_RATE = 16000;
// 默认声道数：1（单声道，语音通用）
constexpr unsigned int DEFAULT_CHANNELS = 1;
// 默认音频格式：16位小端有符号数（PCM标准格式）
constexpr snd_pcm_format_t DEFAULT_FORMAT = SND_PCM_FORMAT_S16_LE;

/**
 * @brief ALSA音频操作基类
 * @details 封装ALSA PCM设备通用操作（设备打开、参数配置、资源管理）
 *          作为AlsaCapture/AlsaPlayback的父类，实现代码复用
 *          采用RAII机制管理音频设备资源，禁止拷贝，支持移动
 */
class AlsaAudioBase {
protected:
    // ===================== 类型别名 =====================
    // 智能指针管理PCM设备句柄：自定义删除器为snd_pcm_close，实现RAII自动释放设备
    using PcmHandle = std::unique_ptr<snd_pcm_t, decltype(&snd_pcm_close)>;

    // ===================== 保护成员变量 =====================
    // ALSA PCM设备句柄（智能指针自动管理生命周期）
    PcmHandle m_pcm_handle{nullptr, snd_pcm_close};
    // 音频设备名称（如default/hw:0,0）
    std::string m_device;
    // 音频采样率
    unsigned int m_sample_rate;
    // 音频声道数
    unsigned int m_channels;
    // 音频采样格式
    snd_pcm_format_t m_format;
    // 硬件周期帧大小：ALSA一次传输的最小音频帧数
    snd_pcm_uframes_t m_period_frames;

    /**
     * @brief 通用PCM设备初始化函数（录音/播放共用）
     * @param stream 流类型：SND_PCM_STREAM_PLAYBACK(播放)/SND_PCM_STREAM_CAPTURE(录音)
     * @return 成功返回0，失败返回-1
     * @details 完成设备打开、硬件参数配置、参数应用、实际参数读取全流程
     */
    int open_pcm_device(snd_pcm_stream_t stream);

public:
    /**
     * @brief 基类构造函数
     * @param device 音频设备名
     * @param sample_rate 采样率
     * @param channels 声道数
     * @param format 音频格式
     */
    explicit AlsaAudioBase(std::string device = DEFAULT_DEVICE,
                           unsigned int sample_rate = DEFAULT_SAMPLE_RATE,
                           unsigned int channels = DEFAULT_CHANNELS,
                           snd_pcm_format_t format = DEFAULT_FORMAT) noexcept;

    // 虚析构函数：保证子类析构时能正确调用，多态必备
    virtual ~AlsaAudioBase() = default;

    // ===================== 禁止拷贝语义 =====================
    // 禁止拷贝构造：避免多个对象管理同一个PCM设备，导致资源重复释放
    AlsaAudioBase(const AlsaAudioBase&) = delete;
    // 禁止拷贝赋值：同上
    AlsaAudioBase& operator=(const AlsaAudioBase&) = delete;

    // ===================== 支持移动语义 =====================
    // 移动构造：支持对象转移，不拷贝资源
    AlsaAudioBase(AlsaAudioBase&&) = default;
    // 移动赋值：支持对象转移，不拷贝资源
    AlsaAudioBase& operator=(AlsaAudioBase&&) = default;

    // ===================== 只读Getter接口 =====================
    // 获取采样率（noexcept保证不抛出异常）
    unsigned int get_sample_rate() const noexcept { return m_sample_rate; }
    // 获取声道数
    unsigned int get_channels() const noexcept { return m_channels; }
    // 获取音频格式
    snd_pcm_format_t get_format() const noexcept { return m_format; }
    // 获取周期帧大小
    snd_pcm_uframes_t get_period_frames() const noexcept { return m_period_frames; }

    /**
     * @brief 获取硬件实际生效的音频参数
     * @param sample_rate 输出：实际采样率
     * @param channels 输出：实际声道数
     * @param format 输出：实际音频格式
     * @note 硬件可能不支持配置参数，会自动匹配最接近的参数
     */
    void get_actual_settings(unsigned int& sample_rate,
                             unsigned int& channels,
                             snd_pcm_format_t& format) const noexcept;
};

} // namespace AlsaAudio