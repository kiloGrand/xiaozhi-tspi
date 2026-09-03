#include "OpusWrapper.hpp"
#include <iostream>
#include <vector>
#include <cstring>
#include <algorithm>

// ====================== 编码器 ======================
OpusEncoder::OpusEncoder(uint32_t in_rate, uint32_t in_ch, uint32_t duration_ms,
                         uint32_t out_rate, uint32_t out_ch)
    : m_in_rate(in_rate), m_in_ch(in_ch), m_duration(duration_ms),
      m_out_rate(out_rate), m_out_ch(out_ch)
{
    int err = 0;
    // 初始化重采样器（智能指针自动管理）
    m_resampler.reset(speex_resampler_init(
        m_out_ch, m_in_rate, m_out_rate,
        SPEEX_RESAMPLER_QUALITY_DEFAULT, &err
    ));
    if (err != RESAMPLER_ERR_SUCCESS) {
        m_resampler.reset();
        std::cerr << "重采样器初始化失败\n";
        return;
    }

    // 初始化解码器
    m_encoder.reset(opus_encoder_create(
        m_out_rate, m_out_ch, OPUS_APPLICATION_AUDIO, &err
    ));
    if (err != OPUS_OK) {
        m_resampler.reset();
        m_encoder.reset();
        std::cerr << "编码器初始化失败: " << opus_strerror(err) << "\n";
        return;
    }

    opus_encoder_ctl(m_encoder.get(), OPUS_SET_BITRATE(64000));
}

bool OpusEncoder::isValid() const {
    return m_resampler && m_encoder;
}

int OpusEncoder::encode(unsigned char* pcmdata, int pcmsize, unsigned char* opusdata, int* opussize)
{
    // 空指针校验
    if (!m_encoder || !m_resampler || !pcmdata || !opusdata || !opussize) {
        std::cerr << "编码器未初始化或参数为空" << std::endl;
        return -1;
    }

    const int frame_src = m_in_rate  * m_duration / 1000;
    const int frame_dst = m_out_rate * m_duration / 1000;

    std::vector<opus_int16> rawFrame(frame_src * m_in_ch);
    std::vector<opus_int16> pcmFrame(frame_src * m_in_ch);
    std::vector<opus_int16> resampledFrame(frame_dst * m_out_ch);
    std::vector<unsigned char> opusFrame(4000);

    int frameCount = 0;
    size_t totalBytesRead = 0;
    int totalEncodedBytes = 0;

    while (totalBytesRead < (size_t)pcmsize)
    {
        size_t bytesRead = std::min(
            (size_t)frame_src * m_in_ch * sizeof(opus_int16),
            (size_t)pcmsize - totalBytesRead
        );
        memcpy(rawFrame.data(), pcmdata + totalBytesRead, bytesRead);
        totalBytesRead += bytesRead;

        // 不完整帧补0
        if (bytesRead < rawFrame.size() * sizeof(opus_int16)) {
            size_t samplesRead = bytesRead / sizeof(opus_int16);
            std::fill(rawFrame.begin() + samplesRead, rawFrame.end(), 0);
        }

        // 通道转换
        if (m_out_ch == 1 && m_in_ch > 1) {
            // 多声道转单声道
            for (int i = 0; i < frame_src; ++i) {
                opus_int32 sum = 0;
                for (uint32_t c = 0; c < m_in_ch; ++c) sum += rawFrame[i * m_in_ch + c];
                pcmFrame[i] = sum / m_in_ch;
            }
        } else if (m_out_ch == m_in_ch) {
            // 通道数相同，直接使用原始数据
            memcpy(pcmFrame.data(), rawFrame.data(), rawFrame.size() * sizeof(opus_int16));
        } else {
            // 通道数不同且不为单声道，需要进行通道数转换
            // 这里简单地将每个通道的数据复制到目标通道
            // 实际应用中可能需要更复杂的通道映射
            for (int i = 0; i < frame_src; ++i)
                for (uint32_t c = 0; c < m_out_ch; ++c)
                    pcmFrame[i * m_out_ch + c] = rawFrame[i * m_in_ch + (c % m_in_ch)];
        }

        // ===================== 重采样+错误处理 =====================
        spx_uint32_t in_len = frame_src, out_len = frame_dst;
        int resampleErr = speex_resampler_process_int(
            m_resampler.get(), 0, pcmFrame.data(), &in_len, resampledFrame.data(), &out_len
        );
        if (resampleErr != RESAMPLER_ERR_SUCCESS) {
            std::cerr << "重采样失败: " << resampleErr << std::endl;
            continue;
        }
        if (in_len != frame_src || out_len != frame_dst) {
            std::cerr << "重采样样本数不匹配" << std::endl;
            continue;
        }

        // ===================== 编码+错误处理 =====================
        int encodedBytes = opus_encode(
            m_encoder.get(), resampledFrame.data(), frame_dst, opusFrame.data(), opusFrame.size()
        );
        if (encodedBytes < 0) {
            std::cerr << "帧 " << frameCount << " 编码失败: " << opus_strerror(encodedBytes) << std::endl;
            continue;
        }

        memcpy(opusdata + totalEncodedBytes, opusFrame.data(), encodedBytes);
        totalEncodedBytes += encodedBytes;
        frameCount++;
    }

    *opussize = totalEncodedBytes;
    return frameCount * frame_dst;
}

