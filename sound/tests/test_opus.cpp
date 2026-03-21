#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <opus/opus.h>
#include "opus.h"

// WAV 文件头结构
#pragma pack(push, 1)
struct WavHeader {
    char riff[4];       // "RIFF"
    uint32_t fileSize;  // 文件大小 - 8
    char wave[4];       // "WAVE"
};
#pragma pack(pop)

// WAV 块头结构
#pragma pack(push, 1)
struct WavChunkHeader {
    char id[4];       // 块标识符
    uint32_t size;    // 块大小
};
#pragma pack(pop)

int pcm2opus_main(int argc, char* argv[]) {

    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;

    const char* wavFilePath = argv[2];
    std::ifstream wavFile(wavFilePath, std::ios::binary);
    if (!wavFile) {
        std::cerr << "无法打开文件: " << wavFilePath << std::endl;
        return 1;
    }

    // 读取 WAV 文件头
    WavHeader header;
    wavFile.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));
    if (wavFile.fail() || std::memcmp(header.riff, "RIFF", 4) != 0 || std::memcmp(header.wave, "WAVE", 4) != 0) {
        std::cerr << "无效的 WAV 文件" << std::endl;
        return 1;
    }

    // 查找 fmt 块
    bool fmtFound = false;
    uint32_t dataSize = 0;
    while (!fmtFound) {
        WavChunkHeader chunkHeader;
        wavFile.read(reinterpret_cast<char*>(&chunkHeader), sizeof(WavChunkHeader));
        if (wavFile.fail()) {
            std::cerr << "读取 WAV 块头失败" << std::endl;
            return 1;
        }

        if (std::memcmp(chunkHeader.id, "fmt ", 4) == 0) {
            fmtFound = true;
            uint32_t fmtSize = chunkHeader.size;
            if (fmtSize < 16) {
                std::cerr << "无效的 fmt 块大小" << std::endl;
                return 1;
            }

            wavFile.read(reinterpret_cast<char*>(&audioFormat), sizeof(audioFormat));
            wavFile.read(reinterpret_cast<char*>(&numChannels), sizeof(numChannels));
            wavFile.read(reinterpret_cast<char*>(&sampleRate), sizeof(sampleRate));
            wavFile.read(reinterpret_cast<char*>(&byteRate), sizeof(byteRate));
            wavFile.read(reinterpret_cast<char*>(&blockAlign), sizeof(blockAlign));
            wavFile.read(reinterpret_cast<char*>(&bitsPerSample), sizeof(bitsPerSample));

            // 打印 WAV 文件信息
            std::cout << "WAV 文件信息:" << std::endl;
            std::cout << "  采样率: " << sampleRate << std::endl;
            std::cout << "  通道数: " << numChannels << std::endl;
            std::cout << "  每样本位数: " << bitsPerSample << std::endl;

            // 跳过额外的格式信息
            if (fmtSize > 16) {
                wavFile.seekg(fmtSize - 16, std::ios::cur);
            }
        } else {
            // 跳过当前块
            wavFile.seekg(chunkHeader.size, std::ios::cur);
        }
    }

    // 查找 data 块
    bool dataFound = false;
    while (!dataFound) {
        WavChunkHeader chunkHeader;
        wavFile.read(reinterpret_cast<char*>(&chunkHeader), sizeof(WavChunkHeader));
        if (wavFile.fail()) {
            std::cerr << "读取 WAV 块头失败" << std::endl;
            return 1;
        }

        if (std::memcmp(chunkHeader.id, "data", 4) == 0) {
            dataFound = true;
            dataSize = chunkHeader.size;
        } else {
            // 跳过当前块
            wavFile.seekg(chunkHeader.size, std::ios::cur);
        }
    }

    // 初始化 Opus 编码器
    int outputSampleRate = 16000;
    int outputChannels = 1;
    int duration_ms = 60;

    std::cout << "原始采样率: " << sampleRate << " 通道数: "<< numChannels << std::endl;
    std::cout << "目标采样率: " << outputSampleRate << " 通道数: "<< outputChannels << std::endl;
    
    if (init_opus_encoder(sampleRate, numChannels, duration_ms, outputSampleRate, outputChannels) != 0) {
        std::cerr << "Opus 编码器初始化失败" << std::endl;
        return 1;
    }

    // 读取 PCM 数据
    std::vector<opus_int16> pcmData(dataSize / sizeof(opus_int16));
    wavFile.read(reinterpret_cast<char*>(pcmData.data()), dataSize);
    if (wavFile.fail()) {
        std::cerr << "读取 PCM 数据失败" << std::endl;
        return 1;
    }

    // 编码 PCM 数据为 Opus 码流
    int frameCount = 0;
    size_t totalBytesRead = 0;
    while (totalBytesRead < dataSize) {
        // 计算当前帧的大小
        size_t frameSize = std::min(static_cast<size_t>(sampleRate * duration_ms / 1000 * numChannels * sizeof(opus_int16)), dataSize - totalBytesRead);

        // 分配 Opus 数据缓冲区
        unsigned char opusData[4000];
        int opusSize = sizeof(opusData);

        // 编码当前帧
        int encodedBytes = pcm2opus(reinterpret_cast<unsigned char*>(pcmData.data()) + totalBytesRead, frameSize, opusData, &opusSize);
        if (encodedBytes < 0) {
            std::cerr << "编码帧失败" << std::endl;
            break;
        }

        // 生成文件名
        std::ostringstream filename;
        filename << "test" << std::setw(3) << std::setfill('0') << (frameCount + 1) << ".opus";

        // 写入文件
        std::ofstream frameFile(filename.str(), std::ios::binary);
        frameFile.write(reinterpret_cast<char*>(opusData), opusSize);
        frameFile.close();

        frameCount++;
        totalBytesRead += frameSize;
    }

    // 清理资源
    // speex_resampler_destroy(g_opus_encoder.resampler);
    // opus_encoder_destroy(g_opus_encoder.encoder);
    free_opus_encoder();

    std::cout << "转换完成，共生成 " << frameCount << " 个帧文件" << std::endl;
    return 0;
}


