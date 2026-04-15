#include "AlsaAudioBase.hpp"
#include <iostream>

namespace AlsaAudio {

// 构造函数
AlsaAudioBase::AlsaAudioBase(std::string device,
                             unsigned int sample_rate,
                             unsigned int channels,
                             snd_pcm_format_t format) noexcept
    : m_device(std::move(device)),
      m_sample_rate(sample_rate),
      m_channels(channels),
      m_format(format),
      m_period_frames(0) {}

// 获取实际参数
void AlsaAudioBase::get_actual_settings(unsigned int& sample_rate,
                                        unsigned int& channels,
                                        snd_pcm_format_t& format) const noexcept {
    sample_rate = m_sample_rate;
    channels = m_channels;
    format = m_format;
}

// =====================================================================
// 通用PCM设备初始化：播放/录音 共用这一个函数！
// =====================================================================
/**
 * @brief 打开并初始化ALSA PCM设备（播放/录音），配置硬件参数
 * @param stream PCM流类型：SND_PCM_STREAM_PLAYBACK(播放) / SND_PCM_STREAM_CAPTURE(录音)
 * @return 成功返回0，失败返回-1
 * @note 完成设备打开、参数配置、参数生效、读取实际硬件参数全流程
 */
int AlsaAudioBase::open_pcm_device(snd_pcm_stream_t stream) {
    // 定义原始PCM设备句柄，用于接收ALSA库返回的设备指针
    snd_pcm_t* raw_handle = nullptr;

    // ===================== 1. 打开PCM音频设备 =====================
    // snd_pcm_open：打开ALSA PCM设备
    // 参数1：输出设备句柄；参数2：设备名称；参数3：流类型(播放/录音)；参数4：打开模式(0=阻塞模式)
    int ret = snd_pcm_open(&raw_handle, m_device.c_str(), stream, 0);
    // 打开失败，打印错误信息并返回
    if (ret < 0) {
        std::cerr << "PCM 打开失败: " << snd_strerror(ret) << std::endl;
        return -1;
    }
    // 将裸指针交给std::unique_ptr管理，开启RAII自动释放资源，无需手动调用snd_pcm_close
    m_pcm_handle.reset(raw_handle);

    // ===================== 2. 分配硬件参数结构体 =====================
    // 定义硬件参数对象指针，存储采样率、格式、声道等配置
    snd_pcm_hw_params_t* hw_params = nullptr;
    // 栈上分配硬件参数内存，自动释放，无需手动free
    snd_pcm_hw_params_alloca(&hw_params);

    // ===================== 3. 初始化硬件参数 =====================
    // 将参数初始化为设备支持的所有默认配置，清空脏数据
    snd_pcm_hw_params_any(m_pcm_handle.get(), hw_params);

    // ===================== 4. 配置核心音频硬件参数 =====================
    // 设置数据访问模式：交错模式(L R L R)，多声道标准格式
    ret = snd_pcm_hw_params_set_access(m_pcm_handle.get(), hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    // 设置音频采样格式(如S16_LE 16位小端)
    ret |= snd_pcm_hw_params_set_format(m_pcm_handle.get(), hw_params, m_format);
    // 设置音频声道数(1单声道/2立体声)
    ret |= snd_pcm_hw_params_set_channels(m_pcm_handle.get(), hw_params, m_channels);

    // 设置采样率（自动就近匹配）
    // 参数4：dir=方向标识，传nullptr表示使用默认规则：自动就近匹配硬件支持的采样率
    // dir取值：0=就近、1=向上取、-1=向下取；
    ret |= snd_pcm_hw_params_set_rate_near(m_pcm_handle.get(), hw_params, &m_sample_rate, 0);

    // 任意参数配置失败，释放设备并返回错误
    if (ret < 0) {
        std::cerr << "PCM 参数设置失败: " << snd_strerror(ret) << std::endl;
        m_pcm_handle.reset();
        return -1;
    }

    // ===================== 5. 应用参数到硬件设备 =====================
    // 将配置好的参数写入物理声卡，是参数生效的核心步骤
    ret = snd_pcm_hw_params(m_pcm_handle.get(), hw_params);
    if (ret < 0) {
        std::cerr << "PCM 参数应用失败: " << snd_strerror(ret) << std::endl;
        m_pcm_handle.reset();
        return -1;
    }

    // ===================== 6. 读取硬件实际生效的参数 =====================
    // 获取周期帧大小：硬件一次处理的最小音频帧数
    // 参数3：dir取值：0=就近、1=向上取、-1=向下取，传0自动就近匹配硬件支持的采样率
    snd_pcm_hw_params_get_period_size(hw_params, &m_period_frames, 0);
    // 获取硬件实际使用的采样率（set_rate_near自动匹配后的值）
    snd_pcm_hw_params_get_rate(hw_params, &m_sample_rate, 0);
    // 获取硬件实际使用的声道数
    snd_pcm_hw_params_get_channels(hw_params, &m_channels);
    // 获取硬件实际使用的音频格式
    snd_pcm_hw_params_get_format(hw_params, &m_format);
    
    // 设备初始化完成，返回成功
    return 0;
}

} // namespace AlsaAudio