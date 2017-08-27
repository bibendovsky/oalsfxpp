/**
 * OpenAL cross platform audio library
 * Copyright (C) 1999-2007 by authors.
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 */

#include "config.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "alMain.h"
#include "alSource.h"
#include "alBuffer.h"
#include "alAuxEffectSlot.h"
#include "alu.h"
#include "bformatdec.h"
#include "static_assert.h"

#include "mixer_defs.h"


struct ChanMap {
    enum Channel channel;
    ALfloat angle;
    ALfloat elevation;
};

/* Cone scalar */
ALfloat ConeScale = 1.0f;

/* Localized Z scalar for mono sources */
ALfloat ZScale = 1.0f;

extern inline ALfloat minf(ALfloat a, ALfloat b);
extern inline ALfloat maxf(ALfloat a, ALfloat b);
extern inline ALfloat clampf(ALfloat val, ALfloat min, ALfloat max);

extern inline ALdouble mind(ALdouble a, ALdouble b);
extern inline ALdouble maxd(ALdouble a, ALdouble b);
extern inline ALdouble clampd(ALdouble val, ALdouble min, ALdouble max);

extern inline ALuint minu(ALuint a, ALuint b);
extern inline ALuint maxu(ALuint a, ALuint b);
extern inline ALuint clampu(ALuint val, ALuint min, ALuint max);

extern inline ALint mini(ALint a, ALint b);
extern inline ALint maxi(ALint a, ALint b);
extern inline ALint clampi(ALint val, ALint min, ALint max);

extern inline ALint64 mini64(ALint64 a, ALint64 b);
extern inline ALint64 maxi64(ALint64 a, ALint64 b);
extern inline ALint64 clampi64(ALint64 val, ALint64 min, ALint64 max);

extern inline ALuint64 minu64(ALuint64 a, ALuint64 b);
extern inline ALuint64 maxu64(ALuint64 a, ALuint64 b);
extern inline ALuint64 clampu64(ALuint64 val, ALuint64 min, ALuint64 max);

extern inline ALfloat lerp(ALfloat val1, ALfloat val2, ALfloat mu);
extern inline ALfloat resample_fir4(ALfloat val0, ALfloat val1, ALfloat val2, ALfloat val3, ALsizei frac);

extern inline void aluVectorSet(aluVector *restrict vector, ALfloat x, ALfloat y, ALfloat z, ALfloat w);

extern inline void aluMatrixfSetRow(aluMatrixf *matrix, ALuint row,
                                    ALfloat m0, ALfloat m1, ALfloat m2, ALfloat m3);
extern inline void aluMatrixfSet(aluMatrixf *matrix,
                                 ALfloat m00, ALfloat m01, ALfloat m02, ALfloat m03,
                                 ALfloat m10, ALfloat m11, ALfloat m12, ALfloat m13,
                                 ALfloat m20, ALfloat m21, ALfloat m22, ALfloat m23,
                                 ALfloat m30, ALfloat m31, ALfloat m32, ALfloat m33);

const aluMatrixf IdentityMatrixf = {{
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
}};


void DeinitVoice(ALvoice *voice)
{
}


/* Prior to VS2013, MSVC lacks the round() family of functions. */
#if defined(_MSC_VER) && _MSC_VER < 1800
static float roundf(float val)
{
    if(val < 0.0f)
        return ceilf(val-0.5f);
    return floorf(val+0.5f);
}
#endif

static inline void aluCrossproduct(const ALfloat *inVector1, const ALfloat *inVector2, ALfloat *outVector)
{
    outVector[0] = inVector1[1]*inVector2[2] - inVector1[2]*inVector2[1];
    outVector[1] = inVector1[2]*inVector2[0] - inVector1[0]*inVector2[2];
    outVector[2] = inVector1[0]*inVector2[1] - inVector1[1]*inVector2[0];
}

static inline ALfloat aluDotproduct(const aluVector *vec1, const aluVector *vec2)
{
    return vec1->v[0]*vec2->v[0] + vec1->v[1]*vec2->v[1] + vec1->v[2]*vec2->v[2];
}