// 写入 WAV 文件头
void writeWavHeader(std::ofstream& wavFile, int sampleRate, int channels, int pcmSize) {
    const int bitsPerSample = 16;
    const int byteRate = sampleRate * channels * bitsPerSample / 8;
    const int blockAlign = channels * bitsPerSample / 8;

    char riff[4] = {'R', 'I', 'F', 'F'};
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    char data[4] = {'d', 'a', 't', 'a'};

    int fileSize = 36 + pcmSize;
    int chunkSize = 16;

    wavFile.write(riff, 4);
    wavFile.write(reinterpret_cast<const char*>(&fileSize), 4);
    wavFile.write(wave, 4);
    wavFile.write(fmt, 4);
    wavFile.write(reinterpret_cast<const char*>(&chunkSize), 4);

    short audioFormat = 1; // PCM
    short numChannels = channels;

    wavFile.write(reinterpret_cast<const char*>(&audioFormat), 2);
    wavFile.write(reinterpret_cast<const char*>(&numChannels), 2);
    wavFile.write(reinterpret_cast<const char*>(&sampleRate), 4);
    wavFile.write(reinterpret_cast<const char*>(&byteRate), 4);
    wavFile.write(reinterpret_cast<const char*>(&blockAlign), 2);
    wavFile.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    int dataSize = pcmSize;

    wavFile.write(data, 4);
    wavFile.write(reinterpret_cast<const char*>(&dataSize), 4);
}

