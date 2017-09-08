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


#include <algorithm>
#include "config.h"
#include "alSource.h"
#include "alu.h"


struct ChanMap {
    enum Channel channel;
    float angle;
    float elevation;
};

extern inline float lerp(float val1, float val2, float mu);

extern inline void aluMatrixfSetRow(aluMatrixf *matrix, int row,
                                    float m0, float m1, float m2, float m3);
extern inline void aluMatrixfSet(aluMatrixf *matrix,
                                 float m00, float m01, float m02, float m03,
                                 float m10, float m11, float m12, float m13,
                                 float m20, float m21, float m22, float m23,
                                 float m30, float m31, float m32, float m33);

const aluMatrixf IdentityMatrixf = {{
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
}};


void DeinitVoice(ALvoice *voice)
{
}

static bool CalcEffectSlotParams(ALeffectslot *slot, ALCdevice *device)
{
    struct ALeffectslotProps *props;
    IEffect *state;

    props = slot->update;
    slot->update = NULL;
    if(!props) return false;

    slot->params.effect_type = props->type;

    /* Swap effect states. No need to play with the ref counts since they keep
     * the same number of refs.
     */
    state = props->state;
    props->state = slot->params.effect_state;
    slot->params.effect_state = state;

    state->update(device, slot, &props->props);

    return true;
}


static const struct ChanMap MonoMap[1] = {
    { FrontCenter, 0.0f, 0.0f }
}, RearMap[2] = {
    { BackLeft,  deg_to_rad(-150.0f), deg_to_rad(0.0f) },
    { BackRight, deg_to_rad( 150.0f), deg_to_rad(0.0f) }
}, QuadMap[4] = {
    { FrontLeft,  deg_to_rad( -45.0f), deg_to_rad(0.0f) },
    { FrontRight, deg_to_rad(  45.0f), deg_to_rad(0.0f) },
    { BackLeft,   deg_to_rad(-135.0f), deg_to_rad(0.0f) },
    { BackRight,  deg_to_rad( 135.0f), deg_to_rad(0.0f) }
}, X51Map[6] = {
    { FrontLeft,   deg_to_rad( -30.0f), deg_to_rad(0.0f) },
    { FrontRight,  deg_to_rad(  30.0f), deg_to_rad(0.0f) },
    { FrontCenter, deg_to_rad(   0.0f), deg_to_rad(0.0f) },
    { LFE, 0.0f, 0.0f },
    { SideLeft,    deg_to_rad(-110.0f), deg_to_rad(0.0f) },
    { SideRight,   deg_to_rad( 110.0f), deg_to_rad(0.0f) }
}, X61Map[7] = {
    { FrontLeft,    deg_to_rad(-30.0f), deg_to_rad(0.0f) },
    { FrontRight,   deg_to_rad( 30.0f), deg_to_rad(0.0f) },
    { FrontCenter,  deg_to_rad(  0.0f), deg_to_rad(0.0f) },
    { LFE, 0.0f, 0.0f },
    { BackCenter,   deg_to_rad(180.0f), deg_to_rad(0.0f) },
    { SideLeft,     deg_to_rad(-90.0f), deg_to_rad(0.0f) },
    { SideRight,    deg_to_rad( 90.0f), deg_to_rad(0.0f) }
}, X71Map[8] = {
    { FrontLeft,   deg_to_rad( -30.0f), deg_to_rad(0.0f) },
    { FrontRight,  deg_to_rad(  30.0f), deg_to_rad(0.0f) },
    { FrontCenter, deg_to_rad(   0.0f), deg_to_rad(0.0f) },
    { LFE, 0.0f, 0.0f },
    { BackLeft,    deg_to_rad(-150.0f), deg_to_rad(0.0f) },
    { BackRight,   deg_to_rad( 150.0f), deg_to_rad(0.0f) },
    { SideLeft,    deg_to_rad( -90.0f), deg_to_rad(0.0f) },
    { SideRight,   deg_to_rad(  90.0f), deg_to_rad(0.0f) }
};

