#include "native_audio.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>

namespace
{
std::array<std::int16_t, ps5::native_audio::kAudioOutGrain * 2> captured{};
int output_calls = 0;
} // namespace

extern "C"
{
    int sceAudioOutInit(void)
    {
        return 0;
    }

    int sceAudioOutOpen(int, int, int, std::uint32_t, std::uint32_t, std::uint32_t)
    {
        return 1;
    }

    int sceAudioOutClose(int)
    {
        return 0;
    }

    int sceAudioOutOutput(int, const void *samples)
    {
        if (samples == nullptr)
        {
            return 0;
        }

        const auto *source = static_cast<const std::int16_t *>(samples);
        std::copy_n(source, captured.size(), captured.begin());
        ++output_calls;
        return 0;
    }

    int sceAudioOutSetVolume(int, int, const int *)
    {
        return 0;
    }
}

int main()
{
    ps5::native_audio::AudioOut audio;
    assert(audio.open());

    constexpr std::array<std::int16_t, 2> stereo_frame{123, -456};
    assert(audio.push(stereo_frame, 2));
    assert(output_calls == 0);
    assert(audio.flush());

    assert(output_calls == 1);
    assert(captured[0] == stereo_frame[0]);
    assert(captured[1] == stereo_frame[1]);
    assert(std::all_of(captured.begin() + 2, captured.end(),
                       [](const auto sample) { return sample == 0; }));
}
