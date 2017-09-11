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


struct ChannelMap {
    enum Channel channel;
    float angle;
    float elevation;
};

float lerp(
    const float val1,
    const float val2,
    const float mu)
{
    return val1 + ((val2 - val1) * mu);
}


void alu_matrix_f_set_row(
    aluMatrixf* matrix,
    const int row,
    const float m0,
    const float m1,
    const float m2,
    const float m3)
{
    matrix->m[row][0] = m0;
    matrix->m[row][1] = m1;
    matrix->m[row][2] = m2;
    matrix->m[row][3] = m3;
}

void alu_matrix_f_set(
    aluMatrixf* matrix,
    const float m00,
    const float m01,
    const float m02,
    const float m03,
    const float m10,
    const float m11,
    const float m12,
    const float m13,
    const float m20,
    const float m21,
    const float m22,
    const float m23,
    const float m30,
    const float m31,
    const float m32,
    const float m33)
{
    alu_matrix_f_set_row(matrix, 0, m00, m01, m02, m03);
    alu_matrix_f_set_row(matrix, 1, m10, m11, m12, m13);
    alu_matrix_f_set_row(matrix, 2, m20, m21, m22, m23);
    alu_matrix_f_set_row(matrix, 3, m30, m31, m32, m33);
}

const aluMatrixf identity_matrix_f = {{
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
}};

void ParamsBase::Gains::reset()
{
    std::uninitialized_fill_n(current, max_output_channels, 0.0F);
    std::uninitialized_fill_n(target, max_output_channels, 0.0F);
}

void ParamsBase::reset()
{
    low_pass.reset();
    high_pass.reset();
    gains.reset();
}

void deinit_voice(
    ALvoice* voice)
{
}

static bool calc_effect_slot_params(
    EffectSlot* slot,
    ALCdevice* device)
{
    if (!slot->is_props_updated_)
    {
        return false;
    }

    slot->is_props_updated_ = false;
    slot->effect_state_->update(device, slot, &slot->effect_.props_);

    return true;
}


static const ChannelMap mono_map[1] = {
    {FrontCenter, 0.0F, 0.0F}
};

static const ChannelMap rear_map[2] = {
    {BackLeft, deg_to_rad(-150.0F), deg_to_rad(0.0F)},
    {BackRight, deg_to_rad(150.0F), deg_to_rad(0.0F)}
};

static const ChannelMap quad_map[4] = {
    {FrontLeft, deg_to_rad(-45.0F), deg_to_rad(0.0F)},
    {FrontRight, deg_to_rad(45.0F), deg_to_rad(0.0F)},
    {BackLeft, deg_to_rad(-135.0F), deg_to_rad(0.0F)},
    {BackRight, deg_to_rad(135.0F), deg_to_rad(0.0F)}
};

static const ChannelMap x5_1_map[6] = {
    {FrontLeft, deg_to_rad(-30.0F), deg_to_rad(0.0F)},
    {FrontRight, deg_to_rad(30.0F), deg_to_rad(0.0F)},
    {FrontCenter, deg_to_rad(0.0F), deg_to_rad(0.0F)},
    {LFE, 0.0F, 0.0F},
    {SideLeft, deg_to_rad(-110.0F), deg_to_rad(0.0F)},
    {SideRight, deg_to_rad(110.0F), deg_to_rad(0.0F)}
};

static const ChannelMap x6_1_map[7] = {
    {FrontLeft, deg_to_rad(-30.0F), deg_to_rad(0.0F)},
    {FrontRight, deg_to_rad(30.0F), deg_to_rad(0.0F)},
    {FrontCenter, deg_to_rad(0.0F), deg_to_rad(0.0F)},
    {LFE, 0.0F, 0.0F},
    {BackCenter, deg_to_rad(180.0F), deg_to_rad(0.0F)},
    {SideLeft, deg_to_rad(-90.0F), deg_to_rad(0.0F)},
    {SideRight, deg_to_rad(90.0F), deg_to_rad(0.0F)}
};

