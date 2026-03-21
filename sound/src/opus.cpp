// SPDX-License-Identifier: GPL-3.0-only
/*
 * Copyright (c) 2008-2023 100askTeam : Dongshan WEI <weidongshan@100ask.net> 
 * Discourse:  https://forums.100ask.net
 */
 
/*  Copyright (C) 2008-2023 深圳百问网科技有限公司
 *  All rights reserved
 *
 * 免责声明: 百问网编写的文档, 仅供学员学习使用, 可以转发或引用(请保留作者信息),禁止用于商业用途！
 * 免责声明: 百问网编写的程序, 用于商业用途请遵循GPL许可, 百问网不承担任何后果！
 * 
 * 本程序遵循GPL V3协议, 请遵循协议
 * 百问网学习平台   : https://www.100ask.net
 * 百问网交流社区   : https://forums.100ask.net
 * 百问网官方B站    : https://space.bilibili.com/275908810
 * 本程序所用开发板 : Linux开发板
 * 百问网官方淘宝   : https://100ask.taobao.com
 * 联系我们(E-mail) : weidongshan@100ask.net
 *
 *          版权所有，盗版必究。
 *  
 * 修改历史     版本号           作者        修改内容
 *-----------------------------------------------------
 * 2025.03.20      v01         百问科技      创建文件
 *-----------------------------------------------------
 */
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <opus/opus.h>
#include <speex/speex_resampler.h>  // 新增重采样头文件

typedef struct opus_encoder {
    unsigned int inputSampleRate;
    unsigned int inputChannels;
    unsigned int outputSampleRate;
    unsigned int outputChannels;
    unsigned int duration_ms;
    SpeexResamplerState* resampler;
    OpusEncoder* encoder;
} opus_encoder;

typedef struct opus_decoder {
    int inputSampleRate;
    int inputChannels;
    int outputSampleRate;
    int outputChannels;
    int duration_ms;
    SpeexResamplerState* resampler;
    OpusDecoder* decoder;
} opus_decoder;

static opus_encoder g_opus_encoder;
static opus_decoder g_opus_decoder;

int init_opus_encoder(unsigned int inputSampleRate, unsigned int inputChannels, unsigned int duration_ms, 
                     unsigned int outputSampleRate, unsigned int outputChannels) {
    // 设置全局配置结构体
    g_opus_encoder.inputSampleRate = inputSampleRate;
    g_opus_encoder.inputChannels = inputChannels;
    g_opus_encoder.duration_ms = duration_ms;
    g_opus_encoder.outputSampleRate = outputSampleRate;
    g_opus_encoder.outputChannels = outputChannels;

    int resampleErr;
    g_opus_encoder.resampler = speex_resampler_init(
        g_opus_encoder.outputChannels,
        g_opus_encoder.inputSampleRate,
        g_opus_encoder.outputSampleRate,
        SPEEX_RESAMPLER_QUALITY_DEFAULT,
        &resampleErr
    );

    if (resampleErr != RESAMPLER_ERR_SUCCESS) {
        std::cerr << "重采样器初始化失败 for encoder: " << resampleErr << std::endl;
        return -1;
    }

    int opusError;
    g_opus_encoder.encoder = opus_encoder_create(
        g_opus_encoder.outputSampleRate,
        g_opus_encoder.outputChannels,
        OPUS_APPLICATION_AUDIO,
        &opusError
    );
    if (opusError != OPUS_OK) {
        std::cerr << "编码器初始化失败: " << opus_strerror(opusError) << std::endl;
        speex_resampler_destroy(g_opus_encoder.resampler);
        return -1;
    }
    opus_encoder_ctl(g_opus_encoder.encoder, OPUS_SET_BITRATE(64000));

    return 0;
}

