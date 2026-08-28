/*
 * ps5-audio-decoding-research - Native AAC hardware/offload proof of concept.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Build this source as the src/main.cpp of a project created from
 * ps5-native-app-boilerplate. The application reads a bounded ADTS file,
 * sends each frame to libSceAudiodec, and feeds signed-16 PCM to AudioOut.
 */

#include "native_audio.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace
{
constexpr std::uint16_t kAudioDecModule = 0x0088;
constexpr std::uint32_t kAudioDecAac = 3;
constexpr std::uint32_t kAudioDecWordS16 = 1;
constexpr std::size_t kInputCapacity = 2 * 1024 * 1024;
constexpr std::size_t kPcmCapacity = 64 * 1024;
constexpr std::size_t kAdtsHeaderSize = 7;
constexpr std::size_t kAdtsMaxFrame = 4608;

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

struct AacParam
{
    std::uint32_t size;
    std::int32_t word_size;
    std::uint32_t config_number;
    std::uint32_t sampling_frequency_index;
    std::uint32_t max_channels;
    std::uint32_t enable_he_aac;
};

struct AacInfo
{
    std::uint32_t size;
    std::uint32_t sampling_frequency;
    std::uint32_t channel_count;
    std::uint32_t he_aac;
    std::int32_t result;
};

static_assert(sizeof(AuInfo) == 24);
static_assert(sizeof(PcmItem) == 24);
static_assert(sizeof(Control) == 32);

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

bool next_adts_frame(std::span<const std::uint8_t> input, std::size_t start, std::size_t &offset,
                     std::size_t &length) noexcept
{
    for (std::size_t at = start; at + kAdtsHeaderSize <= input.size(); ++at)
    {
        if (input[at] != 0xff || (input[at + 1] & 0xf6) != 0xf0)
        {
            continue;
        }

        const std::size_t header_size = (input[at + 1] & 1) != 0 ? 7 : 9;
        const std::size_t frame_size = (static_cast<std::size_t>(input[at + 3] & 3) << 11) |
                                       (static_cast<std::size_t>(input[at + 4]) << 3) |
                                       (static_cast<std::size_t>(input[at + 5]) >> 5);
        if (frame_size < header_size || frame_size > kAdtsMaxFrame ||
            at + frame_size > input.size())
        {
            continue;
        }

        offset = at;
        length = frame_size;
        return true;
    }
    return false;
}

class AacDecoder final
{
  public:
    AacDecoder() noexcept
        : param_{sizeof(param_), kAudioDecWordS16, 1, 4, 2, 0}, info_{sizeof(info_), 0, 0, 0, 0},
          au_{sizeof(au_), nullptr, 0}, pcm_{sizeof(pcm_), nullptr, 0},
          control_{&param_, &info_, &au_, &pcm_}
    {
    }

    AacDecoder(const AacDecoder &) = delete;
    AacDecoder &operator=(const AacDecoder &) = delete;

    ~AacDecoder() noexcept
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
        if (sceAudiodecInitLibrary(kAudioDecAac) < 0)
        {
            return false;
        }
        library_initialized_ = true;
        decoder_ = sceAudiodecCreateDecoder(&control_, kAudioDecAac);
        if (decoder_ < 0)
        {
            close();
            return false;
        }
        return true;
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
        if (sceAudiodecDecode(decoder_, &control_) < 0)
        {
            return false;
        }
        if (pcm_.length > output.size() || (pcm_.length & 1) != 0)
        {
            return false;
        }
        produced = pcm_.length;
        return true;
    }

    std::uint32_t sample_rate() const noexcept
    {
        return info_.sampling_frequency;
    }

    std::uint32_t channels() const noexcept
    {
        return info_.channel_count;
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
            sceAudiodecTermLibrary(kAudioDecAac);
            library_initialized_ = false;
        }
        if (module_loaded_)
        {
            sceSysmoduleUnloadModule(kAudioDecModule);
            module_loaded_ = false;
        }
    }

    AacParam param_;
    AacInfo info_;
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
        ps5::native_audio::read_file("/data/aac-hw-poc.aac", std::span{input});
    if (input_size == 0)
    {
        return 1;
    }

    AacDecoder decoder;
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
        std::size_t frame_offset = 0;
        std::size_t frame_size = 0;
        if (!next_adts_frame(std::span{input}.first(input_size), offset, frame_offset, frame_size))
        {
            return 3;
        }
        std::size_t produced = 0;
        if (!decoder.decode(std::span{input}.first(input_size).subspan(frame_offset, frame_size),
                            pcm_bytes, produced))
        {
            return 4;
        }
        if (decoder.sample_rate() != ps5::native_audio::kAudioOutRate ||
            (decoder.channels() != 1 && decoder.channels() != 2))
        {
            return 5;
        }
        if (!output_open)
        {
            if (!audio.open())
            {
                return 6;
            }
            output_open = true;
        }
        if (!audio.push(std::span{pcm}.first(produced / sizeof(std::int16_t)), decoder.channels()))
        {
            return 7;
        }
        offset = frame_offset + frame_size;
    }

    return output_open && audio.flush() ? 0 : 8;
}
