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
#include "alSource.h"
#include "alu.h"


struct ChanMap {
    enum Channel channel;
    ALfloat angle;
    ALfloat elevation;
};

extern inline ALfloat minf(ALfloat a, ALfloat b);
extern inline ALfloat maxf(ALfloat a, ALfloat b);
extern inline ALfloat clampf(ALfloat val, ALfloat min, ALfloat max);

extern inline ALuint minu(ALuint a, ALuint b);
extern inline ALuint maxu(ALuint a, ALuint b);
extern inline ALuint clampu(ALuint val, ALuint min, ALuint max);

extern inline ALint mini(ALint a, ALint b);
extern inline ALint maxi(ALint a, ALint b);
extern inline ALint clampi(ALint val, ALint min, ALint max);

extern inline ALfloat lerp(ALfloat val1, ALfloat val2, ALfloat mu);

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

static ALboolean CalcEffectSlotParams(ALeffectslot *slot, ALCdevice *device)
{
    struct ALeffectslotProps *props;
    IEffect *state;

    props = slot->update;
    slot->update = NULL;
    if(!props) return AL_FALSE;

    slot->params.effect_type = props->type;

    /* Swap effect states. No need to play with the ref counts since they keep
     * the same number of refs.
     */
    state = props->state;
    props->state = slot->params.effect_state;
    slot->params.effect_state = state;

    state->update(device, slot, &props->props);

    props->next = slot->free_list;
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
    const ALsizei NumSends = Device->num_aux_sends;
    const ALuint Frequency = Device->frequency;
    const struct ChanMap *chans = NULL;
    ALsizei num_channels = 0;
    bool isbformat = false;
    ALfloat downmix_gain = 1.0f;
    ALsizei c, i, j;

    switch(Device->fmt_chans)
    {
    case FmtMono:
        chans = MonoMap;
        num_channels = 1;
        /* Mono buffers are never played direct. */
        DirectChannels = false;
        break;

    case FmtStereo:
        /* Convert counter-clockwise to clockwise. */
        StereoMap[0].angle = -props->stereo_pan[0];
        StereoMap[1].angle = -props->stereo_pan[1];

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
                    voice->direct.params[c].gains.target[j] = 0.0f;
                if(Device->dry.buffer == Device->real_out.buffer)
                {
                    int idx = GetChannelIdxByName(Device->real_out, chans[c].channel);
                    if(idx != -1) voice->direct.params[c].gains.target[idx] = DryGain;
                }

                for(i = 0;i < NumSends;i++)
                {
                    for(j = 0;j < MAX_EFFECT_CHANNELS;j++)
                        voice->send[i].params[c].gains.target[j] = 0.0f;
                }
                continue;
            }

            CalcAngleCoeffs(chans[c].angle, chans[c].elevation, Spread, coeffs);
            ComputePanningGains(Device->dry,
                coeffs, DryGain, voice->direct.params[c].gains.target
            );

            for(i = 0;i < NumSends;i++)
            {
                const ALeffectslot *Slot = SendSlots[i];
                if(Slot)
                    ComputePanningGainsBF(Slot->chan_map, Slot->num_channels,
                        coeffs, WetGain[i], voice->send[i].params[c].gains.target
                    );
                else
                    for(j = 0;j < MAX_EFFECT_CHANNELS;j++)
                        voice->send[i].params[c].gains.target[j] = 0.0f;
            }
        }
    }

    {
        ALfloat hfScale = props->direct.hf_reference / Frequency;
        ALfloat lfScale = props->direct.lf_reference / Frequency;
        ALfloat gainHF = maxf(DryGainHF, 0.001f); /* Limit -60dB */
        ALfloat gainLF = maxf(DryGainLF, 0.001f);

        voice->direct.filter_type = AF_None;
        if(gainHF != 1.0f) voice->direct.filter_type = static_cast<ActiveFilters>(voice->direct.filter_type | AF_LowPass);
        if(gainLF != 1.0f) voice->direct.filter_type = static_cast<ActiveFilters>(voice->direct.filter_type | AF_HighPass);
        ALfilterState_setParams(
            &voice->direct.params[0].low_pass, ALfilterType_HighShelf,
            gainHF, hfScale, calc_rcpQ_from_slope(gainHF, 1.0f)
        );
        ALfilterState_setParams(
            &voice->direct.params[0].high_pass, ALfilterType_LowShelf,
            gainLF, lfScale, calc_rcpQ_from_slope(gainLF, 1.0f)
        );
        for(c = 1;c < num_channels;c++)
        {
            ALfilterState_copyParams(&voice->direct.params[c].low_pass,
                                     &voice->direct.params[0].low_pass);
            ALfilterState_copyParams(&voice->direct.params[c].high_pass,
                                     &voice->direct.params[0].high_pass);
        }
    }
    for(i = 0;i < NumSends;i++)
    {
        ALfloat hfScale = props->send[i].hf_reference / Frequency;
        ALfloat lfScale = props->send[i].lf_reference / Frequency;
        ALfloat gainHF = maxf(WetGainHF[i], 0.001f);
        ALfloat gainLF = maxf(WetGainLF[i], 0.001f);

        voice->send[i].filter_type = AF_None;
        if(gainHF != 1.0f) voice->send[i].filter_type = static_cast<ActiveFilters>(voice->send[i].filter_type | AF_LowPass);
        if(gainLF != 1.0f) voice->send[i].filter_type = static_cast<ActiveFilters>(voice->send[i].filter_type | AF_HighPass);
        ALfilterState_setParams(
            &voice->send[i].params[0].low_pass, ALfilterType_HighShelf,
            gainHF, hfScale, calc_rcpQ_from_slope(gainHF, 1.0f)
        );
        ALfilterState_setParams(
            &voice->send[i].params[0].high_pass, ALfilterType_LowShelf,
            gainLF, lfScale, calc_rcpQ_from_slope(gainLF, 1.0f)
        );
        for(c = 1;c < num_channels;c++)
        {
            ALfilterState_copyParams(&voice->send[i].params[c].low_pass,
                                     &voice->send[i].params[0].low_pass);
            ALfilterState_copyParams(&voice->send[i].params[c].high_pass,
                                     &voice->send[i].params[0].high_pass);
        }
    }
}