int init_opus_decoder(int inputSampleRate, int inputChannels, int duration_ms, 
                       int outputSampleRate, int outputChannels) {
    // 设置全局配置结构体
    g_opus_decoder.inputSampleRate = inputSampleRate;
    g_opus_decoder.inputChannels = inputChannels;
    g_opus_decoder.duration_ms = duration_ms;
    g_opus_decoder.outputSampleRate = outputSampleRate;
    g_opus_decoder.outputChannels = outputChannels;

    int resampleErr;
    g_opus_decoder.resampler = speex_resampler_init(
        g_opus_decoder.inputChannels,
        g_opus_decoder.inputSampleRate,
        g_opus_decoder.outputSampleRate,
        SPEEX_RESAMPLER_QUALITY_DEFAULT,
        &resampleErr
    );

    if (resampleErr != RESAMPLER_ERR_SUCCESS) {
        std::cerr << "重采样器初始化失败 for decoder: " << resampleErr <<" inputSampleRate "<< g_opus_decoder.inputSampleRate << "  "<< g_opus_decoder.outputSampleRate << "inputChannels "<< g_opus_decoder.inputChannels<< std::endl;
        return -1;
    }

    // 初始化 Opus 解码器
    int error;
    OpusDecoder* decoder = opus_decoder_create(g_opus_decoder.inputSampleRate, g_opus_decoder.inputChannels, &error);
    if (error != OPUS_OK) {
        std::cerr << "解码器初始化失败: " << opus_strerror(error) << std::endl;
        return -1;
    }    
    g_opus_decoder.decoder = decoder;
    return 0;
}

int pcm2opus(unsigned char* pcmdata, int pcmsize, unsigned char* opusdata, int* opussize) {
    // 使用全局配置结构体中的参数
    int sampleRate = g_opus_encoder.inputSampleRate;
    int inputChannels = g_opus_encoder.inputChannels;
    int outsampleRate = g_opus_encoder.outputSampleRate;
    int outputChannels = g_opus_encoder.outputChannels;

    const int originalFrameSize = sampleRate * g_opus_encoder.duration_ms / 1000;  // 原始帧大小
    const int targetFrameSize = outsampleRate * g_opus_encoder.duration_ms / 1000; // 目标帧大小

    // 输入缓冲区（多声道）
    std::vector<opus_int16> rawFrame(originalFrameSize * inputChannels);
    // 中间缓冲区（多声道原始采样率）
    std::vector<opus_int16> pcmFrame(originalFrameSize * inputChannels);
    // 输出缓冲区（多声道目标采样率）
    std::vector<opus_int16> resampledFrame(targetFrameSize * outputChannels);

    // Opus 数据缓冲区
    std::vector<unsigned char> opusFrame(4000);

    // 逐帧处理
    int frameCount = 0;
    size_t totalBytesRead = 0;
    int totalEncodedBytes = 0; // 用于累加编码后的总字节数
    while (totalBytesRead < pcmsize) {
        // 读取原始 PCM 数据
        size_t bytesRead = std::min(static_cast<size_t>(originalFrameSize * inputChannels * sizeof(opus_int16)), static_cast<size_t>(pcmsize - totalBytesRead));
        //printf("bytesRead=%d,originalFrameSize=%d,pcmsize=%d\n", bytesRead, originalFrameSize, pcmsize);
        //printf("%s %d\n", __FUNCTION__, __LINE__);
        memcpy(rawFrame.data(), pcmdata + totalBytesRead, bytesRead);
        totalBytesRead += bytesRead;

        //printf("%s %d\n", __FUNCTION__, __LINE__);

        // 处理不完整帧
        if (bytesRead < rawFrame.size() * sizeof(opus_int16)) {
            size_t samplesRead = bytesRead / sizeof(opus_int16);
            std::fill(rawFrame.begin() + samplesRead, rawFrame.end(), 0);
        }

        //printf("%s %d\n", __FUNCTION__, __LINE__);

        // 根据目标通道数处理音频数据
        if (outputChannels == 1 && inputChannels > 1) {
            // 多声道转单声道
            for (int i = 0; i < originalFrameSize; ++i) {
                opus_int32 sum = 0;
                for (int c = 0; c < inputChannels; ++c) {
                    sum += rawFrame[i * inputChannels + c];
                }
                pcmFrame[i] = static_cast<opus_int16>(sum / inputChannels);
            }
        } else if (outputChannels == inputChannels) {
            // 通道数相同，直接使用原始数据
            memcpy(pcmFrame.data(), rawFrame.data(), originalFrameSize * inputChannels * sizeof(opus_int16));
        } else {
            // 通道数不同且不为单声道，需要进行通道数转换
            // 这里简单地将每个通道的数据复制到目标通道
            // 实际应用中可能需要更复杂的通道映射
            for (int i = 0; i < originalFrameSize; ++i) {
                for (int c = 0; c < outputChannels; ++c) {
                    pcmFrame[i * outputChannels + c] = rawFrame[i * inputChannels + (c % inputChannels)];
                }
            }
        }
        //printf("%s %d\n", __FUNCTION__, __LINE__);
        // 执行重采样
        spx_uint32_t in_len = originalFrameSize;
        spx_uint32_t out_len = targetFrameSize;
        int resampleErr = speex_resampler_process_int(
            g_opus_encoder.resampler,
            0,
            pcmFrame.data(),
            &in_len,
            resampledFrame.data(),
            &out_len
        );
        //printf("%s %d\n", __FUNCTION__, __LINE__);
        if (resampleErr != RESAMPLER_ERR_SUCCESS) {
            std::cerr << "重采样失败: " << resampleErr << std::endl;
            continue;
        }
        //printf("%s %d\n", __FUNCTION__, __LINE__);
        // 检查重采样结果
        if (in_len != originalFrameSize || out_len != targetFrameSize) {
            std::cerr << "重采样样本数不匹配" << std::endl;
            continue;
        }
        //printf("%s %d\n", __FUNCTION__, __LINE__);
        // 编码 Opus 帧
        int encodedBytes = opus_encode(
            g_opus_encoder.encoder,
            resampledFrame.data(),
            targetFrameSize,
            opusFrame.data(),
            opusFrame.size()
        );
        //printf("%s %d\n", __FUNCTION__, __LINE__);
        if (encodedBytes < 0) {
            std::cerr << "帧 " << frameCount << " 编码失败: " << opus_strerror(encodedBytes) << std::endl;
            continue;
        }
        //printf("%s %d\n", __FUNCTION__, __LINE__);
        // 检查 Opus 数据缓冲区是否足够
        //if (totalEncodedBytes + encodedBytes > *opussize) {
        //    std::cerr << "Opus 数据缓冲区不足" << std::endl;
        //    break;
        //}

        // 将编码后的 Opus 数据复制到 opusdata 缓冲区
        //printf("totalEncodedBytes = %d\n", totalEncodedBytes);
        memcpy(opusdata + totalEncodedBytes, opusFrame.data(), encodedBytes);
        totalEncodedBytes += encodedBytes;
        //printf("%s %d\n", __FUNCTION__, __LINE__);
        frameCount++;
    }

    // 更新 opussize 为实际编码的数据大小
    *opussize = totalEncodedBytes;

    return frameCount * targetFrameSize;
}

