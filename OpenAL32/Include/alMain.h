#ifndef AL_MAIN_H
#define AL_MAIN_H

#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <assert.h>
#include <math.h>
#include <limits.h>

#include "AL/al.h"
#include "AL/alc.h"
#include "AL/alext.h"

#include "static_assert.h"
#include "align.h"
#include "vector.h"
#include "almalloc.h"


typedef ALint64SOFT ALint64;
typedef ALuint64SOFT ALuint64;

#ifndef UNUSED
#define UNUSED(x) x
#endif

#define DECL_FORMAT(x, y, z)

/* Calculates the size of a struct with N elements of a flexible array member.
 * GCC and Clang allow offsetof(Type, fam[N]) for this, but MSVC seems to have
 * trouble, so a bit more verbose workaround is needed.
 */
#define FAM_SIZE(T, M, N)  (offsetof(T, M) + sizeof(((T*)NULL)->M[0])*(N))

#define FORCE_ALIGN

#define DECL_VLA(T, _name, _size)  T *_name = alloca((_size) * sizeof(T))

#define COUNTOF(x) (sizeof(x) / sizeof(0[x]))


#define DERIVE_FROM_TYPE(t)          t t##_parent
#define STATIC_CAST(to, obj)         (&(obj)->to##_parent)
#define STATIC_UPCAST(to, from, obj) ((to*)((char*)(obj) - offsetof(to, from##_parent)))