static ALfloat aluNormalize(ALfloat *vec)
{
    ALfloat length = sqrtf(vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2]);
    if(length > 0.0f)
    {
        ALfloat inv_length = 1.0f/length;
        vec[0] *= inv_length;
        vec[1] *= inv_length;
        vec[2] *= inv_length;
    }
    return length;
}

static void aluMatrixfFloat3(ALfloat *vec, ALfloat w, const aluMatrixf *mtx)
{
    ALfloat v[4] = { vec[0], vec[1], vec[2], w };

    vec[0] = v[0]*mtx->m[0][0] + v[1]*mtx->m[1][0] + v[2]*mtx->m[2][0] + v[3]*mtx->m[3][0];
    vec[1] = v[0]*mtx->m[0][1] + v[1]*mtx->m[1][1] + v[2]*mtx->m[2][1] + v[3]*mtx->m[3][1];
    vec[2] = v[0]*mtx->m[0][2] + v[1]*mtx->m[1][2] + v[2]*mtx->m[2][2] + v[3]*mtx->m[3][2];
}

static aluVector aluMatrixfVector(const aluMatrixf *mtx, const aluVector *vec)
{
    aluVector v;
    v.v[0] = vec->v[0]*mtx->m[0][0] + vec->v[1]*mtx->m[1][0] + vec->v[2]*mtx->m[2][0] + vec->v[3]*mtx->m[3][0];
    v.v[1] = vec->v[0]*mtx->m[0][1] + vec->v[1]*mtx->m[1][1] + vec->v[2]*mtx->m[2][1] + vec->v[3]*mtx->m[3][1];
    v.v[2] = vec->v[0]*mtx->m[0][2] + vec->v[1]*mtx->m[1][2] + vec->v[2]*mtx->m[2][2] + vec->v[3]*mtx->m[3][2];
    v.v[3] = vec->v[0]*mtx->m[0][3] + vec->v[1]*mtx->m[1][3] + vec->v[2]*mtx->m[2][3] + vec->v[3]*mtx->m[3][3];
    return v;
}


static ALboolean CalcEffectSlotParams(ALeffectslot *slot, ALCdevice *device)
{
    struct ALeffectslotProps *props;
    ALeffectState *state;

    props = slot->Update;
    slot->Update = NULL;
    if(!props) return AL_FALSE;

    slot->Params.EffectType = props->Type;

    /* Swap effect states. No need to play with the ref counts since they keep
     * the same number of refs.
     */
    state = props->State;
    props->State = slot->Params.EffectState;
    slot->Params.EffectState = state;

    V(state,update)(device, slot, &props->Props);

    props->next = slot->FreeList;
    return AL_TRUE;
}


static const struct ChanMap MonoMap[1] = {
    { FrontCenter, 0.0f, 0.0f }
}, RearMap[2] = {
    { BackLeft,  DEG2RAD(-150.0f), DEG2RAD(0.0f) },
    { BackRight, DEG2RAD( 150.0f), DEG2RAD(0.0f) }
}, QuadMap[4] = {
    { FrontLeft,  DEG2RAD( -45.0f), DEG2RAD(0.0f) },
    { FrontRight, DEG2RAD(  45.0f), DEG2RAD(0.0f) },
    { BackLeft,   DEG2RAD(-135.0f), DEG2RAD(0.0f) },
    { BackRight,  DEG2RAD( 135.0f), DEG2RAD(0.0f) }
}, X51Map[6] = {
    { FrontLeft,   DEG2RAD( -30.0f), DEG2RAD(0.0f) },
    { FrontRight,  DEG2RAD(  30.0f), DEG2RAD(0.0f) },
    { FrontCenter, DEG2RAD(   0.0f), DEG2RAD(0.0f) },
    { LFE, 0.0f, 0.0f },
    { SideLeft,    DEG2RAD(-110.0f), DEG2RAD(0.0f) },
    { SideRight,   DEG2RAD( 110.0f), DEG2RAD(0.0f) }
}, X61Map[7] = {
    { FrontLeft,    DEG2RAD(-30.0f), DEG2RAD(0.0f) },
    { FrontRight,   DEG2RAD( 30.0f), DEG2RAD(0.0f) },
    { FrontCenter,  DEG2RAD(  0.0f), DEG2RAD(0.0f) },
    { LFE, 0.0f, 0.0f },
    { BackCenter,   DEG2RAD(180.0f), DEG2RAD(0.0f) },
    { SideLeft,     DEG2RAD(-90.0f), DEG2RAD(0.0f) },
    { SideRight,    DEG2RAD( 90.0f), DEG2RAD(0.0f) }
}, X71Map[8] = {
    { FrontLeft,   DEG2RAD( -30.0f), DEG2RAD(0.0f) },
    { FrontRight,  DEG2RAD(  30.0f), DEG2RAD(0.0f) },
    { FrontCenter, DEG2RAD(   0.0f), DEG2RAD(0.0f) },
    { LFE, 0.0f, 0.0f },
    { BackLeft,    DEG2RAD(-150.0f), DEG2RAD(0.0f) },
    { BackRight,   DEG2RAD( 150.0f), DEG2RAD(0.0f) },
    { SideLeft,    DEG2RAD( -90.0f), DEG2RAD(0.0f) },
    { SideRight,   DEG2RAD(  90.0f), DEG2RAD(0.0f) }
};

