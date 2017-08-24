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
#include "AL/al.h"
#include "AL/alc.h"
#include "alSource.h"
#include "alBuffer.h"
#include "alListener.h"
#include "alAuxEffectSlot.h"
#include "alu.h"

#include "mixer_defs.h"


static_assert((INT_MAX>>FRACTIONBITS)/MAX_PITCH > BUFFERSIZE,
              "MAX_PITCH and/or BUFFERSIZE are too large for FRACTIONBITS!");

extern inline void InitiatePositionArrays(ALsizei frac, ALint increment, ALsizei *restrict frac_arr, ALint *restrict pos_arr, ALsizei size);


enum Resampler ResamplerDefault = PointResampler;

static MixerFunc MixSamples = Mix_C;

MixerFunc SelectMixer(void)
{
#ifdef HAVE_NEON
    if((CPUCapFlags&CPU_CAP_NEON))
        return Mix_Neon;
#endif
#ifdef HAVE_SSE
    if((CPUCapFlags&CPU_CAP_SSE))
        return Mix_SSE;
#endif
    return Mix_C;
}

RowMixerFunc SelectRowMixer(void)
{
#ifdef HAVE_NEON
    if((CPUCapFlags&CPU_CAP_NEON))
        return MixRow_Neon;
#endif
#ifdef HAVE_SSE
    if((CPUCapFlags&CPU_CAP_SSE))
        return MixRow_SSE;
#endif
    return MixRow_C;
}

ResamplerFunc SelectResampler(enum Resampler resampler)
{
    switch(resampler)
    {
        case PointResampler:
            return Resample_point32_C;
        case LinearResampler:
#ifdef HAVE_NEON
            if((CPUCapFlags&CPU_CAP_NEON))
                return Resample_lerp32_Neon;
#endif
#ifdef HAVE_SSE4_1
            if((CPUCapFlags&CPU_CAP_SSE4_1))
                return Resample_lerp32_SSE41;
#endif
#ifdef HAVE_SSE2
            if((CPUCapFlags&CPU_CAP_SSE2))
                return Resample_lerp32_SSE2;
#endif
            return Resample_lerp32_C;
        case FIR4Resampler:
#ifdef HAVE_NEON
            if((CPUCapFlags&CPU_CAP_NEON))
                return Resample_fir4_32_Neon;
#endif
#ifdef HAVE_SSE4_1
            if((CPUCapFlags&CPU_CAP_SSE4_1))
                return Resample_fir4_32_SSE41;
#endif
#ifdef HAVE_SSE3
            if((CPUCapFlags&CPU_CAP_SSE3))
                return Resample_fir4_32_SSE3;
#endif
            return Resample_fir4_32_C;
        case BSincResampler:
#ifdef HAVE_NEON
            if((CPUCapFlags&CPU_CAP_NEON))
                return Resample_bsinc32_Neon;
#endif
#ifdef HAVE_SSE
            if((CPUCapFlags&CPU_CAP_SSE))
                return Resample_bsinc32_SSE;
#endif
            return Resample_bsinc32_C;
    }

    return Resample_point32_C;
}


void aluInitMixer(void)
{
    MixSamples = SelectMixer();
}


static inline ALfloat Sample_ALbyte(ALbyte val)
{ return val * (1.0f/128.0f); }

static inline ALfloat Sample_ALshort(ALshort val)
{ return val * (1.0f/32768.0f); }

static inline ALfloat Sample_ALfloat(ALfloat val)
{ return val; }

#define DECL_TEMPLATE(T)                                                      \
static inline void Load_##T(ALfloat *dst, const T *src, ALint srcstep, ALsizei samples)\
{                                                                             \
    ALsizei i;                                                                \
    for(i = 0;i < samples;i++)                                                \
        dst[i] = Sample_##T(src[i*srcstep]);                                  \
}

DECL_TEMPLATE(ALbyte)
DECL_TEMPLATE(ALshort)
DECL_TEMPLATE(ALfloat)

#undef DECL_TEMPLATE

static void LoadSamples(ALfloat *dst, const ALvoid *src, ALint srcstep, enum FmtType srctype, ALsizei samples)
{
    switch(srctype)
    {
        case FmtByte:
            Load_ALbyte(dst, src, srcstep, samples);
            break;
        case FmtShort:
            Load_ALshort(dst, src, srcstep, samples);
            break;
        case FmtFloat:
            Load_ALfloat(dst, src, srcstep, samples);
            break;
    }
}

static inline void SilenceSamples(ALfloat *dst, ALsizei samples)
{
    ALsizei i;
    for(i = 0;i < samples;i++)
        dst[i] = 0.0f;
}


static const ALfloat *DoFilters(ALfilterState *lpfilter, ALfilterState *hpfilter,
                                ALfloat *restrict dst, const ALfloat *restrict src,
                                ALsizei numsamples, enum ActiveFilters type)
{
    ALsizei i;
    switch(type)
    {
        case AF_None:
            ALfilterState_processPassthru(lpfilter, src, numsamples);
            ALfilterState_processPassthru(hpfilter, src, numsamples);
            break;

        case AF_LowPass:
            ALfilterState_process(lpfilter, dst, src, numsamples);
            ALfilterState_processPassthru(hpfilter, dst, numsamples);
            return dst;
        case AF_HighPass:
            ALfilterState_processPassthru(lpfilter, src, numsamples);
            ALfilterState_process(hpfilter, dst, src, numsamples);
            return dst;

        case AF_BandPass:
            for(i = 0;i < numsamples;)
            {
                ALfloat temp[256];
                ALsizei todo = mini(256, numsamples-i);

                ALfilterState_process(lpfilter, temp, src+i, todo);
                ALfilterState_process(hpfilter, dst+i, temp, todo);
                i += todo;
            }
            return dst;
    }
    return src;
}