static void CalcPanningAndFilters(ALvoice *voice, const float Distance, const float *Dir,
                                  const float Spread, const float DryGain,
                                  const float DryGainHF, const float DryGainLF,
                                  const float *WetGain, const float *WetGainLF,
                                  const float *WetGainHF, ALeffectslot* send_slot,
                                  const struct ALvoiceProps *props,
                                  const ALCdevice *Device)
{
    struct ChanMap StereoMap[2] = {
        { FrontLeft,  deg_to_rad(-30.0f), deg_to_rad(0.0f) },
        { FrontRight, deg_to_rad( 30.0f), deg_to_rad(0.0f) }
    };
    bool DirectChannels = AL_FALSE;
    const int NumSends = Device->num_aux_sends;
    const int Frequency = Device->frequency;
    const struct ChanMap *chans = NULL;
    int num_channels = 0;
    bool isbformat = false;
    float downmix_gain = 1.0f;
    int c, j;

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
        float w0 = 0.0f;

        for(c = 0;c < num_channels;c++)
        {
            float coeffs[max_ambi_coeffs];

            /* Special-case LFE */
            if(chans[c].channel == LFE)
            {
                for(j = 0;j < max_output_channels;j++)
                    voice->direct.params[c].gains.target[j] = 0.0f;
                if(&Device->dry.buffers == Device->real_out.buffers)
                {
                    int idx = GetChannelIdxByName(Device->real_out, chans[c].channel);
                    if(idx != -1) voice->direct.params[c].gains.target[idx] = DryGain;
                }

                if (NumSends > 0)
                {
                    for(j = 0;j < max_effect_channels;j++)
                        voice->send.params[c].gains.target[j] = 0.0f;
                }
                continue;
            }

            CalcAngleCoeffs(chans[c].angle, chans[c].elevation, Spread, coeffs);
            ComputePanningGains(Device->dry,
                coeffs, DryGain, voice->direct.params[c].gains.target
            );

            if (NumSends > 0)
            {
                const ALeffectslot *Slot = send_slot;
                if(Slot)
                {
                    ComputePanningGainsBF(Slot->chan_map, Slot->num_channels,
                        coeffs, WetGain[0], voice->send.params[c].gains.target
                    );
                }
                else
                {
                    for(j = 0;j < max_effect_channels;j++)
                    {
                        voice->send.params[c].gains.target[j] = 0.0f;
                    }
                }
            }
        }
    }

    {
        float hfScale = props->direct.hf_reference / Frequency;
        float lfScale = props->direct.lf_reference / Frequency;
        float gainHF = std::max(DryGainHF, 0.001f); /* Limit -60dB */
        float gainLF = std::max(DryGainLF, 0.001f);

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
    if(NumSends > 0)
    {
        float hfScale = props->send.hf_reference / Frequency;
        float lfScale = props->send.lf_reference / Frequency;
        float gainHF = std::max(WetGainHF[0], 0.001f);
        float gainLF = std::max(WetGainLF[0], 0.001f);

        voice->send.filter_type = AF_None;
        if(gainHF != 1.0f) voice->send.filter_type = static_cast<ActiveFilters>(voice->send.filter_type | AF_LowPass);
        if(gainLF != 1.0f) voice->send.filter_type = static_cast<ActiveFilters>(voice->send.filter_type | AF_HighPass);
        ALfilterState_setParams(
            &voice->send.params[0].low_pass, ALfilterType_HighShelf,
            gainHF, hfScale, calc_rcpQ_from_slope(gainHF, 1.0f)
        );
        ALfilterState_setParams(
            &voice->send.params[0].high_pass, ALfilterType_LowShelf,
            gainLF, lfScale, calc_rcpQ_from_slope(gainLF, 1.0f)
        );
        for(c = 1;c < num_channels;c++)
        {
            ALfilterState_copyParams(&voice->send.params[c].low_pass,
                                     &voice->send.params[0].low_pass);
            ALfilterState_copyParams(&voice->send.params[c].high_pass,
                                     &voice->send.params[0].high_pass);
        }
    }
}