static void CalcPanningAndFilters(ALvoice *voice, const ALfloat Distance, const ALfloat *Dir,
                                  const ALfloat Spread, const ALfloat DryGain,
                                  const ALfloat DryGainHF, const ALfloat DryGainLF,
                                  const ALfloat *WetGain, const ALfloat *WetGainLF,
                                  const ALfloat *WetGainHF, ALeffectslot **SendSlots,
                                  const struct ALvoiceProps *props,
                                  const ALCdevice *Device)
{
    struct ChanMap StereoMap[2] = {
        { FrontLeft,  DEG2RAD(-30.0f), DEG2RAD(0.0f) },
        { FrontRight, DEG2RAD( 30.0f), DEG2RAD(0.0f) }
    };
    bool DirectChannels = AL_FALSE;
    const ALsizei NumSends = Device->NumAuxSends;
    const ALuint Frequency = Device->Frequency;
    const struct ChanMap *chans = NULL;
    ALsizei num_channels = 0;
    bool isbformat = false;
    ALfloat downmix_gain = 1.0f;
    ALsizei c, i, j;

    switch(Device->FmtChans)
    {
    case FmtMono:
        chans = MonoMap;
        num_channels = 1;
        /* Mono buffers are never played direct. */
        DirectChannels = false;
        break;

    case FmtStereo:
        /* Convert counter-clockwise to clockwise. */
        StereoMap[0].angle = -props->StereoPan[0];
        StereoMap[1].angle = -props->StereoPan[1];

        chans = StereoMap;
        num_channels = 2;
        downmix_gain = 1.0f / 2.0f;
        break;
    }

    /* Non-HRTF rendering. Use normal panning to the output. */

    {
        ALfloat w0 = 0.0f;

        for(c = 0;c < num_channels;c++)
        {
            ALfloat coeffs[MAX_AMBI_COEFFS];

            /* Special-case LFE */
            if(chans[c].channel == LFE)
            {
                for(j = 0;j < MAX_OUTPUT_CHANNELS;j++)
                    voice->Direct.Params[c].Gains.Target[j] = 0.0f;
                if(Device->Dry.Buffer == Device->RealOut.Buffer)
                {
                    int idx = GetChannelIdxByName(Device->RealOut, chans[c].channel);
                    if(idx != -1) voice->Direct.Params[c].Gains.Target[idx] = DryGain;
                }

                for(i = 0;i < NumSends;i++)
                {
                    for(j = 0;j < MAX_EFFECT_CHANNELS;j++)
                        voice->Send[i].Params[c].Gains.Target[j] = 0.0f;
                }
                continue;
            }

            CalcAngleCoeffs(chans[c].angle, chans[c].elevation, Spread, coeffs);
            ComputePanningGains(Device->Dry,
                coeffs, DryGain, voice->Direct.Params[c].Gains.Target
            );

            for(i = 0;i < NumSends;i++)
            {
                const ALeffectslot *Slot = SendSlots[i];
                if(Slot)
                    ComputePanningGainsBF(Slot->ChanMap, Slot->NumChannels,
                        coeffs, WetGain[i], voice->Send[i].Params[c].Gains.Target
                    );
                else
                    for(j = 0;j < MAX_EFFECT_CHANNELS;j++)
                        voice->Send[i].Params[c].Gains.Target[j] = 0.0f;
            }
        }
    }

    {
        ALfloat hfScale = props->Direct.HFReference / Frequency;
        ALfloat lfScale = props->Direct.LFReference / Frequency;
        ALfloat gainHF = maxf(DryGainHF, 0.001f); /* Limit -60dB */
        ALfloat gainLF = maxf(DryGainLF, 0.001f);

        voice->Direct.FilterType = AF_None;
        if(gainHF != 1.0f) voice->Direct.FilterType |= AF_LowPass;
        if(gainLF != 1.0f) voice->Direct.FilterType |= AF_HighPass;
        ALfilterState_setParams(
            &voice->Direct.Params[0].LowPass, ALfilterType_HighShelf,
            gainHF, hfScale, calc_rcpQ_from_slope(gainHF, 1.0f)
        );
        ALfilterState_setParams(
            &voice->Direct.Params[0].HighPass, ALfilterType_LowShelf,
            gainLF, lfScale, calc_rcpQ_from_slope(gainLF, 1.0f)
        );
        for(c = 1;c < num_channels;c++)
        {
            ALfilterState_copyParams(&voice->Direct.Params[c].LowPass,
                                     &voice->Direct.Params[0].LowPass);
            ALfilterState_copyParams(&voice->Direct.Params[c].HighPass,
                                     &voice->Direct.Params[0].HighPass);
        }
    }
    for(i = 0;i < NumSends;i++)
    {
        ALfloat hfScale = props->Send[i].HFReference / Frequency;
        ALfloat lfScale = props->Send[i].LFReference / Frequency;
        ALfloat gainHF = maxf(WetGainHF[i], 0.001f);
        ALfloat gainLF = maxf(WetGainLF[i], 0.001f);

        voice->Send[i].FilterType = AF_None;
        if(gainHF != 1.0f) voice->Send[i].FilterType |= AF_LowPass;
        if(gainLF != 1.0f) voice->Send[i].FilterType |= AF_HighPass;
        ALfilterState_setParams(
            &voice->Send[i].Params[0].LowPass, ALfilterType_HighShelf,
            gainHF, hfScale, calc_rcpQ_from_slope(gainHF, 1.0f)
        );
        ALfilterState_setParams(
            &voice->Send[i].Params[0].HighPass, ALfilterType_LowShelf,
            gainLF, lfScale, calc_rcpQ_from_slope(gainLF, 1.0f)
        );
        for(c = 1;c < num_channels;c++)
        {
            ALfilterState_copyParams(&voice->Send[i].Params[c].LowPass,
                                     &voice->Send[i].Params[0].LowPass);
            ALfilterState_copyParams(&voice->Send[i].Params[c].HighPass,
                                     &voice->Send[i].Params[0].HighPass);
        }
    }
}