ALboolean MixSource(ALvoice *voice, ALsource *Source, ALCdevice *Device, ALsizei SamplesToDo)
{
    ALbufferlistitem *BufferListItem;
    ALbufferlistitem *BufferLoopItem;
    ALsizei NumChannels, SampleSize;
    ResamplerFunc Resample;
    ALsizei DataPosInt;
    ALsizei DataPosFrac;
    ALint64 DataSize64;
    ALint increment;
    ALsizei Counter;
    ALsizei OutPos;
    ALsizei IrSize;
    bool isplaying;
    bool firstpass;
    ALsizei chan;
    ALsizei send;

    /* Get source info */
    isplaying      = true; /* Will only be called while playing. */
    DataPosInt     = voice->position;
    DataPosFrac    = voice->position_fraction;
    BufferListItem = voice->current_buffer;
    BufferLoopItem = voice->loop_buffer;
    NumChannels    = voice->NumChannels;
    SampleSize     = voice->SampleSize;
    increment      = voice->Step;

    IrSize = 0;

    Resample = ((increment == FRACTIONONE && DataPosFrac == 0) ?
                Resample_copy32_C : voice->Resampler);

    Counter = (voice->Flags&VOICE_IS_FADING) ? SamplesToDo : 0;
    firstpass = true;
    OutPos = 0;

    {
        ALsizei SrcBufferSize, DstBufferSize;

        /* Figure out how many buffer samples will be needed */
        DataSize64  = SamplesToDo-OutPos;
        DataSize64 *= increment;
        DataSize64 += DataPosFrac+FRACTIONMASK;
        DataSize64 >>= FRACTIONBITS;

        SrcBufferSize = (ALsizei)mini64(DataSize64, BUFFERSIZE);

        /* Figure out how many samples we can actually mix from this. */
        DataSize64  = SrcBufferSize;
        DataSize64 <<= FRACTIONBITS;
        DataSize64 -= DataPosFrac;

        DstBufferSize = (ALsizei)((DataSize64+(increment-1)) / increment);
        DstBufferSize = mini(DstBufferSize, (SamplesToDo-OutPos));

        /* Some mixers like having a multiple of 4, so try to give that unless
         * this is the last update. */
        if(OutPos+DstBufferSize < SamplesToDo)
            DstBufferSize &= ~3;

        for(chan = 0;chan < NumChannels;chan++)
        {
            const ALfloat *ResampledData;
            ALfloat *SrcData = Device->SourceData;
            ALsizei SrcDataSize;

            SrcDataSize = 0;

            {
                const ALbuffer *ALBuffer = BufferListItem->buffer;
                const ALubyte *Data = ALBuffer->data;
                ALsizei DataSize;

                /* Offset buffer data to current channel */
                Data += chan*SampleSize;

                /* If current pos is beyond the loop range, do not loop */
                {
                    BufferLoopItem = NULL;

                    /* Load what's left to play from the source buffer, and
                     * clear the rest of the temp buffer */
                    DataSize = minu(SrcBufferSize - SrcDataSize,
                                    ALBuffer->SampleLen - DataPosInt);

                    LoadSamples(&SrcData[SrcDataSize], &Data[DataPosInt * NumChannels*SampleSize],
                                NumChannels, ALBuffer->FmtType, DataSize);
                    SrcDataSize += DataSize;

                    SilenceSamples(&SrcData[SrcDataSize], SrcBufferSize - SrcDataSize);
                    SrcDataSize += SrcBufferSize - SrcDataSize;
                }
            }

            /* Now resample, then filter and mix to the appropriate outputs. */
            ResampledData = Resample(&voice->ResampleState,
                SrcData, DataPosFrac, increment,
                Device->ResampledData, DstBufferSize
            );
            {
                DirectParams *parms = &voice->Direct.Params[chan];
                const ALfloat *samples;

                samples = DoFilters(
                    &parms->LowPass, &parms->HighPass, Device->FilteredData,
                    ResampledData, DstBufferSize, voice->Direct.FilterType
                );

                {
                    if(!Counter)
                        memcpy(parms->Gains.Current, parms->Gains.Target,
                               sizeof(parms->Gains.Current));
                    MixSamples(samples, voice->Direct.Channels, voice->Direct.Buffer,
                        parms->Gains.Current, parms->Gains.Target, Counter, OutPos,
                        DstBufferSize
                    );
                }
            }

            for(send = 0;send < Device->NumAuxSends;send++)
            {
                SendParams *parms = &voice->Send[send].Params[chan];
                const ALfloat *samples;

                if(!voice->Send[send].Buffer)
                    continue;

                samples = DoFilters(
                    &parms->LowPass, &parms->HighPass, Device->FilteredData,
                    ResampledData, DstBufferSize, voice->Send[send].FilterType
                );

                if(!Counter)
                    memcpy(parms->Gains.Current, parms->Gains.Target,
                           sizeof(parms->Gains.Current));
                MixSamples(samples, voice->Send[send].Channels, voice->Send[send].Buffer,
                    parms->Gains.Current, parms->Gains.Target, Counter, OutPos, DstBufferSize
                );
            }
        }
        /* Update positions */
        DataPosFrac += increment*DstBufferSize;
        DataPosInt  += DataPosFrac>>FRACTIONBITS;
        DataPosFrac &= FRACTIONMASK;

        OutPos += DstBufferSize;
        voice->Offset += DstBufferSize;
        Counter = maxi(DstBufferSize, Counter) - DstBufferSize;
        firstpass = false;
    }

    voice->Flags |= VOICE_IS_FADING;

    /* Update source info */
    voice->position = DataPosInt;
    voice->position_fraction = DataPosFrac;
    voice->current_buffer = BufferListItem;
    return isplaying;
}