// 主函数
int opus2pcm_main(int argc, char* argv[]) {
    std::string outputFilename = argv[2];
    std::ofstream wavFile(outputFilename, std::ios::binary);
    if (!wavFile) {
        std::cerr << "无法打开输出文件: " << outputFilename << std::endl;
        return 1;
    }

    // 初始化 Opus 解码器
    int inputSampleRate = 16000; // 假设输入采样率为 16kHz
    int inputChannels = 1; // 假设输入通道数为 1
    int duration_ms = 60;
    int outputSampleRate = 16000; // 假设输出采样率为 16kHz
    int outputChannels = 1; // 假设输出通道数为 1

    if (init_opus_decoder(inputSampleRate, inputChannels, duration_ms, outputSampleRate, outputChannels) != 0) {
        std::cerr << "Opus 解码器初始化失败" << std::endl;
        return 1;
    }

    std::vector<opus_int16> allPcmData;
    int totalPcmSize = 0;

    for (int fileIndex = 1; ; ++fileIndex) {
        char filename[20];
        snprintf(filename, sizeof(filename), "test%03d.opus", fileIndex);

        std::ifstream opusFile(filename, std::ios::binary);
        if (!opusFile) {
            break; // 没有更多文件，退出循环
        }

        opusFile.seekg(0, std::ios::end);
        int opusSize = opusFile.tellg();
        opusFile.seekg(0, std::ios::beg);

        std::vector<unsigned char> opusData(opusSize);
        opusFile.read(reinterpret_cast<char*>(opusData.data()), opusSize);

        int maxPcmSize = 480 * outputChannels * sizeof(opus_int16); // 假设最大帧大小为 480 样本
        std::vector<opus_int16> pcmData(maxPcmSize);

        int pcmSize = maxPcmSize;
        int result = opus2pcm(opusData.data(), opusSize, reinterpret_cast<unsigned char*>(pcmData.data()), &pcmSize);
        if (result < 0) {
            std::cerr << "文件 " << filename << " 解码失败" << std::endl;
            continue;
        }

        allPcmData.insert(allPcmData.end(), pcmData.begin(), pcmData.begin() + pcmSize / sizeof(opus_int16));
        totalPcmSize += pcmSize;
    }

    // 写入 WAV 文件头
    writeWavHeader(wavFile, outputSampleRate, outputChannels, totalPcmSize);

    // 写入 PCM 数据
    wavFile.write(reinterpret_cast<char*>(allPcmData.data()), totalPcmSize);

    // 清理资源
    free_opus_decoder();

    wavFile.close();

    std::cout << "转换完成，共生成 " << totalPcmSize << " 字节的 PCM 数据" << std::endl;
    return 0;
}

#define TEST_INPUT_WAV  "./record.wav"   // 你的测试输入WAV文件
#define TEST_OUTPUT_WAV "./Opus2Wav.wav"  // 解码后的输出WAV文件

// 测试用例1：测试 wav2opus（调用你的 pcm2opus_main）
TEST(OpusTest, Wav2Opus) {
    // 构造命令行参数（模拟 ./程序 wav2opus test_input.wav）
    char* argv[] = {
        const_cast<char*>("dummy_program_name"),  // argv[0]（程序名，无实际意义）
        const_cast<char*>("wav2opus"),            // argv[1]（指令）
        const_cast<char*>(TEST_INPUT_WAV)         // argv[2]（输入WAV文件）
    };
    int argc = sizeof(argv) / sizeof(char*);

    // 调用你的 pcm2opus_main 函数
    int ret = pcm2opus_main(argc, argv);
    
    // GTest 断言：返回值为0表示成功
    ASSERT_EQ(ret, 0) << "WAV转Opus失败！";
}

// 测试用例2：测试 opus2wav（调用你的 opus2pcm_main）
TEST(OpusTest, Opus2Wav) {
    // 构造命令行参数（模拟 ./程序 opus2wav test_output.wav）
    char* argv[] = {
        const_cast<char*>("dummy_program_name"),  // argv[0]
        const_cast<char*>("opus2wav"),            // argv[1]（指令）
        const_cast<char*>(TEST_OUTPUT_WAV)        // argv[2]（输出WAV文件）
    };
    int argc = sizeof(argv) / sizeof(char*);

    // 调用你的 opus2pcm_main 函数
    int ret = opus2pcm_main(argc, argv);
    
    // GTest 断言：返回值为0表示成功
    ASSERT_EQ(ret, 0) << "Opus转WAV失败！";
}