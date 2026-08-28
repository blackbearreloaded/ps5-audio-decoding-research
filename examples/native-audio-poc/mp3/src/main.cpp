/*
 * ps5-audio-decoding-research - Native MP3 hardware/offload proof of concept.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Build this source as the src/main.cpp of a project created from
 * ps5-native-app-boilerplate. The application reads a bounded 48 kHz MP3,
 * frames it, decodes through libSceAudiodec, and feeds signed-16 PCM to
 * AudioOut.
 */

#include "native_audio.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace
{
constexpr std::uint16_t kAudioDecModule = 0x0088;
constexpr std::uint32_t kAudioDecMp3 = 2;
constexpr std::int32_t kAudioDecWordS16 = 1;
constexpr std::size_t kInputCapacity = 2 * 1024 * 1024;
constexpr std::size_t kPcmCapacity = 1152 * 2 * sizeof(std::int16_t);

struct AuInfo
{
    std::uint32_t size;
    void *address;
    std::uint32_t length;
};

struct PcmItem
{
    std::uint32_t size;
    void *address;
    std::uint32_t length;
};

struct Control
{
    void *param;
    void *stream_info;
    AuInfo *au_info;
    PcmItem *pcm_item;
};

struct Mp3Param
{
    std::uint32_t size;
    std::int32_t word_size;
};

struct Mp3Info
{
    std::uint32_t size;
    std::uint32_t header;
    std::uint8_t crc;
    std::uint8_t mode;
    std::uint8_t mode_extension;
    std::uint8_t copyright;
    std::uint8_t original;
    std::uint8_t emphasis;
    std::array<std::uint8_t, 2> reserved;
    std::int32_t result;
};

struct Frame
{
    std::size_t offset;
    std::size_t bytes;
    std::uint32_t sample_rate;
    std::uint32_t channels;
};

static_assert(sizeof(AuInfo) == 24);
static_assert(sizeof(PcmItem) == 24);
static_assert(sizeof(Control) == 32);
static_assert(sizeof(Mp3Param) == 8);
static_assert(sizeof(Mp3Info) == 20);

extern "C"
{
    int sceSysmoduleLoadModule(std::uint16_t id);
    int sceSysmoduleUnloadModule(std::uint16_t id);
    int sceAudiodecInitLibrary(std::uint32_t codec_type);
    int sceAudiodecTermLibrary(std::uint32_t codec_type);
    int sceAudiodecCreateDecoder(Control *control, std::uint32_t codec_type);
    int sceAudiodecDeleteDecoder(int handle);
    int sceAudiodecDecode(int handle, Control *control);
}

bool parse_frame(std::span<const std::uint8_t> input, std::size_t offset, Frame &frame) noexcept
{
    static constexpr std::array<std::uint16_t, 16> kMpeg1Bitrates{
        0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
    static constexpr std::array<std::uint16_t, 16> kMpeg2Bitrates{
        0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};
    static constexpr std::array<std::uint32_t, 3> kBaseRates{44100, 48000, 32000};

    if (offset + 4 > input.size() || input[offset] != 0xff || (input[offset + 1] & 0xe0) != 0xe0)
    {
        return false;
    }

    const std::uint32_t version = (input[offset + 1] >> 3) & 3;
    const std::uint32_t layer = (input[offset + 1] >> 1) & 3;
    const std::uint32_t bitrate_index = input[offset + 2] >> 4;
    const std::uint32_t rate_index = (input[offset + 2] >> 2) & 3;
    if (version == 1 || layer != 1 || bitrate_index == 0 || bitrate_index == 15 || rate_index == 3)
    {
        return false;
    }

    std::uint32_t rate = kBaseRates[rate_index];
    if (version == 2)
    {
        rate /= 2;
    }
    else if (version == 0)
    {
        rate /= 4;
    }

    const bool mpeg1 = version == 3;
    const std::uint32_t bitrate =
        (mpeg1 ? kMpeg1Bitrates[bitrate_index] : kMpeg2Bitrates[bitrate_index]) * 1000;
    const std::size_t bytes =
        (mpeg1 ? 144U : 72U) * bitrate / rate + ((input[offset + 2] >> 1) & 1U);
    if (bytes < 4 || bytes > 1441 || offset + bytes > input.size())
    {
        return false;
    }

    frame = {offset, bytes, rate, (input[offset + 3] >> 6) == 3 ? 1U : 2U};
    return true;
}

bool next_frame(std::span<const std::uint8_t> input, std::size_t start, Frame &frame) noexcept
{
    for (std::size_t offset = start; offset + 4 <= input.size(); ++offset)
    {
        if (parse_frame(input, offset, frame))
        {
            return true;
        }
    }
    return false;
}

class Mp3Decoder final
{
  public:
    Mp3Decoder() noexcept
        : param_{sizeof(param_), kAudioDecWordS16},
          info_{sizeof(info_), 0, 0, 0, 0, 0, 0, 0, {}, 0}, au_{sizeof(au_), nullptr, 0},
          pcm_{sizeof(pcm_), nullptr, 0}, control_{&param_, &info_, &au_, &pcm_}
    {
    }

    Mp3Decoder(const Mp3Decoder &) = delete;
    Mp3Decoder &operator=(const Mp3Decoder &) = delete;

    ~Mp3Decoder() noexcept
    {
        close();
    }

    bool open() noexcept
    {
        if (sceSysmoduleLoadModule(kAudioDecModule) < 0)
        {
            return false;
        }
        module_loaded_ = true;
        if (sceAudiodecInitLibrary(kAudioDecMp3) < 0)
        {
            return false;
        }
        library_initialized_ = true;
        decoder_ = sceAudiodecCreateDecoder(&control_, kAudioDecMp3);
        return decoder_ >= 0;
    }

    bool decode(std::span<const std::uint8_t> frame, std::span<std::uint8_t> output,
                std::size_t &produced) noexcept
    {
        if (decoder_ < 0 || frame.empty() || output.empty())
        {
            return false;
        }
        au_.address = const_cast<std::uint8_t *>(frame.data());
        au_.length = static_cast<std::uint32_t>(frame.size());
        pcm_.address = output.data();
        pcm_.length = static_cast<std::uint32_t>(output.size());
        if (sceAudiodecDecode(decoder_, &control_) < 0 || au_.length != frame.size() ||
            pcm_.length > output.size() || (pcm_.length & 1) != 0)
        {
            return false;
        }
        produced = pcm_.length;
        return true;
    }

  private:
    void close() noexcept
    {
        if (decoder_ >= 0)
        {
            sceAudiodecDeleteDecoder(decoder_);
            decoder_ = -1;
        }
        if (library_initialized_)
        {
            sceAudiodecTermLibrary(kAudioDecMp3);
            library_initialized_ = false;
        }
        if (module_loaded_)
        {
            sceSysmoduleUnloadModule(kAudioDecModule);
            module_loaded_ = false;
        }
    }

    Mp3Param param_;
    Mp3Info info_;
    AuInfo au_;
    PcmItem pcm_;
    Control control_;
    int decoder_ = -1;
    bool module_loaded_ = false;
    bool library_initialized_ = false;
};
} // namespace

int main() noexcept
{
    static std::array<std::uint8_t, kInputCapacity> input{};
    const std::size_t input_size =
        ps5::native_audio::read_file("/data/mp3-hw-poc.mp3", std::span{input});
    if (input_size == 0)
    {
        return 1;
    }

    Mp3Decoder decoder;
    if (!decoder.open())
    {
        return 2;
    }

    std::array<std::int16_t, kPcmCapacity / sizeof(std::int16_t)> pcm{};
    std::span<std::uint8_t> pcm_bytes(reinterpret_cast<std::uint8_t *>(pcm.data()), sizeof(pcm));
    ps5::native_audio::AudioOut audio;
    std::size_t offset = 0;
    bool output_open = false;
    while (offset < input_size)
    {
        Frame frame{};
        if (!next_frame(std::span{input}.first(input_size), offset, frame))
        {
            break;
        }
        if (frame.sample_rate != ps5::native_audio::kAudioOutRate)
        {
            return 3;
        }

        std::size_t produced = 0;
        if (!decoder.decode(std::span{input}.first(input_size).subspan(frame.offset, frame.bytes),
                            pcm_bytes, produced))
        {
            return 4;
        }
        if (!output_open)
        {
            if (!audio.open())
            {
                return 5;
            }
            output_open = true;
        }
        if (!audio.push(std::span{pcm}.first(produced / sizeof(std::int16_t)), frame.channels))
        {
            return 6;
        }
        offset = frame.offset + frame.bytes;
    }

    return output_open && audio.flush() ? 0 : 7;
}