static void CalcNonAttnSourceParams(ALvoice *voice, const struct ALvoiceProps *props, const ALCcontext *ALContext)
{
    static const ALfloat dir[3] = { 0.0f, 0.0f, -1.0f };
    const ALCdevice *Device = ALContext->Device;
    ALfloat DryGain, DryGainHF, DryGainLF;
    ALfloat WetGain[MAX_SENDS];
    ALfloat WetGainHF[MAX_SENDS];
    ALfloat WetGainLF[MAX_SENDS];
    ALeffectslot *SendSlots[MAX_SENDS];
    ALsizei i;

    voice->Direct.Buffer = Device->Dry.Buffer;
    voice->Direct.Channels = Device->Dry.NumChannels;
    for(i = 0;i < Device->NumAuxSends;i++)
    {
        SendSlots[i] = props->Send[i].Slot;
        if(!SendSlots[i] || SendSlots[i]->Params.EffectType == AL_EFFECT_NULL)
        {
            SendSlots[i] = NULL;
            voice->Send[i].Buffer = NULL;
            voice->Send[i].Channels = 0;
        }
        else
        {
            voice->Send[i].Buffer = SendSlots[i]->WetBuffer;
            voice->Send[i].Channels = SendSlots[i]->NumChannels;
        }
    }

    /* Calculate gains */
    DryGain  = 1.0F;
    DryGain *= props->Direct.Gain;
    DryGain  = minf(DryGain, GAIN_MIX_MAX);
    DryGainHF = props->Direct.GainHF;
    DryGainLF = props->Direct.GainLF;
    for(i = 0;i < Device->NumAuxSends;i++)
    {
        WetGain[i]  = 1.0F;
        WetGain[i] *= props->Send[i].Gain;
        WetGain[i]  = minf(WetGain[i], GAIN_MIX_MAX);
        WetGainHF[i] = props->Send[i].GainHF;
        WetGainLF[i] = props->Send[i].GainLF;
    }

    CalcPanningAndFilters(voice, 0.0f, dir, 0.0f, DryGain, DryGainHF, DryGainLF, WetGain,
                          WetGainLF, WetGainHF, SendSlots, props, Device);
}