static const ChannelMap x7_1_map[8] = {
    {FrontLeft, deg_to_rad(-30.0F), deg_to_rad(0.0F)},
    {FrontRight, deg_to_rad(30.0F), deg_to_rad(0.0F)},
    {FrontCenter, deg_to_rad(0.0F), deg_to_rad(0.0F)},
    {LFE, 0.0F, 0.0F},
    {BackLeft, deg_to_rad(-150.0F), deg_to_rad(0.0F)},
    {BackRight, deg_to_rad(150.0F), deg_to_rad(0.0F)},
    {SideLeft, deg_to_rad(-90.0F), deg_to_rad(0.0F)},
    {SideRight, deg_to_rad(90.0F), deg_to_rad(0.0F)}
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
    EffectSlot* send_slot,
    const struct ALvoiceProps* props,
    const ALCdevice* device)
{
    ChannelMap stereo_map[2] = {
        {FrontLeft, deg_to_rad(-30.0F), deg_to_rad(0.0F)},
        {FrontRight, deg_to_rad(30.0F), deg_to_rad(0.0F)}
    };

    const auto num_sends = device->num_aux_sends;
    const auto frequency = device->frequency;
    const ChannelMap* chans = nullptr;
    auto num_channels = 0;
    auto downmix_gain = 1.0F;

    switch (device->fmt_chans)
    {
    case FmtMono:
        chans = mono_map;
        num_channels = 1;
        break;

    case FmtStereo:
        // Convert counter-clockwise to clockwise.
        stereo_map[0].angle = -props->stereo_pan[0];
        stereo_map[1].angle = -props->stereo_pan[1];

        chans = stereo_map;
        num_channels = 2;
        downmix_gain = 1.0F / 2.0F;
        break;
    }

    // Non-HRTF rendering. Use normal panning to the output.
    {
        for (int c = 0; c < num_channels; ++c)
        {
            float coeffs[max_ambi_coeffs];

            // Special-case LFE
            if (chans[c].channel == LFE)
            {
                for (int j = 0; j < max_output_channels; ++j)
                {
                    voice->direct.params[c].gains.target[j] = 0.0F;
                }

                const auto idx = get_channel_index(device->channel_names, chans[c].channel);

                if (idx != -1)
                {
                    voice->direct.params[c].gains.target[idx] = dry_gain;
                }

                if (num_sends > 0)
                {
                    for (int j = 0; j < max_effect_channels; ++j)
                    {
                        voice->send.params[c].gains.target[j] = 0.0F;
                    }
                }

                continue;
            }

            calc_angle_coeffs(chans[c].angle, chans[c].elevation, spread, coeffs);

            compute_panning_gains(device, coeffs, dry_gain, voice->direct.params[c].gains.target);

            if (num_sends > 0)
            {
                const auto slot = send_slot;

                if (slot)
                {
                    compute_panning_gains_bf(
                        slot->channel_map_,
                        slot->channel_count_,
                        coeffs,
                        wet_gain[0],
                        voice->send.params[c].gains.target);
                }
                else
                {
                    for (int j = 0; j < max_effect_channels; ++j)
                    {
                        voice->send.params[c].gains.target[j] = 0.0F;
                    }
                }
            }
        }
    }

    {
        const auto hf_scale = props->direct.hf_reference / frequency;
        const auto lf_scale = props->direct.lf_reference / frequency;
        const auto gain_hf = std::max(dry_gain_hf, 0.001F); // Limit -60dB
        const auto gain_lf = std::max(dry_gain_lf, 0.001F);

        voice->direct.filter_type = ActiveFilters::none;

        if (gain_hf != 1.0F)
        {
            voice->direct.filter_type = static_cast<ActiveFilters>(
                static_cast<int>(voice->direct.filter_type) | static_cast<int>(ActiveFilters::low_pass));
        }

        if (gain_lf != 1.0F)
        {
            voice->direct.filter_type = static_cast<ActiveFilters>(
                static_cast<int>(voice->direct.filter_type) | static_cast<int>(ActiveFilters::high_pass));
        }

        al_filter_state_set_params(
            &voice->direct.params[0].low_pass,
            FilterType::high_shelf,
            gain_hf,
            hf_scale,
            calc_rcp_q_from_slope(gain_hf, 1.0F));

        al_filter_state_set_params(
            &voice->direct.params[0].high_pass,
            FilterType::low_shelf,
            gain_lf,
            lf_scale,
            calc_rcp_q_from_slope(gain_lf, 1.0F));

        for (int c = 1; c < num_channels; ++c)
        {
            al_filter_state_copy_params(&voice->direct.params[c].low_pass, &voice->direct.params[0].low_pass);
            al_filter_state_copy_params(&voice->direct.params[c].high_pass, &voice->direct.params[0].high_pass);
        }
    }

    if (num_sends > 0)
    {
        const auto hf_scale = props->send.hf_reference / frequency;
        const auto lf_scale = props->send.lf_reference / frequency;
        const auto gain_hf = std::max(wet_gain_hf[0], 0.001F);
        const auto gain_lf = std::max(wet_gain_lf[0], 0.001F);

        voice->send.filter_type = ActiveFilters::none;

        if (gain_hf != 1.0F)
        {
            voice->send.filter_type = static_cast<ActiveFilters>(
                static_cast<int>(voice->send.filter_type) | static_cast<int>(ActiveFilters::low_pass));
        }

        if (gain_lf != 1.0F)
        {
            voice->send.filter_type = static_cast<ActiveFilters>(
                static_cast<int>(voice->send.filter_type) | static_cast<int>(ActiveFilters::high_pass));
        }

        al_filter_state_set_params(
            &voice->send.params[0].low_pass,
            FilterType::high_shelf,
            gain_hf,
            hf_scale,
            calc_rcp_q_from_slope(gain_hf, 1.0F));

        al_filter_state_set_params(
            &voice->send.params[0].high_pass,
            FilterType::low_shelf,
            gain_lf,
            lf_scale,
            calc_rcp_q_from_slope(gain_lf, 1.0F));

        for (int c = 1; c < num_channels; ++c)
        {
            al_filter_state_copy_params(&voice->send.params[c].low_pass, &voice->send.params[0].low_pass);
            al_filter_state_copy_params(&voice->send.params[c].high_pass, &voice->send.params[0].high_pass);
        }
    }
}

