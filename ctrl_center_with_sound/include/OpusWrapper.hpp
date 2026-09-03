#pragma once

#include <memory>
#include <cstdint>
#include <opus/opus.h>
#include <speex/speex_resampler.h>

// ====================== 编码器（极简） ======================
class OpusEncoder {
public:
    // 构造函数直接初始化
    OpusEncoder(uint32_t in_rate, uint32_t in_ch, uint32_t duration_ms,
                uint32_t out_rate, uint32_t out_ch);

    // 禁用拷贝，支持移动
    OpusEncoder(const OpusEncoder&) = delete;
    OpusEncoder& operator=(const OpusEncoder&) = delete;
    OpusEncoder(OpusEncoder&&) = default;
    OpusEncoder& operator=(OpusEncoder&&) = default;

    ~OpusEncoder() = default;

    bool isValid() const;
    int encode(uint8_t* pcmdata, int pcmsize, uint8_t* opusdata, int* opussize);

private:
    // 重采样器指针 + 自动推导 speex_resampler_destroy 类型
    std::unique_ptr<SpeexResamplerState, decltype(&speex_resampler_destroy)> m_resampler{nullptr, speex_resampler_destroy};
    // 编码器指针 + 自动推导 opus_encoder_destroy 类型
    std::unique_ptr<OpusEncoder, decltype(&opus_encoder_destroy)> m_encoder{nullptr, opus_encoder_destroy};

    uint32_t m_in_rate  = 0;
    uint32_t m_in_ch    = 0;
    uint32_t m_out_rate = 0;
    uint32_t m_out_ch   = 0;
    uint32_t m_duration = 0;
};

// ====================== 解码器（极简） ======================
class OpusDecoder {
public:
    OpusDecoder(int in_rate, int in_ch, int duration_ms,
                int out_rate, int out_ch);

    OpusDecoder(const OpusDecoder&) = delete;
    OpusDecoder& operator=(const OpusDecoder&) = delete;
    OpusDecoder(OpusDecoder&&) = default;
    OpusDecoder& operator=(OpusDecoder&&) = default;

    ~OpusDecoder() = default;

    bool isValid() const;
    int decode(uint8_t* opusdata, int opussize, uint8_t* pcmdata, int* pcmsize);

private:
    // decltype 自动推导销毁函数
    std::unique_ptr<SpeexResamplerState, decltype(&speex_resampler_destroy)> m_resampler{nullptr, speex_resampler_destroy};
    std::unique_ptr<OpusDecoder, decltype(&opus_decoder_destroy)> m_decoder{nullptr, opus_decoder_destroy};

    int m_in_rate  = 0;
    int m_in_ch    = 0;
    int m_out_rate = 0;
    int m_out_ch   = 0;
    int m_duration = 0;
};