static void CalcSourceParams(ALvoice *voice, ALCcontext *context, ALboolean force)
{
    CalcNonAttnSourceParams(voice, voice->Props, context);
}


static void UpdateContextSources(ALCcontext *ctx, const struct ALeffectslotArray *slots)
{
    ALvoice *voice;
    ALsource *source;
    ALsizei i;

    ALboolean force = AL_TRUE;
    for(i = 0;i < slots->count;i++)
        force |= CalcEffectSlotParams(slots->slot[i], ctx->Device);

    voice = ctx->voice;
    source = voice->Source;
    if(source) CalcSourceParams(voice, ctx, force);
}


static void ApplyDistanceComp(ALfloatBUFFERSIZE *restrict Samples, DistanceComp *distcomp,
                              ALfloat *restrict Values, ALsizei SamplesToDo, ALsizei numchans)
{
    ALsizei i, c;

    Values = ASSUME_ALIGNED(Values, 16);
    for(c = 0;c < numchans;c++)
    {
        ALfloat *restrict inout = ASSUME_ALIGNED(Samples[c], 16);
        const ALfloat gain = distcomp[c].Gain;
        const ALsizei base = distcomp[c].Length;
        ALfloat *restrict distbuf = ASSUME_ALIGNED(distcomp[c].Buffer, 16);

        if(base == 0)
        {
            if(gain < 1.0f)
            {
                for(i = 0;i < SamplesToDo;i++)
                    inout[i] *= gain;
            }
            continue;
        }

        if(SamplesToDo >= base)
        {
            for(i = 0;i < base;i++)
                Values[i] = distbuf[i];
            for(;i < SamplesToDo;i++)
                Values[i] = inout[i-base];
            memcpy(distbuf, &inout[SamplesToDo-base], base*sizeof(ALfloat));
        }
        else
        {
            for(i = 0;i < SamplesToDo;i++)
                Values[i] = distbuf[i];
            memmove(distbuf, distbuf+SamplesToDo, (base-SamplesToDo)*sizeof(ALfloat));
            memcpy(distbuf+base-SamplesToDo, inout, SamplesToDo*sizeof(ALfloat));
        }
        for(i = 0;i < SamplesToDo;i++)
            inout[i] = Values[i]*gain;
    }
}

static inline ALfloat Conv_ALfloat(ALfloat val)
{ return val; }
static inline ALint Conv_ALint(ALfloat val)
{
    /* Floats only have a 24-bit mantissa, so [-16777216, +16777216] is the max
     * integer range normalized floats can be safely converted to (a bit of the
     * exponent helps out, effectively giving 25 bits).
     */
    return fastf2i(clampf(val*16777216.0f, -16777216.0f, 16777215.0f))<<7;
}
static inline ALshort Conv_ALshort(ALfloat val)
{ return fastf2i(clampf(val*32768.0f, -32768.0f, 32767.0f)); }
static inline ALbyte Conv_ALbyte(ALfloat val)
{ return fastf2i(clampf(val*128.0f, -128.0f, 127.0f)); }

/* Define unsigned output variations. */
#define DECL_TEMPLATE(T, func, O)                             \
static inline T Conv_##T(ALfloat val) { return func(val)+O; }

DECL_TEMPLATE(ALubyte, Conv_ALbyte, 128)
DECL_TEMPLATE(ALushort, Conv_ALshort, 32768)
DECL_TEMPLATE(ALuint, Conv_ALint, 2147483648u)