static void calc_non_attn_source_params(
    ALvoice* voice,
    const ALvoiceProps* props,
    ALCdevice* device)
{
    voice->direct.buffer = &device->sample_buffers;
    voice->direct.channels = device->channel_count;

    EffectSlot* send_slot = nullptr;

    if (device->num_aux_sends > 0)
    {
        send_slot = props->send.slot;

        if (!send_slot || send_slot->effect_.type_ == EffectType::null)
        {
            voice->send.buffer = nullptr;
            voice->send.channels = 0;
        }
        else
        {
            voice->send.buffer = &send_slot->wet_buffer_;
            voice->send.channels = send_slot->channel_count_;
        }
    }

    // Calculate gains
    auto dry_gain = 1.0F;
    dry_gain *= props->direct.gain;
    dry_gain = std::min(dry_gain, max_mix_gain);

    const auto dry_gain_hf = props->direct.gain_hf;
    const auto dry_gain_lf = props->direct.gain_lf;

    static const float dir[3] = {0.0F, 0.0F, -1.0F};

    float wet_gain[max_sends];
    float wet_gain_hf[max_sends];
    float wet_gain_lf[max_sends];

    for (int i = 0; i < device->num_aux_sends; ++i)
    {
        wet_gain[i] = 1.0F;
        wet_gain[i] *= props->send.gain;
        wet_gain[i] = std::min(wet_gain[i], max_mix_gain);
        wet_gain_hf[i] = props->send.gain_hf;
        wet_gain_lf[i] = props->send.gain_lf;
    }

    calc_panning_and_filters(
        voice,
        0.0F,
        dir,
        0.0F,
        dry_gain,
        dry_gain_hf,
        dry_gain_lf,
        wet_gain,
        wet_gain_lf,
        wet_gain_hf,
        send_slot,
        props,
        device);
}

static void update_context_sources(
    ALCdevice* device)
{
    auto slot = device->effect_slot;
    auto voice = device->voice;
    auto source = voice->source;

    const auto is_props_updated = calc_effect_slot_params(slot, device);

    if (source && is_props_updated)
    {
        calc_non_attn_source_params(voice, &voice->props, device);
    }
}

static void write_f32(
    const SampleBuffers* in_buffer,
    void* out_buffer,
    const int offset,
    const int samples_to_do,
    const int num_chans)
{
    for (int j = 0; j < num_chans; ++j)
    {
        const auto in = (*in_buffer)[j].data();
        auto out = static_cast<float*>(out_buffer) + (offset * num_chans) + j;

        for (int i = 0; i < samples_to_do; ++i)
        {
            out[i * num_chans] = in[i];
        }
    }
}

void alu_mix_data(
    ALCdevice* device,
    void* out_buffer,
    const int num_samples,
    const float* src_samples)
{
    device->source_samples = src_samples;

    for (int samples_done = 0; samples_done < num_samples; )
    {
        const auto samples_to_do = std::min(num_samples - samples_done, max_sample_buffer_size);

        for (int c = 0; c < device->channel_count; ++c)
        {
            std::fill_n(device->sample_buffers[c].begin(), samples_to_do, 0.0F);
        }

        update_context_sources(device);

        auto slot = device->effect_slot;

        for (int c = 0; c < slot->channel_count_; ++c)
        {
            std::fill_n(slot->wet_buffer_[c].begin(), samples_to_do, 0.0F);
        }

        // source processing
        for (int i = 0; i < device->voice_count; ++i)
        {
            auto voice = device->voice;
            auto source = voice->source;

            if (source && voice->playing)
            {
                if (!mix_source(voice, source, device, samples_to_do))
                {
                    voice->source = nullptr;
                    voice->playing = false;
                }
            }
        }

        // effect slot processing
        auto state = slot->effect_state_.get();

        state->process(samples_to_do, slot->wet_buffer_, *state->out_buffer, state->out_channels);

        if (out_buffer)
        {
            auto buffers = &device->sample_buffers;
            const auto channels = device->channel_count;

            write_f32(buffers, out_buffer, samples_done, samples_to_do, channels);
        }

        samples_done += samples_to_do;
    }
}
