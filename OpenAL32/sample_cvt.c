
#include "config.h"

#include "sample_cvt.h"

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "AL/al.h"
#include "alu.h"
#include "alBuffer.h"


/* Define same-type pass-through sample conversion functions (excludes ADPCM,
 * which are block-based). */
#define DECL_TEMPLATE(T) \
static inline T Conv_##T##_##T(T val) { return val; }

DECL_TEMPLATE(ALbyte);
DECL_TEMPLATE(ALubyte);
DECL_TEMPLATE(ALshort);
DECL_TEMPLATE(ALushort);

/* Slightly special handling for floats and doubles (converts NaN to 0, and
 * allows float<->double pass-through).
 */
static inline ALfloat Conv_ALfloat_ALfloat(ALfloat val)
{ return (val==val) ? val : 0.0f; }
static inline ALfloat Conv_ALfloat_ALdouble(ALdouble val)
{ return (val==val) ? (ALfloat)val : 0.0f; }
static inline ALdouble Conv_ALdouble_ALfloat(ALfloat val)
{ return (val==val) ? (ALdouble)val : 0.0; }
static inline ALdouble Conv_ALdouble_ALdouble(ALdouble val)
{ return (val==val) ? val : 0.0; }

#undef DECL_TEMPLATE

/* Define alternate-sign functions. */
#define DECL_TEMPLATE(T1, T2, O)                                  \
static inline T1 Conv_##T1##_##T2(T2 val) { return (T1)val - O; } \
static inline T2 Conv_##T2##_##T1(T1 val) { return (T2)val + O; }

DECL_TEMPLATE(ALbyte, ALubyte, 128);
DECL_TEMPLATE(ALshort, ALushort, 32768);

#undef DECL_TEMPLATE

/* Define int-type to int-type functions */
#define DECL_TEMPLATE(T, ST, UT, SH)                                          \
static inline T Conv_##T##_##ST(ST val){ return val >> SH; }                  \
static inline T Conv_##T##_##UT(UT val){ return Conv_##ST##_##UT(val) >> SH; }\
static inline ST Conv_##ST##_##T(T val){ return val << SH; }                  \
static inline UT Conv_##UT##_##T(T val){ return Conv_##UT##_##ST(val << SH); }

#define DECL_TEMPLATE2(T1, T2, SH)          \
DECL_TEMPLATE(AL##T1, AL##T2, ALu##T2, SH)  \
DECL_TEMPLATE(ALu##T1, ALu##T2, AL##T2, SH)

DECL_TEMPLATE2(byte,  short, 8)

#undef DECL_TEMPLATE2
#undef DECL_TEMPLATE

/* Define int-type to fp functions */
#define DECL_TEMPLATE(T, ST, UT, OP)                                          \
static inline T Conv_##T##_##ST(ST val) { return (T)val * OP; }               \
static inline T Conv_##T##_##UT(UT val) { return (T)Conv_##ST##_##UT(val) * OP; }

#define DECL_TEMPLATE2(T1, T2, OP)     \
DECL_TEMPLATE(T1, AL##T2, ALu##T2, OP)

DECL_TEMPLATE2(ALfloat, byte, (1.0f/128.0f))
DECL_TEMPLATE2(ALfloat, short, (1.0f/32768.0f))

#undef DECL_TEMPLATE2
#undef DECL_TEMPLATE

/* Define fp to int-type functions */
#define DECL_TEMPLATE(FT, T, smin, smax)        \
static inline AL##T Conv_AL##T##_##FT(FT val)   \
{                                               \
    val *= (FT)smax + 1;                        \
    if(val >= (FT)smax) return smax;            \
    if(val <= (FT)smin) return smin;            \
    return (AL##T)val;                          \
}                                               \
static inline ALu##T Conv_ALu##T##_##FT(FT val) \
{ return Conv_ALu##T##_AL##T(Conv_AL##T##_##FT(val)); }

DECL_TEMPLATE(ALfloat, byte, -128, 127)
DECL_TEMPLATE(ALfloat, short, -32768, 32767)

#undef DECL_TEMPLATE


#define DECL_TEMPLATE(T1, T2)                                                 \
static void Convert_##T1##_##T2(T1 *dst, const T2 *src, ALuint numchans,      \
                                ALuint len, ALsizei UNUSED(align))            \
{                                                                             \
    ALuint i, j;                                                              \
    for(i = 0;i < len;i++)                                                    \
    {                                                                         \
        for(j = 0;j < numchans;j++)                                           \
            *(dst++) = Conv_##T1##_##T2(*(src++));                            \
    }                                                                         \
}

#define DECL_TEMPLATE2(T)  \
DECL_TEMPLATE(T, ALbyte)   \
DECL_TEMPLATE(T, ALubyte)  \
DECL_TEMPLATE(T, ALshort)  \
DECL_TEMPLATE(T, ALushort) \
DECL_TEMPLATE(T, ALfloat)

DECL_TEMPLATE2(ALbyte)
DECL_TEMPLATE2(ALubyte)
DECL_TEMPLATE2(ALshort)
DECL_TEMPLATE2(ALushort)
DECL_TEMPLATE2(ALfloat)

#undef DECL_TEMPLATE2
#undef DECL_TEMPLATE


#define DECL_TEMPLATE(T)                                                      \
static void Convert_##T(T *dst, const ALvoid *src, enum UserFmtType srcType,  \
                        ALsizei numchans, ALsizei len, ALsizei align)         \
{                                                                             \
    switch(srcType)                                                           \
    {                                                                         \
        case UserFmtByte:                                                     \
            Convert_##T##_ALbyte(dst, src, numchans, len, align);             \
            break;                                                            \
        case UserFmtUByte:                                                    \
            Convert_##T##_ALubyte(dst, src, numchans, len, align);            \
            break;                                                            \
        case UserFmtShort:                                                    \
            Convert_##T##_ALshort(dst, src, numchans, len, align);            \
            break;                                                            \
        case UserFmtUShort:                                                   \
            Convert_##T##_ALushort(dst, src, numchans, len, align);           \
            break;                                                            \
        case UserFmtFloat:                                                    \
            Convert_##T##_ALfloat(dst, src, numchans, len, align);            \
            break;                                                            \
    }                                                                         \
}

DECL_TEMPLATE(ALbyte)
DECL_TEMPLATE(ALubyte)
DECL_TEMPLATE(ALshort)
DECL_TEMPLATE(ALushort)
DECL_TEMPLATE(ALfloat)

#undef DECL_TEMPLATE


void ConvertData(ALvoid *dst, enum UserFmtType dstType, const ALvoid *src, enum UserFmtType srcType, ALsizei numchans, ALsizei len, ALsizei align)
{
    switch(dstType)
    {
        case UserFmtByte:
            Convert_ALbyte(dst, src, srcType, numchans, len, align);
            break;
        case UserFmtUByte:
            Convert_ALubyte(dst, src, srcType, numchans, len, align);
            break;
        case UserFmtShort:
            Convert_ALshort(dst, src, srcType, numchans, len, align);
            break;
        case UserFmtUShort:
            Convert_ALushort(dst, src, srcType, numchans, len, align);
            break;
        case UserFmtFloat:
            Convert_ALfloat(dst, src, srcType, numchans, len, align);
            break;
    }
}