static void CalcNonAttnSourceParams(ALvoice *voice, const struct ALvoiceProps *props, ALCdevice* device)
{
    static const float dir[3] = { 0.0f, 0.0f, -1.0f };
    float DryGain, DryGainHF, DryGainLF;
    float WetGain[max_sends];
    float WetGainHF[max_sends];
    float WetGainLF[max_sends];
    ALeffectslot* send_slot = nullptr;
    int i;

    voice->direct.buffer = &device->dry.buffers;
    voice->direct.channels = device->dry.num_channels;
    if(device->num_aux_sends > 0)
    {
        send_slot = props->send.slot;
        if(!send_slot || send_slot->params.effect_type == AL_EFFECT_NULL)
        {
            voice->send.buffer = nullptr;
            voice->send.channels = 0;
        }
        else
        {
            voice->send.buffer = &send_slot->wet_buffer;
            voice->send.channels = send_slot->num_channels;
        }
    }

    /* Calculate gains */
    DryGain  = 1.0F;
    DryGain *= props->direct.gain;
    DryGain  = std::min(DryGain, max_mix_gain);
    DryGainHF = props->direct.gain_hf;
    DryGainLF = props->direct.gain_lf;
    for(i = 0;i < device->num_aux_sends;i++)
    {
        WetGain[i]  = 1.0F;
        WetGain[i] *= props->send.gain;
        WetGain[i]  = std::min(WetGain[i], max_mix_gain);
        WetGainHF[i] = props->send.gain_hf;
        WetGainLF[i] = props->send.gain_lf;
    }

    CalcPanningAndFilters(voice, 0.0f, dir, 0.0f, DryGain, DryGainHF, DryGainLF, WetGain,
                          WetGainLF, WetGainHF, send_slot, props, device);
}

static void CalcSourceParams(ALvoice *voice, ALCdevice* device, bool force)
{
    CalcNonAttnSourceParams(voice, &voice->props, device);
}


static void UpdateContextSources(ALCdevice* device)
{
    auto slot = device->effect_slot;
    auto force = CalcEffectSlotParams(slot, device);
    auto voice = device->voice;
    auto source = voice->source;

    if(source)
    {
        CalcSourceParams(voice, device, force);
    }
}

static void WriteF32(const SampleBuffers* InBuffer, void *OutBuffer,
                     int Offset, int SamplesToDo, int numchans)
{
    int i, j;
    for(j = 0;j < numchans;j++)
    {
        const float *in = (*InBuffer)[j].data();
        auto *out = (float*)OutBuffer + Offset*numchans + j;

        for(i = 0;i < SamplesToDo;i++)
            out[i*numchans] = in[i];
    }
}

void aluMixData(ALCdevice *device, void *OutBuffer, int NumSamples, const float* src_samples)
{
    int SamplesToDo;
    int SamplesDone;
    int i, c;

    device->source_samples = src_samples;

    for(SamplesDone = 0;SamplesDone < NumSamples;)
    {
        SamplesToDo = std::min(NumSamples-SamplesDone, max_sample_buffer_size);

        for(c = 0;c < device->dry.num_channels;c++)
        {
            std::fill_n(device->dry.buffers[c].begin(), SamplesToDo, 0.0F);
        }

        UpdateContextSources(device);

        auto slot = device->effect_slot;

        for(c = 0;c < slot->num_channels;c++)
        {
            std::fill_n(slot->wet_buffer[c].begin(), SamplesToDo, 0.0F);
        }

        /* source processing */
        for(i = 0;i < device->voice_count;i++)
        {
            ALvoice *voice = device->voice;
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
        state->process(SamplesToDo, slot->wet_buffer, *state->out_buffer, state->out_channels);

        if(OutBuffer)
        {
            auto Buffer = device->real_out.buffers;
            int Channels = device->real_out.num_channels;
            WriteF32(Buffer, OutBuffer, SamplesDone, SamplesToDo, Channels);
        }

        SamplesDone += SamplesToDo;
    }
}