#define DECLARE_FORWARD(T1, T2, rettype, func)                                \
rettype T1##_##func(T1 *obj)                                                  \
{ return T2##_##func(STATIC_CAST(T2, obj)); }

#define DECLARE_FORWARD1(T1, T2, rettype, func, argtype1)                     \
rettype T1##_##func(T1 *obj, argtype1 a)                                      \
{ return T2##_##func(STATIC_CAST(T2, obj), a); }

#define DECLARE_FORWARD2(T1, T2, rettype, func, argtype1, argtype2)           \
rettype T1##_##func(T1 *obj, argtype1 a, argtype2 b)                          \
{ return T2##_##func(STATIC_CAST(T2, obj), a, b); }

#define DECLARE_FORWARD3(T1, T2, rettype, func, argtype1, argtype2, argtype3) \
rettype T1##_##func(T1 *obj, argtype1 a, argtype2 b, argtype3 c)              \
{ return T2##_##func(STATIC_CAST(T2, obj), a, b, c); }


#define GET_VTABLE1(T1)     (&(T1##_vtable))
#define GET_VTABLE2(T1, T2) (&(T1##_##T2##_vtable))

#define SET_VTABLE1(T1, obj)     ((obj)->vtbl = GET_VTABLE1(T1))
#define SET_VTABLE2(T1, T2, obj) (STATIC_CAST(T2, obj)->vtbl = GET_VTABLE2(T1, T2))

#define DECLARE_THUNK(T1, T2, rettype, func)                                  \
static rettype T1##_##T2##_##func(T2 *obj)                                    \
{ return T1##_##func(STATIC_UPCAST(T1, T2, obj)); }

#define DECLARE_THUNK1(T1, T2, rettype, func, argtype1)                       \
static rettype T1##_##T2##_##func(T2 *obj, argtype1 a)                        \
{ return T1##_##func(STATIC_UPCAST(T1, T2, obj), a); }

#define DECLARE_THUNK2(T1, T2, rettype, func, argtype1, argtype2)             \
static rettype T1##_##T2##_##func(T2 *obj, argtype1 a, argtype2 b)            \
{ return T1##_##func(STATIC_UPCAST(T1, T2, obj), a, b); }

#define DECLARE_THUNK3(T1, T2, rettype, func, argtype1, argtype2, argtype3)   \
static rettype T1##_##T2##_##func(T2 *obj, argtype1 a, argtype2 b, argtype3 c) \
{ return T1##_##func(STATIC_UPCAST(T1, T2, obj), a, b, c); }

#define DECLARE_THUNK4(T1, T2, rettype, func, argtype1, argtype2, argtype3, argtype4) \
static rettype T1##_##T2##_##func(T2 *obj, argtype1 a, argtype2 b, argtype3 c, argtype4 d) \
{ return T1##_##func(STATIC_UPCAST(T1, T2, obj), a, b, c, d); }

#define DECLARE_DEFAULT_ALLOCATORS(T)                                         \
static void* T##_New(size_t size) { return al_malloc(16, size); }             \
static void T##_Delete(void *ptr) { al_free(ptr); }

/* Helper to extract an argument list for VCALL. Not used directly. */
#define EXTRACT_VCALL_ARGS(...)  __VA_ARGS__))

/* Call a "virtual" method on an object, with arguments. */
#define V(obj, func)  ((obj)->vtbl->func((obj), EXTRACT_VCALL_ARGS
/* Call a "virtual" method on an object, with no arguments. */
#define V0(obj, func) ((obj)->vtbl->func((obj) EXTRACT_VCALL_ARGS

#define DELETE_OBJ(obj) do {                                                  \
    if((obj) != NULL)                                                         \
    {                                                                         \
        V0((obj),destruct)();                                                 \
        V0((obj),delete1)();                                                  \
    }                                                                         \
} while(0)


#define EXTRACT_NEW_ARGS(...)  __VA_ARGS__);                                  \
    }                                                                         \
} while(0)

#define NEW_OBJ(_res, T) do {                                                 \
    _res = T##_New(sizeof(T));                                                \
    if(_res)                                                                  \
    {                                                                         \
        memset(_res, 0, sizeof(T));                                           \
        T##_Construct(_res, EXTRACT_NEW_ARGS
#define NEW_OBJ0(_res, T) do {                                                \
    _res = T##_New(sizeof(T));                                                \
    if(_res)                                                                  \
    {                                                                         \
        memset(_res, 0, sizeof(T));                                           \
        T##_Construct(_res EXTRACT_NEW_ARGS


#ifdef __cplusplus
extern "C" {
#endif

struct Compressor;


#define DEFAULT_OUTPUT_RATE  (44100)


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

/* Scales the given value using 64-bit integer math, rounding the result. */
inline ALuint64 ScaleRound(ALuint64 val, ALuint64 new_scale, ALuint64 old_scale)
{
    return (val*new_scale + old_scale/2) / old_scale;
}

/* Scales the given value using 64-bit integer math, flooring the result. */
inline ALuint64 ScaleFloor(ALuint64 val, ALuint64 new_scale, ALuint64 old_scale)
{
    return val * new_scale / old_scale;
}

/* Scales the given value using 64-bit integer math, ceiling the result. */
inline ALuint64 ScaleCeil(ALuint64 val, ALuint64 new_scale, ALuint64 old_scale)
{
    return (val*new_scale + old_scale-1) / old_scale;
}

/* Fast float-to-int conversion. Assumes the FPU is already in round-to-zero
 * mode. */
inline ALint fastf2i(ALfloat f)
{
    return lrintf(f);
}


enum DevProbe {
    ALL_DEVICE_PROBE
};

struct ALCbackend;

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
#define MAX_OUTPUT_CHANNELS  (16)


extern const struct EffectList {
    const char *name;
    int type;
    const char *ename;
    ALenum val;
} EffectList[];


/* The maximum number of Ambisonics coefficients. For a given order (o), the
 * size needed will be (o+1)**2, thus zero-order has 1, first-order has 4,
 * second-order has 9, third-order has 16, and fourth-order has 25.
 */
#define MAX_AMBI_ORDER  3
#define MAX_AMBI_COEFFS ((MAX_AMBI_ORDER+1) * (MAX_AMBI_ORDER+1))

/* A bitmask of ambisonic channels with height information. If none of these
 * channels are used/needed, there's no height (e.g. with most surround sound
 * speaker setups). This only specifies up to 4th order, which is the highest
 * order a 32-bit mask value can specify (a 64-bit mask could handle up to 7th
 * order). This is ACN ordering, with bit 0 being ACN 0, etc.
 */
#define AMBI_PERIPHONIC_MASK (0xfe7ce4)

/* The maximum number of Ambisonic coefficients for 2D (non-periphonic)
 * representation. This is 2 per each order above zero-order, plus 1 for zero-
 * order. Or simply, o*2 + 1.
 */
#define MAX_AMBI2D_COEFFS (MAX_AMBI_ORDER*2 + 1)


typedef ALfloat ChannelConfig[MAX_AMBI_COEFFS];
typedef struct BFChannelConfig {
    ALfloat scale;
    ALsizei index;
} BFChannelConfig;

typedef union AmbiConfig {
    /* Ambisonic coefficients for mixing to the dry buffer. */
    ChannelConfig coeffs[MAX_OUTPUT_CHANNELS];
    /* Coefficient channel mapping for mixing to the dry buffer. */
    BFChannelConfig map[MAX_OUTPUT_CHANNELS];
} AmbiConfig;


/* Maximum delay in samples for speaker distance compensation. */
#define MAX_DELAY_LENGTH 1024

typedef struct DistanceComp {
    ALfloat gain;
    ALsizei length; /* Valid range is [0...MAX_DELAY_LENGTH). */
    ALfloat *buffer;
} DistanceComp;

/* Size for temporary storage of buffer data, in ALfloats. Larger values need
 * more memory, while smaller values may need more iterations. The value needs
 * to be a sensible size, however, as it constrains the max stepping value used
 * for mixing, as well as the maximum number of samples per mixing iteration.
 */
#define BUFFERSIZE 2048

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
    alignas(16) ALfloat source_data[BUFFERSIZE];
    alignas(16) ALfloat resampled_data[BUFFERSIZE];
    alignas(16) ALfloat filtered_data[BUFFERSIZE];

    /* The "dry" path corresponds to the main output. */
    struct Dry {
        AmbiConfig ambi;
        /* Number of coefficients in each Ambi.Coeffs to mix together (4 for
         * first-order, 9 for second-order, etc). If the count is 0, Ambi.Map
         * is used instead to map each output to a coefficient index.
         */
        ALsizei coeff_count;

        ALfloat (*buffer)[BUFFERSIZE];
        ALsizei num_channels;
        ALsizei num_channels_per_order[MAX_AMBI_ORDER+1];
    } dry;

    /* First-order ambisonics output, to be upsampled to the dry buffer if different. */
    struct FOAOut {
        AmbiConfig ambi;
        /* Will only be 4 or 0. */
        ALsizei coeff_count;

        ALfloat (*buffer)[BUFFERSIZE];
        ALsizei num_channels;
    } foa_out;

    /* "Real" output, which will be written to the device buffer. May alias the
     * dry buffer.
     */
    struct RealOut {
        enum Channel channel_name[MAX_OUTPUT_CHANNELS];

        ALfloat (*buffer)[BUFFERSIZE];
        ALsizei num_channels;
    } real_out;

    ALCcontext* context;

    struct ALsource* source;
    const ALfloat* source_samples;
    struct ALeffectslot* effect_slot;
    struct ALeffect* effect;
};

// Frequency was requested by the app or config file
#define DEVICE_FREQUENCY_REQUEST                 (1u<<1)
// Channel configuration was requested by the config file
#define DEVICE_CHANNELS_REQUEST                  (1u<<2)
// Sample type was requested by the config file
#define DEVICE_SAMPLE_TYPE_REQUEST               (1u<<3)

// Specifies if the DSP is paused at user request
#define DEVICE_PAUSED                            (1u<<30)

// Specifies if the device is currently running
#define DEVICE_RUNNING                           (1u<<31)


/* Must be less than 15 characters (16 including terminating null) for
 * compatibility with pthread_setname_np limitations. */
#define MIXER_THREAD_NAME "alsoft-mixer"

#define RECORD_THREAD_NAME "alsoft-record"


struct ALCcontext_struct {
    unsigned int ref;

    struct ALvoice *voice;
    ALsizei voice_count;

    struct ALeffectslotArray* active_aux_slots;

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
typedef ALfloat ALfloatBUFFERSIZE[BUFFERSIZE];
typedef ALfloat ALfloat2[2];


/* The compressor requires the following information for proper
 * initialization:
 *
 *   PreGainDb      - Gain applied before detection (in dB).
 *   PostGainDb     - Gain applied after compression (in dB).
 *   SummedLink     - Whether to use summed (true) or maxed (false) linking.
 *   RmsSensing     - Whether to use RMS (true) or Peak (false) sensing.
 *   AttackTimeMin  - Minimum attack time (in seconds).
 *   AttackTimeMax  - Maximum attack time.  Automates when min != max.
 *   ReleaseTimeMin - Minimum release time (in seconds).
 *   ReleaseTimeMax - Maximum release time.  Automates when min != max.
 *   Ratio          - Compression ratio (x:1).  Set to 0 for true limiter.
 *   ThresholdDb    - Triggering threshold (in dB).
 *   KneeDb         - Knee width (below threshold; in dB).
 *   SampleRate     - Sample rate to process.
 */
struct Compressor *CompressorInit(const ALfloat PreGainDb, const ALfloat PostGainDb,
    const ALboolean SummedLink, const ALboolean RmsSensing, const ALfloat AttackTimeMin,
    const ALfloat AttackTimeMax, const ALfloat ReleaseTimeMin, const ALfloat ReleaseTimeMax,
    const ALfloat Ratio, const ALfloat ThresholdDb, const ALfloat KneeDb,
    const ALuint SampleRate);

ALuint GetCompressorSampleRate(const struct Compressor *Comp);

void ApplyCompression(struct Compressor *Comp, const ALsizei NumChans, const ALsizei SamplesToDo,
                      ALfloat (*restrict OutBuffer)[BUFFERSIZE]);

#ifdef __cplusplus
}
#endif

#endif