static void CalcNonAttnSourceParams(ALvoice *voice, const struct ALvoiceProps *props, const ALCcontext *ALContext)
{
    static const ALfloat dir[3] = { 0.0f, 0.0f, -1.0f };
    const ALCdevice *Device = ALContext->device;
    ALfloat DryGain, DryGainHF, DryGainLF;
    ALfloat WetGain[MAX_SENDS];
    ALfloat WetGainHF[MAX_SENDS];
    ALfloat WetGainLF[MAX_SENDS];
    ALeffectslot *SendSlots[MAX_SENDS];
    ALsizei i;

    voice->direct.buffer = Device->dry.buffer;
    voice->direct.channels = Device->dry.num_channels;
    for(i = 0;i < Device->num_aux_sends;i++)
    {
        SendSlots[i] = props->send[i].slot;
        if(!SendSlots[i] || SendSlots[i]->params.effect_type == AL_EFFECT_NULL)
        {
            SendSlots[i] = NULL;
            voice->send[i].buffer = NULL;
            voice->send[i].channels = 0;
        }
        else
        {
            voice->send[i].buffer = SendSlots[i]->wet_buffer;
            voice->send[i].channels = SendSlots[i]->num_channels;
        }
    }

    /* Calculate gains */
    DryGain  = 1.0F;
    DryGain *= props->direct.gain;
    DryGain  = minf(DryGain, GAIN_MIX_MAX);
    DryGainHF = props->direct.gain_hf;
    DryGainLF = props->direct.gain_lf;
    for(i = 0;i < Device->num_aux_sends;i++)
    {
        WetGain[i]  = 1.0F;
        WetGain[i] *= props->send[i].gain;
        WetGain[i]  = minf(WetGain[i], GAIN_MIX_MAX);
        WetGainHF[i] = props->send[i].gain_hf;
        WetGainLF[i] = props->send[i].gain_lf;
    }

    CalcPanningAndFilters(voice, 0.0f, dir, 0.0f, DryGain, DryGainHF, DryGainLF, WetGain,
                          WetGainLF, WetGainHF, SendSlots, props, Device);
}