// ====================== 解码器 ======================
OpusDecoder::OpusDecoder(int in_rate, int in_ch, int duration_ms,
                         int out_rate, int out_ch)
    : m_in_rate(in_rate), m_in_ch(in_ch), m_duration(duration_ms),
      m_out_rate(out_rate), m_out_ch(out_ch)
{
    int err = 0;
    m_resampler.reset(speex_resampler_init(
        m_in_ch, m_in_rate, m_out_rate,
        SPEEX_RESAMPLER_QUALITY_DEFAULT, &err
    ));
    if (err != RESAMPLER_ERR_SUCCESS) {
        m_resampler.reset();
        std::cerr << "重采样器初始化失败\n";
        return;
    }

    m_decoder.reset(opus_decoder_create(m_in_rate, m_in_ch, &err));
    if (err != OPUS_OK) {
        m_resampler.reset();
        m_decoder.reset();
        std::cerr << "解码器初始化失败: " << opus_strerror(err) << "\n";
        return;
    }
}

bool OpusDecoder::isValid() const {
    return m_resampler && m_decoder;
}

int OpusDecoder::decode(unsigned char* opusdata, int opussize, unsigned char* pcmdata, int* pcmsize)
{
    if (!m_decoder || !m_resampler || !opusdata || !pcmdata || !pcmsize) {
        std::cerr << "参数为空或解码器未初始化\n";
        return -1;
    }

    // ===================== 修复1：60ms帧，计算正确的最大采样数 =====================
    // 输入采样率(编码率)的60ms最大采样数 = 16000 * 60 / 1000 = 960
    const int maxFrameSize = m_in_rate * m_duration / 1000;
    // 目标输出采样率的60ms采样数
    const int targetFrameSize = m_out_rate * m_duration / 1000;

    // ===================== 修复2：正确计算PCM缓冲区大小 =====================
    int maxPcmSize = maxFrameSize * m_in_ch; // 单帧最大采样数
    std::vector<opus_int16> pcmFrame(maxPcmSize);

    // 输出重采样后缓冲区
    std::vector<opus_int16> resampledFrame(targetFrameSize * m_out_ch);

    // ===================== 修复3：移除错误循环！Opus一帧就是一个完整数据包 =====================
    // 直接解码完整的Opus帧
    int decodedSamples = opus_decode(
        m_decoder.get(), 
        opusdata,      // 完整的Opus数据包
        opussize,      // 完整的Opus数据包大小
        pcmFrame.data(), 
        maxFrameSize,  // 正确的60ms最大采样数
        0
    );
    if (decodedSamples < 0) {
        std::cerr << "帧解码失败: " << opus_strerror(decodedSamples) << std::endl;
        return -1;
    }

    // 重采样
    spx_uint32_t in_len = decodedSamples;
    spx_uint32_t out_len = targetFrameSize;
    int resampleErr = speex_resampler_process_int(
        m_resampler.get(), 0, 
        pcmFrame.data(), &in_len, 
        resampledFrame.data(), &out_len
    );
    if (resampleErr != RESAMPLER_ERR_SUCCESS) {
        std::cerr << "重采样失败: " << resampleErr << std::endl;
        return -1;
    }

    // 通道转换（保持原有逻辑）
    std::vector<opus_int16> finalPcmFrame(targetFrameSize * m_out_ch);
    if (m_out_ch == 1 && m_in_ch > 1) {
        for (int i = 0; i < targetFrameSize; ++i) {
            opus_int32 sum = 0;
            for (int c = 0; c < m_in_ch; ++c) sum += resampledFrame[i * m_in_ch + c];
            finalPcmFrame[i] = sum / m_in_ch;
        }
    } else if (m_out_ch == m_in_ch) {
        memcpy(finalPcmFrame.data(), resampledFrame.data(), targetFrameSize * m_out_ch * sizeof(opus_int16));
    } else {
        for (int i = 0; i < targetFrameSize; ++i) {
            for (int c = 0; c < m_out_ch; ++c) {
                finalPcmFrame[i * m_out_ch + c] = resampledFrame[i * m_in_ch + (c % m_in_ch)];
            }
        }
    }

    // 输出PCM数据
    int finalPcmBytes = targetFrameSize * m_out_ch * sizeof(opus_int16);
    memcpy(pcmdata, finalPcmFrame.data(), finalPcmBytes);
    *pcmsize = finalPcmBytes;

    return 0;
}