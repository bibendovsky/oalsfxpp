#ifndef AL_MAIN_H
#define AL_MAIN_H

#include <cstring>
#include <cassert>
#include <cmath>

#include "AL/al.h"
#include "AL/alc.h"
#include "AL/alext.h"

#include "almalloc.h"



/* Calculates the size of a struct with N elements of a flexible array member.
 * GCC and Clang allow offsetof(Type, fam[N]) for this, but MSVC seems to have
 * trouble, so a bit more verbose workaround is needed.
 */
#define FAM_SIZE(T, M, N)  (offsetof(T, M) + sizeof(((T*)NULL)->M[0])*(N))

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
    _res = static_cast<T*>(T##_New(sizeof(T)));                               \
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
#define MAX_OUTPUT_CHANNELS  (16)


/* The maximum number of Ambisonics coefficients. For a given order (o), the
 * size needed will be (o+1)**2, thus zero-order has 1, first-order has 4,
 * second-order has 9, third-order has 16, and fourth-order has 25.
 */
#define MAX_AMBI_ORDER  3
#define MAX_AMBI_COEFFS ((MAX_AMBI_ORDER+1) * (MAX_AMBI_ORDER+1))


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


#ifdef __cplusplus
}
#endif

#endif