#undef DECL_TEMPLATE

#define DECL_TEMPLATE(T, A)                                                   \
static void Write##A(const ALfloatBUFFERSIZE *InBuffer, ALvoid *OutBuffer,    \
                     ALsizei Offset, ALsizei SamplesToDo, ALsizei numchans)   \
{                                                                             \
    ALsizei i, j;                                                             \
    for(j = 0;j < numchans;j++)                                               \
    {                                                                         \
        const ALfloat *restrict in = ASSUME_ALIGNED(InBuffer[j], 16);         \
        T *restrict out = (T*)OutBuffer + Offset*numchans + j;                \
                                                                              \
        for(i = 0;i < SamplesToDo;i++)                                        \
            out[i*numchans] = Conv_##T(in[i]);                                \
    }                                                                         \
}

DECL_TEMPLATE(ALfloat, F32)

#undef DECL_TEMPLATE


void aluMixData(ALCdevice *device, ALvoid *OutBuffer, ALsizei NumSamples, const ALfloat* src_samples)
{
    ALsizei SamplesToDo;
    ALsizei SamplesDone;
    ALCcontext *ctx;
    ALsizei i, c;

    device->source_data = src_samples;

    for(SamplesDone = 0;SamplesDone < NumSamples;)
    {
        SamplesToDo = mini(NumSamples-SamplesDone, BUFFERSIZE);
        for(c = 0;c < device->Dry.NumChannels;c++)
            memset(device->Dry.Buffer[c], 0, SamplesToDo*sizeof(ALfloat));
        if(device->Dry.Buffer != device->FOAOut.Buffer)
            for(c = 0;c < device->FOAOut.NumChannels;c++)
                memset(device->FOAOut.Buffer[c], 0, SamplesToDo*sizeof(ALfloat));
        if(device->Dry.Buffer != device->RealOut.Buffer)
            for(c = 0;c < device->RealOut.NumChannels;c++)
                memset(device->RealOut.Buffer[c], 0, SamplesToDo*sizeof(ALfloat));

        ctx = device->context;
        if(ctx)
        {
            const struct ALeffectslotArray *auxslots;

            auxslots = ctx->ActiveAuxSlots;
            UpdateContextSources(ctx, auxslots);

            for(i = 0;i < auxslots->count;i++)
            {
                ALeffectslot *slot = auxslots->slot[i];
                for(c = 0;c < slot->NumChannels;c++)
                    memset(slot->WetBuffer[c], 0, SamplesToDo*sizeof(ALfloat));
            }

            /* source processing */
            for(i = 0;i < ctx->VoiceCount;i++)
            {
                ALvoice *voice = ctx->voice;
                ALsource *source = voice->Source;
                if(source && voice->Playing)
                {
                    if(!MixSource(voice, source, device, SamplesToDo))
                    {
                        voice->Source = NULL;
                        voice->Playing = false;
                    }
                }
            }

            /* effect slot processing */
            for(i = 0;i < auxslots->count;i++)
            {
                const ALeffectslot *slot = auxslots->slot[i];
                ALeffectState *state = slot->Params.EffectState;
                V(state,process)(SamplesToDo, slot->WetBuffer, state->OutBuffer,
                                 state->OutChannels);
            }
        }

        if(OutBuffer)
        {
            ALfloat (*Buffer)[BUFFERSIZE] = device->RealOut.Buffer;
            ALsizei Channels = device->RealOut.NumChannels;
            WriteF32(Buffer, OutBuffer, SamplesDone, SamplesToDo, Channels);
        }

        SamplesDone += SamplesToDo;
    }
}


void aluHandleDisconnect(ALCdevice *device)
{
    ALCcontext *ctx;

    ctx = device->context;
    if(ctx)
    {
        ALsizei i;
        for(i = 0;i < ctx->VoiceCount;i++)
        {
            ALvoice *voice = ctx->voice;
            ALsource *source;

            source = voice->Source;
            voice->Source = NULL;
            voice->Playing = false;

            if(source)
            {
                if (source->state == AL_PLAYING)
                {
                    source->state = AL_STOPPED;
                }
            }
        }
        ctx->VoiceCount = 0;
    }
}
