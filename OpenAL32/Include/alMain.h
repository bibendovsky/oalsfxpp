#ifndef AL_MAIN_H
#define AL_MAIN_H

#include <cstring>
#include <cassert>
#include <cmath>

#include <array>
#include <vector>

#include "AL/al.h"
#include "AL/alc.h"
#include "AL/alext.h"


/* Calculates the size of a struct with N elements of a flexible array member.
 * GCC and Clang allow offsetof(Type, fam[N]) for this, but MSVC seems to have
 * trouble, so a bit more verbose workaround is needed.
 */
#define FAM_SIZE(T, M, N)  (offsetof(T, M) + sizeof(((T*)NULL)->M[0])*(N))

#define COUNTOF(x) (sizeof(x) / sizeof(0[x]))


constexpr auto DEFAULT_OUTPUT_RATE = 44100;


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


/* Find the next power-of-2 for non-power-of-2 numbers. */
inline int NextPowerOf2(int value)
{
    if(value > 0)
    {
        value--;
        value |= value>>1;
        value |= value>>2;
        value |= value>>4;
        value |= value>>8;
        value |= value>>16;
    }
    return value+1;
}

/** Round up a value to the next multiple. */
inline size_t RoundUp(size_t value, size_t r)
{
    value += r-1;
    return value - (value%r);
}

/* Fast float-to-int conversion. Assumes the FPU is already in round-to-zero
 * mode. */
inline ALint fastf2i(float f)
{
    return lrintf(f);
}


enum Channel {
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

    Aux0,
    Aux1,
    Aux2,
    Aux3,
    Aux4,
    Aux5,
    Aux6,
    Aux7,
    Aux8,
    Aux9,
    Aux10,
    Aux11,
    Aux12,
    Aux13,
    Aux14,
    Aux15,

    InvalidChannel
};


/* Device formats */
enum DevFmtChannels {
    DevFmtMono   = ALC_MONO_SOFT,
    DevFmtStereo = ALC_STEREO_SOFT,
    DevFmtQuad   = ALC_QUAD_SOFT,
    DevFmtX51    = ALC_5POINT1_SOFT,
    DevFmtX61    = ALC_6POINT1_SOFT,
    DevFmtX71    = ALC_7POINT1_SOFT,

    /* Similar to 5.1, except using rear channels instead of sides */
    DevFmtX51Rear = 0x80000000,

    DevFmtChannelsDefault = DevFmtMono
};
constexpr auto MAX_OUTPUT_CHANNELS = 16;


/* The maximum number of Ambisonics coefficients. For a given order (o), the
 * size needed will be (o+1)**2, thus zero-order has 1, first-order has 4,
 * second-order has 9, third-order has 16, and fourth-order has 25.
 */
constexpr auto MAX_AMBI_ORDER = 3;
constexpr auto MAX_AMBI_COEFFS = (MAX_AMBI_ORDER + 1) * (MAX_AMBI_ORDER + 1);


using ChannelConfig = std::array<float, MAX_AMBI_COEFFS>;

struct BFChannelConfig
{
    float scale;
    int index;
}; // BFChannelConfig

union AmbiConfig
{
    using Coeffs = std::array<ChannelConfig, MAX_OUTPUT_CHANNELS>;
    using Map = std::array<BFChannelConfig, MAX_OUTPUT_CHANNELS>;


    // Ambisonic coefficients for mixing to the dry buffer.
    Coeffs coeffs;

    // Coefficient channel mapping for mixing to the dry buffer.
    Map map;
}; // AmbiConfig


// Size for temporary storage of buffer data, in ALfloats. Larger values need
// more memory, while smaller values may need more iterations. The value needs
// to be a sensible size, however, as it constrains the max stepping value used
// for mixing, as well as the maximum number of samples per mixing iteration.
constexpr auto BUFFERSIZE = 2048;

using SampleBuffer = std::array<float, BUFFERSIZE>;
using SampleBuffers = std::vector<SampleBuffer>;

struct ALCdevice_struct
{
    using ChannelNames = std::array<Channel, MAX_OUTPUT_CHANNELS>;


    // The "dry" path corresponds to the main output.
    struct Dry
    {
        using ChannelsPerOrder = std::array<int, MAX_AMBI_ORDER + 1>;


        AmbiConfig ambi;

        // Number of coefficients in each Ambi.Coeffs to mix together (4 for
        // first-order, 9 for second-order, etc). If the count is 0, Ambi.Map
        // is used instead to map each output to a coefficient index.
        int coeff_count;

        SampleBuffers buffers;
        int num_channels;
        ChannelsPerOrder num_channels_per_order;
    }; // Dry

    // First-order ambisonics output, to be upsampled to the dry buffer if different. */
    struct FOAOut
    {
        AmbiConfig ambi;

        // Will only be 4 or 0.
        int coeff_count;

        SampleBuffers* buffers;
        int num_channels;
    }; // FOAOut

    // "Real" output, which will be written to the device buffer. May alias the
    // dry buffer.
    struct RealOut
    {
        ChannelNames channel_name;
        SampleBuffers* buffers;
        int num_channels;
    }; // RealOut


    int frequency;
    int update_size;
    DevFmtChannels fmt_chans;

    // Maximum number of slots that can be created
    int auxiliary_effect_slot_max;

    int num_aux_sends;

    // Temp storage used for each source when mixing.
    SampleBuffer source_data;
    SampleBuffer resampled_data;
    SampleBuffer filtered_data;

    Dry dry;
    FOAOut foa_out;
    RealOut real_out;

    struct ALsource* source;
    const float* source_samples;
    struct ALeffectslot* effect_slot;
    struct ALeffect* effect;
    struct ALvoice* voice;
    int voice_count;
}; // ALCdevice_struct


extern ALCdevice* g_device;
void AllocateVoices(ALCdevice* device, int num_voices, int old_sends);
void SetDefaultWFXChannelOrder(ALCdevice* device);


/**
 * GetChannelIdxByName
 *
 * Returns the index for the given channel name (e.g. FrontCenter), or -1 if it
 * doesn't exist.
 */
inline ALint GetChannelIndex(const ALCdevice::ChannelNames& names, const Channel chan)
{
    ALint i;
    for(i = 0;i < MAX_OUTPUT_CHANNELS;i++)
    {
        if(names[i] == chan)
            return i;
    }
    return -1;
}

#define GetChannelIdxByName(x, c) GetChannelIndex((x).channel_name, (c))



/* Small hack to use a pointer-to-array types as a normal argument type.
 * Shouldn't be used directly.
 */
using ALfloatBUFFERSIZE = float[BUFFERSIZE];


#endif