static void CalcSourceParams(ALvoice *voice, ALCcontext *context, ALboolean force)
{
    CalcNonAttnSourceParams(voice, voice->props, context);
}


static void UpdateContextSources(ALCcontext *ctx, const struct ALeffectslotArray *slots)
{
    auto slot = ctx->device->effect_slot;
    auto force = CalcEffectSlotParams(slot, ctx->device);
    auto voice = ctx->voice;
    auto source = voice->source;

    if(source)
    {
        CalcSourceParams(voice, ctx, force);
    }
}

static void WriteF32(const ALfloatBUFFERSIZE *InBuffer, ALvoid *OutBuffer,
                     ALsizei Offset, ALsizei SamplesToDo, ALsizei numchans)
{
    ALsizei i, j;
    for(j = 0;j < numchans;j++)
    {
        const ALfloat *in = ASSUME_ALIGNED(InBuffer[j], 16);
        auto *out = (ALfloat*)OutBuffer + Offset*numchans + j;

        for(i = 0;i < SamplesToDo;i++)
            out[i*numchans] = in[i];
    }
}

void aluMixData(ALCdevice *device, ALvoid *OutBuffer, ALsizei NumSamples, const ALfloat* src_samples)
{
    ALsizei SamplesToDo;
    ALsizei SamplesDone;
    ALCcontext *ctx;
    ALsizei i, c;

    device->source_samples = src_samples;

    for(SamplesDone = 0;SamplesDone < NumSamples;)
    {
        SamplesToDo = mini(NumSamples-SamplesDone, BUFFERSIZE);
        for(c = 0;c < device->dry.num_channels;c++)
            memset(device->dry.buffer[c], 0, SamplesToDo*sizeof(ALfloat));
        if(device->dry.buffer != device->foa_out.buffer)
            for(c = 0;c < device->foa_out.num_channels;c++)
                memset(device->foa_out.buffer[c], 0, SamplesToDo*sizeof(ALfloat));
        if(device->dry.buffer != device->real_out.buffer)
            for(c = 0;c < device->real_out.num_channels;c++)
                memset(device->real_out.buffer[c], 0, SamplesToDo*sizeof(ALfloat));

        ctx = device->context;
        if(ctx)
        {
            UpdateContextSources(ctx, nullptr);

            auto slot = device->effect_slot;

            for(c = 0;c < slot->num_channels;c++)
            {
                memset(slot->wet_buffer[c], 0, SamplesToDo*sizeof(ALfloat));
            }

            /* source processing */
            for(i = 0;i < ctx->voice_count;i++)
            {
                ALvoice *voice = ctx->voice;
                ALsource *source = voice->source;
                if(source && voice->playing)
                {
                    if(!MixSource(voice, source, device, SamplesToDo))
                    {
                        voice->source = NULL;
                        voice->playing = false;
                    }
                }
            }

            /* effect slot processing */
            IEffect *state = slot->params.effect_state;
            state->process(SamplesToDo, slot->wet_buffer, state->out_buffer, state->out_channels);
        }

        if(OutBuffer)
        {
            ALfloat (*Buffer)[BUFFERSIZE] = device->real_out.buffer;
            ALsizei Channels = device->real_out.num_channels;
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
        for(i = 0;i < ctx->voice_count;i++)
        {
            ALvoice *voice = ctx->voice;
            ALsource *source;

            source = voice->source;
            voice->source = NULL;
            voice->playing = false;

            if(source)
            {
                if (source->state == AL_PLAYING)
                {
                    source->state = AL_STOPPED;
                }
            }
        }
        ctx->voice_count = 0;
    }
}
