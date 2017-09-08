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

extern inline void alu_matrix_f_set_row(aluMatrixf *matrix, int row,
                                    float m0, float m1, float m2, float m3);
extern inline void alu_matrix_f_set(aluMatrixf *matrix,
                                 float m00, float m01, float m02, float m03,
                                 float m10, float m11, float m12, float m13,
                                 float m20, float m21, float m22, float m23,
                                 float m30, float m31, float m32, float m33);

const aluMatrixf identity_matrix_f = {{
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
}};


void deinit_voice(ALvoice* voice)
{
}

static bool calc_effect_slot_params(ALeffectslot* slot, ALCdevice* device)
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


static const struct ChanMap mono_map[1] = {
    { FrontCenter, 0.0f, 0.0f }
}, rear_map[2] = {
    { BackLeft,  deg_to_rad(-150.0f), deg_to_rad(0.0f) },
    { BackRight, deg_to_rad( 150.0f), deg_to_rad(0.0f) }
}, quad_map[4] = {
    { FrontLeft,  deg_to_rad( -45.0f), deg_to_rad(0.0f) },
    { FrontRight, deg_to_rad(  45.0f), deg_to_rad(0.0f) },
    { BackLeft,   deg_to_rad(-135.0f), deg_to_rad(0.0f) },
    { BackRight,  deg_to_rad( 135.0f), deg_to_rad(0.0f) }
}, x5_1_map[6] = {
    { FrontLeft,   deg_to_rad( -30.0f), deg_to_rad(0.0f) },
    { FrontRight,  deg_to_rad(  30.0f), deg_to_rad(0.0f) },
    { FrontCenter, deg_to_rad(   0.0f), deg_to_rad(0.0f) },
    { LFE, 0.0f, 0.0f },
    { SideLeft,    deg_to_rad(-110.0f), deg_to_rad(0.0f) },
    { SideRight,   deg_to_rad( 110.0f), deg_to_rad(0.0f) }
}, x6_1_map[7] = {
    { FrontLeft,    deg_to_rad(-30.0f), deg_to_rad(0.0f) },
    { FrontRight,   deg_to_rad( 30.0f), deg_to_rad(0.0f) },
    { FrontCenter,  deg_to_rad(  0.0f), deg_to_rad(0.0f) },
    { LFE, 0.0f, 0.0f },
    { BackCenter,   deg_to_rad(180.0f), deg_to_rad(0.0f) },
    { SideLeft,     deg_to_rad(-90.0f), deg_to_rad(0.0f) },
    { SideRight,    deg_to_rad( 90.0f), deg_to_rad(0.0f) }
}, x7_1_map[8] = {
    { FrontLeft,   deg_to_rad( -30.0f), deg_to_rad(0.0f) },
    { FrontRight,  deg_to_rad(  30.0f), deg_to_rad(0.0f) },
    { FrontCenter, deg_to_rad(   0.0f), deg_to_rad(0.0f) },
    { LFE, 0.0f, 0.0f },
    { BackLeft,    deg_to_rad(-150.0f), deg_to_rad(0.0f) },
    { BackRight,   deg_to_rad( 150.0f), deg_to_rad(0.0f) },
    { SideLeft,    deg_to_rad( -90.0f), deg_to_rad(0.0f) },
    { SideRight,   deg_to_rad(  90.0f), deg_to_rad(0.0f) }
};