int opus2pcm(unsigned char* opusdata, int opussize, unsigned char* pcmdata, int *pcmsize) {
    // 计算最大可能的 PCM 数据大小
    int maxFrameSize = 480; // Opus 最大帧大小为 120 ms，假设 48 kHz 采样率
    int maxPcmSize = maxFrameSize * g_opus_decoder.inputChannels * sizeof(opus_int16);
    std::vector<opus_int16> pcmFrame(maxPcmSize);

    // 计算目标 PCM 数据大小
    int targetFrameSize = g_opus_decoder.outputSampleRate * g_opus_decoder.duration_ms / 1000;
    int targetPcmSize = targetFrameSize * g_opus_decoder.outputChannels * sizeof(opus_int16);
    std::vector<opus_int16> resampledFrame(targetPcmSize);

    // 逐帧解码
    int totalBytesRead = 0;
    int totalPcmBytes = 0;
    while (totalBytesRead < opussize) {
        // 计算当前帧的大小
        size_t frameSize = std::min(static_cast<size_t>(maxFrameSize * sizeof(opus_int16) * g_opus_decoder.inputChannels), static_cast<size_t>(opussize - totalBytesRead));

        // 解码 Opus 帧
        int decodedSamples = opus_decode(g_opus_decoder.decoder, opusdata + totalBytesRead, frameSize, pcmFrame.data(), maxPcmSize, 0);
        if (decodedSamples < 0) {
            std::cerr << "帧 " << totalBytesRead / frameSize + 1 << " 解码失败: " << opus_strerror(decodedSamples) << std::endl;
            return -1;
        }

        // 计算解码后的 PCM 数据大小
        int decodedBytes = decodedSamples * sizeof(opus_int16) * g_opus_decoder.inputChannels;

        // 执行重采样
        spx_uint32_t in_len = decodedSamples;
        spx_uint32_t out_len = targetFrameSize;
        int resampleErr = speex_resampler_process_int(
            g_opus_decoder.resampler,
            0,
            pcmFrame.data(),
            &in_len,
            resampledFrame.data(),
            &out_len
        );

        if (resampleErr != RESAMPLER_ERR_SUCCESS) {
            std::cerr << "重采样失败: " << resampleErr << std::endl;
            return -1;
        }

        // 检查重采样结果
        if (in_len != decodedSamples || out_len != targetFrameSize) {
            std::cerr << "重采样样本数不匹配" << std::endl;
            return -1;
        }

        // 处理通道数不同的情况
        std::vector<opus_int16> finalPcmFrame(targetFrameSize * g_opus_decoder.outputChannels);
        if (g_opus_decoder.outputChannels == 1 && g_opus_decoder.inputChannels > 1) {
            // 多声道转单声道
            for (int i = 0; i < targetFrameSize; ++i) {
                opus_int32 sum = 0;
                for (int c = 0; c < g_opus_decoder.inputChannels; ++c) {
                    sum += resampledFrame[i * g_opus_decoder.inputChannels + c];
                }
                finalPcmFrame[i] = static_cast<opus_int16>(sum / g_opus_decoder.inputChannels);
            }
        } else if (g_opus_decoder.outputChannels == g_opus_decoder.inputChannels) {
            // 通道数相同，直接使用重采样后的数据
            memcpy(finalPcmFrame.data(), resampledFrame.data(), targetFrameSize * g_opus_decoder.outputChannels * sizeof(opus_int16));
        } else {
            // 通道数不同且不为单声道，需要进行通道数转换
            // 这里简单地将每个通道的数据复制到目标通道
            // 实际应用中可能需要更复杂的通道映射
            for (int i = 0; i < targetFrameSize; ++i) {
                for (int c = 0; c < g_opus_decoder.outputChannels; ++c) {
                    finalPcmFrame[i * g_opus_decoder.outputChannels + c] = resampledFrame[i * g_opus_decoder.inputChannels + (c % g_opus_decoder.inputChannels)];
                }
            }
        }

        // 计算最终 PCM 数据大小
        int finalPcmBytes = targetFrameSize * g_opus_decoder.outputChannels * sizeof(opus_int16);

        // 检查 PCM 数据缓冲区是否足够
        //if (totalPcmBytes + finalPcmBytes > *pcmsize) {
        //    std::cerr << "PCM 数据缓冲区不足" << std::endl;
        //    return -1;
        //}

        // 将处理后的 PCM 数据复制到 pcmdata 缓冲区
        memcpy(pcmdata + totalPcmBytes, finalPcmFrame.data(), finalPcmBytes);
        totalPcmBytes += finalPcmBytes;
        totalBytesRead += frameSize;
    }

    // 更新 pcmsize 为实际解码的数据大小
    *pcmsize = totalPcmBytes;

    return 0;
}

