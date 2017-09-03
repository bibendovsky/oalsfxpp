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


/* Find the next power-of-2 for non-power-of-2 numbers. */
inline ALuint NextPowerOf2(ALuint value)
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
inline ALint fastf2i(ALfloat f)
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
enum DevFmtType {
    DevFmtByte   = ALC_BYTE_SOFT,
    DevFmtUByte  = ALC_UNSIGNED_BYTE_SOFT,
    DevFmtShort  = ALC_SHORT_SOFT,
    DevFmtUShort = ALC_UNSIGNED_SHORT_SOFT,
    DevFmtInt    = ALC_INT_SOFT,
    DevFmtUInt   = ALC_UNSIGNED_INT_SOFT,
    DevFmtFloat  = ALC_FLOAT_SOFT,

    DevFmtTypeDefault = DevFmtFloat
};
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
constexpr auto MAX_AMBI_COEFFS = (MAX_AMBI_ORDER+1) * (MAX_AMBI_ORDER+1);


using ChannelConfig = ALfloat[MAX_AMBI_COEFFS];

struct BFChannelConfig
{
    ALfloat scale;
    ALsizei index;
}; // BFChannelConfig

union AmbiConfig
{
    /* Ambisonic coefficients for mixing to the dry buffer. */
    ChannelConfig coeffs[MAX_OUTPUT_CHANNELS];
    /* Coefficient channel mapping for mixing to the dry buffer. */
    BFChannelConfig map[MAX_OUTPUT_CHANNELS];
}; // AmbiConfig


/* Size for temporary storage of buffer data, in ALfloats. Larger values need
 * more memory, while smaller values may need more iterations. The value needs
 * to be a sensible size, however, as it constrains the max stepping value used
 * for mixing, as well as the maximum number of samples per mixing iteration.
 */
constexpr auto BUFFERSIZE = 2048;

using SampleBuffer = std::array<ALfloat, BUFFERSIZE>;
using SampleBuffers = std::vector<SampleBuffer>;

struct ALCdevice_struct
{
    unsigned int ref;

    ALuint frequency;
    ALuint update_size;
    enum DevFmtChannels fmt_chans;

    // Maximum number of slots that can be created
    ALuint auxiliary_effect_slot_max;

    ALsizei num_aux_sends;

    /* Temp storage used for each source when mixing. */
    ALfloat source_data[BUFFERSIZE];
    ALfloat resampled_data[BUFFERSIZE];
    ALfloat filtered_data[BUFFERSIZE];

    /* The "dry" path corresponds to the main output. */
    struct Dry {
        AmbiConfig ambi;
        /* Number of coefficients in each Ambi.Coeffs to mix together (4 for
         * first-order, 9 for second-order, etc). If the count is 0, Ambi.Map
         * is used instead to map each output to a coefficient index.
         */
        ALsizei coeff_count;

        SampleBuffers buffer;
        ALsizei num_channels;
        ALsizei num_channels_per_order[MAX_AMBI_ORDER+1];
    } dry;

    /* First-order ambisonics output, to be upsampled to the dry buffer if different. */
    struct FOAOut {
        AmbiConfig ambi;
        /* Will only be 4 or 0. */
        ALsizei coeff_count;

        SampleBuffers* buffer;
        ALsizei num_channels;
    } foa_out;

    /* "Real" output, which will be written to the device buffer. May alias the
     * dry buffer.
     */
    struct RealOut {
        enum Channel channel_name[MAX_OUTPUT_CHANNELS];

        SampleBuffers *buffer;
        ALsizei num_channels;
    } real_out;

    ALCcontext* context;

    struct ALsource* source;
    const ALfloat* source_samples;
    struct ALeffectslot* effect_slot;
    struct ALeffect* effect;
};


struct ALCcontext_struct {
    unsigned int ref;

    struct ALvoice *voice;
    ALsizei voice_count;
    ALCdevice  *device;
};

ALCcontext *GetContextRef(void);

void ALCcontext_IncRef(ALCcontext *context);
void ALCcontext_DecRef(ALCcontext *context);

void AllocateVoices(ALCcontext *context, ALsizei num_voices, ALsizei old_sends);


void SetDefaultWFXChannelOrder(ALCdevice *device);


/**
 * GetChannelIdxByName
 *
 * Returns the index for the given channel name (e.g. FrontCenter), or -1 if it
 * doesn't exist.
 */
inline ALint GetChannelIndex(const enum Channel names[MAX_OUTPUT_CHANNELS], enum Channel chan)
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
using ALfloatBUFFERSIZE = ALfloat[BUFFERSIZE];


#endif