static void calc_panning_and_filters(
    ALvoice* voice,
    const float distance,
    const float* dir,
    const float spread,
    const float dry_gain,
    const float dry_gain_hf,
    const float dry_gain_lf,
    const float* wet_gain,
    const float* wet_gain_lf,
    const float* wet_gain_hf,
    ALeffectslot* send_slot,
    const struct ALvoiceProps* props,
    const ALCdevice* device)
{
    struct ChanMap stereo_map[2] = {
        { FrontLeft,  deg_to_rad(-30.0f), deg_to_rad(0.0f) },
        { FrontRight, deg_to_rad( 30.0f), deg_to_rad(0.0f) }
    };

    bool DirectChannels = AL_FALSE;
    const int NumSends = device->num_aux_sends;
    const int Frequency = device->frequency;
    const struct ChanMap *chans = NULL;
    int num_channels = 0;
    bool isbformat = false;
    float downmix_gain = 1.0f;
    int c, j;

    switch(device->fmt_chans)
    {
    case FmtMono:
        chans = mono_map;
        num_channels = 1;
        /* Mono buffers are never played direct. */
        DirectChannels = false;
        break;

    case FmtStereo:
        /* Convert counter-clockwise to clockwise. */
        stereo_map[0].angle = -props->stereo_pan[0];
        stereo_map[1].angle = -props->stereo_pan[1];

        chans = stereo_map;
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
                if(&device->dry.buffers == device->real_out.buffers)
                {
                    int idx = get_channel_index_by_name(device->real_out, chans[c].channel);
                    if(idx != -1) voice->direct.params[c].gains.target[idx] = dry_gain;
                }

                if (NumSends > 0)
                {
                    for(j = 0;j < max_effect_channels;j++)
                        voice->send.params[c].gains.target[j] = 0.0f;
                }
                continue;
            }

            calc_angle_coeffs(chans[c].angle, chans[c].elevation, spread, coeffs);
            compute_panning_gains(device->dry,
                coeffs, dry_gain, voice->direct.params[c].gains.target
            );

            if (NumSends > 0)
            {
                const ALeffectslot *Slot = send_slot;
                if(Slot)
                {
                    compute_panning_gains_bf(Slot->chan_map, Slot->num_channels,
                        coeffs, wet_gain[0], voice->send.params[c].gains.target
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
        float gainHF = std::max(dry_gain_hf, 0.001f); /* Limit -60dB */
        float gainLF = std::max(dry_gain_lf, 0.001f);

        voice->direct.filter_type = AF_None;
        if(gainHF != 1.0f) voice->direct.filter_type = static_cast<ActiveFilters>(voice->direct.filter_type | AF_LowPass);
        if(gainLF != 1.0f) voice->direct.filter_type = static_cast<ActiveFilters>(voice->direct.filter_type | AF_HighPass);
        al_filter_state_set_params(
            &voice->direct.params[0].low_pass, ALfilterType_HighShelf,
            gainHF, hfScale, calc_rcp_q_from_slope(gainHF, 1.0f)
        );
        al_filter_state_set_params(
            &voice->direct.params[0].high_pass, ALfilterType_LowShelf,
            gainLF, lfScale, calc_rcp_q_from_slope(gainLF, 1.0f)
        );
        for(c = 1;c < num_channels;c++)
        {
            al_filter_state_copy_params(&voice->direct.params[c].low_pass,
                                     &voice->direct.params[0].low_pass);
            al_filter_state_copy_params(&voice->direct.params[c].high_pass,
                                     &voice->direct.params[0].high_pass);
        }
    }
    if(NumSends > 0)
    {
        float hfScale = props->send.hf_reference / Frequency;
        float lfScale = props->send.lf_reference / Frequency;
        float gainHF = std::max(wet_gain_hf[0], 0.001f);
        float gainLF = std::max(wet_gain_lf[0], 0.001f);

        voice->send.filter_type = AF_None;
        if(gainHF != 1.0f) voice->send.filter_type = static_cast<ActiveFilters>(voice->send.filter_type | AF_LowPass);
        if(gainLF != 1.0f) voice->send.filter_type = static_cast<ActiveFilters>(voice->send.filter_type | AF_HighPass);
        al_filter_state_set_params(
            &voice->send.params[0].low_pass, ALfilterType_HighShelf,
            gainHF, hfScale, calc_rcp_q_from_slope(gainHF, 1.0f)
        );
        al_filter_state_set_params(
            &voice->send.params[0].high_pass, ALfilterType_LowShelf,
            gainLF, lfScale, calc_rcp_q_from_slope(gainLF, 1.0f)
        );
        for(c = 1;c < num_channels;c++)
        {
            al_filter_state_copy_params(&voice->send.params[c].low_pass,
                                     &voice->send.params[0].low_pass);
            al_filter_state_copy_params(&voice->send.params[c].high_pass,
                                     &voice->send.params[0].high_pass);
        }
    }
}

static void calc_non_attn_source_params(ALvoice* voice, const struct ALvoiceProps* props, ALCdevice* device)
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

    calc_panning_and_filters(voice, 0.0f, dir, 0.0f, DryGain, DryGainHF, DryGainLF, WetGain,
                          WetGainLF, WetGainHF, send_slot, props, device);
}

static void calc_source_params(ALvoice* voice, ALCdevice* device, bool force)
{
    calc_non_attn_source_params(voice, &voice->props, device);
}

static void update_context_sources(ALCdevice* device)
{
    auto slot = device->effect_slot;
    auto force = calc_effect_slot_params(slot, device);
    auto voice = device->voice;
    auto source = voice->source;

    if(source)
    {
        calc_source_params(voice, device, force);
    }
}

static void write_f32(const SampleBuffers* in_buffer, void *out_buffer,
                     int offset, int samples_to_do, int num_chans)
{
    int i, j;
    for(j = 0;j < num_chans;j++)
    {
        const float *in = (*in_buffer)[j].data();
        auto *out = (float*)out_buffer + offset*num_chans + j;

        for(i = 0;i < samples_to_do;i++)
            out[i*num_chans] = in[i];
    }
}

void alu_mix_data(ALCdevice* device, void* out_buffer, int num_samples, const float* src_samples)
{
    int SamplesToDo;
    int SamplesDone;
    int i, c;

    device->source_samples = src_samples;

    for(SamplesDone = 0;SamplesDone < num_samples;)
    {
        SamplesToDo = std::min(num_samples-SamplesDone, max_sample_buffer_size);

        for(c = 0;c < device->dry.num_channels;c++)
        {
            std::fill_n(device->dry.buffers[c].begin(), SamplesToDo, 0.0F);
        }

        update_context_sources(device);

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
                if(!mix_source(voice, source, device, SamplesToDo))
                {
                    voice->source = NULL;
                    voice->playing = false;
                }
            }
        }

        /* effect slot processing */
        IEffect *state = slot->params.effect_state;
        state->process(SamplesToDo, slot->wet_buffer, *state->out_buffer, state->out_channels);

        if(out_buffer)
        {
            auto Buffer = device->real_out.buffers;
            int Channels = device->real_out.num_channels;
            write_f32(Buffer, out_buffer, SamplesDone, SamplesToDo, Channels);
        }

        SamplesDone += SamplesToDo;
    }
}