// ========== 新增：资源释放函数实现 ==========
void free_opus_encoder(void) {
    // 销毁重采样器
    if (g_opus_encoder.resampler) {
        speex_resampler_destroy(g_opus_encoder.resampler);
        g_opus_encoder.resampler = nullptr; // 置空避免重复销毁
    }
    // 销毁编码器
    if (g_opus_encoder.encoder) {
        opus_encoder_destroy(g_opus_encoder.encoder);
        g_opus_encoder.encoder = nullptr; // 置空避免重复销毁
    }
    // 重置全局编码器参数（可选，增强安全性）
    g_opus_encoder.inputSampleRate = 0;
    g_opus_encoder.inputChannels = 0;
    g_opus_encoder.outputSampleRate = 0;
    g_opus_encoder.outputChannels = 0;
    g_opus_encoder.duration_ms = 0;
}

void free_opus_decoder(void) {
    // 销毁重采样器
    if (g_opus_decoder.resampler) {
        speex_resampler_destroy(g_opus_decoder.resampler);
        g_opus_decoder.resampler = nullptr; // 置空避免重复销毁
    }
    // 销毁解码器
    if (g_opus_decoder.decoder) {
        opus_decoder_destroy(g_opus_decoder.decoder);
        g_opus_decoder.decoder = nullptr; // 置空避免重复销毁
    }
    // 重置全局解码器参数（可选，增强安全性）
    g_opus_decoder.inputSampleRate = 0;
    g_opus_decoder.inputChannels = 0;
    g_opus_decoder.outputSampleRate = 0;
    g_opus_decoder.outputChannels = 0;
    g_opus_decoder.duration_ms = 0;
}
// ========== 新增结束 ==========
