#ifndef AL_MAIN_H
#define AL_MAIN_H

#include <cstring>
#include <cassert>
#include <cmath>

#include <array>
#include <type_traits>
#include <vector>


constexpr auto max_input_channels = 8;


enum class EffectType
{
    null,
    chorus,
    compressor,
    dedicated_dialog,
    dedicated_low_frequency,
    distortion,
    echo,
    equalizer,
    flanger,
    ring_modulator,
    reverb,
    eax_reverb,
}; // EffectType


template<typename T>
constexpr int count_of(const T&)
{
    static_assert(std::rank_v<T> == 1, "Expected an one-dimensional array.");
    return static_cast<int>(std::extent_v<T>);
}


constexpr auto default_output_rate = 44100;
constexpr auto max_output_channels = 16;

// The maximum number of Ambisonics coefficients. For a given order (o), the
// size needed will be (o+1)**2, thus zero-order has 1, first-order has 4,
// second-order has 9, third-order has 16, and fourth-order has 25.
constexpr auto max_ambi_order = 3;
constexpr auto max_ambi_coeffs = (max_ambi_order + 1) * (max_ambi_order + 1);

// Size for temporary storage of buffer data, in ALfloats. Larger values need
// more memory, while smaller values may need more iterations. The value needs
// to be a sensible size, however, as it constrains the max stepping value used
// for mixing, as well as the maximum number of samples per mixing iteration.
constexpr auto max_sample_buffer_size = 2048;


namespace detail
{


template<typename T, int TExtent1, std::size_t... TExtents>
struct MdArray
{
    using Type = typename std::array<typename MdArray<T, TExtents...>::Type, TExtent1>;
};

template<typename T, std::size_t TExtent>
struct MdArray<T, TExtent>
{
    using Type = std::array<T, TExtent>;
};


} // detail


// Multidimensional std::array.
template<typename T, std::size_t... TExtents>
using MdArray = typename detail::MdArray<T, TExtents...>::Type;


// Find the next power-of-2 for non-power-of-2 numbers.
inline int next_power_of_2(
    int value)
{
    if (value > 0)
    {
        value -= 1;
        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;
    }

    return value + 1;
}

enum Channel
{
    FrontLeft = 0,
    FrontRight,
    FrontCenter,
    LFE,
    BackLeft,
    BackRight,
    BackCenter,
    SideLeft,
    SideRight,

    UpperFrontLeft,
    UpperFrontRight,
    UpperBackLeft,
    UpperBackRight,
    LowerFrontLeft,
    LowerFrontRight,
    LowerBackLeft,
    LowerBackRight,

    InvalidChannel,
}; // Channel

// Device formats
enum DevFmtChannels
{
    DevFmtMono,
    DevFmtStereo,
    DevFmtQuad,
    DevFmtX51,
    DevFmtX61,
    DevFmtX71,

    // Similar to 5.1, except using rear channels instead of sides
    DevFmtX51Rear = 0x80000000,

    DevFmtChannelsDefault = DevFmtMono
}; // DevFmtChannels


using ChannelConfig = std::array<float, max_ambi_coeffs>;

struct BFChannelConfig
{
    float scale_;
    int index_;


    void reset();
}; // BFChannelConfig

union AmbiConfig
{
    using Coeffs = std::array<ChannelConfig, max_output_channels>;
    using Map = std::array<BFChannelConfig, max_output_channels>;


    // Ambisonic coefficients for mixing to the dry buffer.
    Coeffs coeffs_;

    // Coefficient channel mapping for mixing to the dry buffer.
    Map map_;


    void reset();
}; // AmbiConfig


using SampleBuffer = std::array<float, max_sample_buffer_size>;
using SampleBuffers = std::vector<SampleBuffer>;

struct ALCdevice_struct
{
    using ChannelNames = std::array<Channel, max_output_channels>;

    struct AmbiOutput
    {
        AmbiConfig ambi_;

        // Number of coefficients in each Ambi.Coeffs to mix together (4 for
        // first-order, 9 for second-order, etc). If the count is 0, Ambi.Map
        // is used instead to map each output to a coefficient index.
        //
        // Will only be 4 or 0 (first-order ambisonics output).
        int coeff_count_;
    }; // AmbiOutput


    int frequency_;
    int update_size_;
    DevFmtChannels channel_format_;

    int channel_count_;
    ChannelNames channel_names_;
    SampleBuffers sample_buffers_;

    // Temp storage used for each source when mixing.
    SampleBuffer source_data_;
    SampleBuffer resampled_data_;
    SampleBuffer filtered_data_;

    // The "dry" path corresponds to the main output.
    AmbiOutput dry_;

    // First-order ambisonics output, to be upsampled to the dry buffer if different.
    AmbiOutput foa_;

    struct ALsource* source_;
    const float* source_samples_;
    struct EffectSlot* effect_slot_;
    struct Effect* effect_;
}; // ALCdevice_struct

using ALCdevice = ALCdevice_struct;


extern ALCdevice* g_device;
void set_default_wfx_channel_order(ALCdevice* device);


// Returns the index for the given channel name (e.g. FrontCenter), or -1 if it
// doesn't exist.
inline int get_channel_index(
    const ALCdevice::ChannelNames& names,
    const Channel chan)
{
    auto i = 0;

    for (const auto& name : names)
    {
        if(name == chan)
        {
            return i;
        }

        i += 1;
    }

    return -1;
}


#endif
