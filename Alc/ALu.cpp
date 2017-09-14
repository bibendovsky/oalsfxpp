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
#include "oalsfxpp_api_impl.h"
#include "alSource.h"


struct ChannelMap
{
    ChannelId channel_id;
    float angle;
    float elevation;
}; // ChannelMap

float lerp(
    const float val1,
    const float val2,
    const float mu)
{
    return val1 + ((val2 - val1) * mu);
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
    {ChannelId::front_center, 0.0F, 0.0F}
};

static const ChannelMap stereo_map[2] = {
    {ChannelId::front_left, deg_to_rad(-30.0F), deg_to_rad(0.0F)},
    {ChannelId::front_right, deg_to_rad(30.0F), deg_to_rad(0.0F)}
};

static const ChannelMap quad_map[4] = {
    {ChannelId::front_left, deg_to_rad(-45.0F), deg_to_rad(0.0F)},
    {ChannelId::front_right, deg_to_rad(45.0F), deg_to_rad(0.0F)},
    {ChannelId::back_left, deg_to_rad(-135.0F), deg_to_rad(0.0F)},
    {ChannelId::back_right, deg_to_rad(135.0F), deg_to_rad(0.0F)}
};

static const ChannelMap x5_1_map[6] = {
    {ChannelId::front_left, deg_to_rad(-30.0F), deg_to_rad(0.0F)},
    {ChannelId::front_right, deg_to_rad(30.0F), deg_to_rad(0.0F)},
    {ChannelId::front_center, deg_to_rad(0.0F), deg_to_rad(0.0F)},
    {ChannelId::lfe, 0.0F, 0.0F},
    {ChannelId::side_left, deg_to_rad(-110.0F), deg_to_rad(0.0F)},
    {ChannelId::side_right, deg_to_rad(110.0F), deg_to_rad(0.0F)}
};

static const ChannelMap x6_1_map[7] = {
    {ChannelId::front_left, deg_to_rad(-30.0F), deg_to_rad(0.0F)},
    {ChannelId::front_right, deg_to_rad(30.0F), deg_to_rad(0.0F)},
    {ChannelId::front_center, deg_to_rad(0.0F), deg_to_rad(0.0F)},
    {ChannelId::lfe, 0.0F, 0.0F},
    {ChannelId::back_center, deg_to_rad(180.0F), deg_to_rad(0.0F)},
    {ChannelId::side_left, deg_to_rad(-90.0F), deg_to_rad(0.0F)},
    {ChannelId::side_right, deg_to_rad(90.0F), deg_to_rad(0.0F)}
};

static const ChannelMap x7_1_map[8] = {
    {ChannelId::front_left, deg_to_rad(-30.0F), deg_to_rad(0.0F)},
    {ChannelId::front_right, deg_to_rad(30.0F), deg_to_rad(0.0F)},
    {ChannelId::front_center, deg_to_rad(0.0F), deg_to_rad(0.0F)},
    {ChannelId::lfe, 0.0F, 0.0F},
    {ChannelId::back_left, deg_to_rad(-150.0F), deg_to_rad(0.0F)},
    {ChannelId::back_right, deg_to_rad(150.0F), deg_to_rad(0.0F)},
    {ChannelId::side_left, deg_to_rad(-90.0F), deg_to_rad(0.0F)},
    {ChannelId::side_right, deg_to_rad(90.0F), deg_to_rad(0.0F)}
};

static void calc_panning_and_filters(
    ALsource* source,
    const float distance,
    const float* dir,
    const float spread,
    const float dry_gain,
    const float dry_gain_hf,
    const float dry_gain_lf,
    const float wet_gain,
    const float wet_gain_lf,
    const float wet_gain_hf,
    EffectSlot* send_slot,
    const ALCdevice* device)
{
    const auto frequency = device->frequency_;
    const ChannelMap* channel_map = nullptr;
    auto channel_count = 0;
    auto downmix_gain = 1.0F;

    switch (device->channel_format_)
    {
    case ChannelFormat::mono:
        channel_map = mono_map;
        channel_count = 1;
        break;

    case ChannelFormat::stereo:
        channel_map = stereo_map;
        channel_count = 2;
        downmix_gain = 1.0F / 2.0F;
        break;

    case ChannelFormat::quad:
        channel_map = quad_map;
        channel_count = 4;
        downmix_gain = 1.0F / 4.0F;
        break;

    case ChannelFormat::five_point_one:
        channel_map = x5_1_map;
        channel_count = 6;
        // NOTE: Excludes LFE.
        downmix_gain = 1.0F / 5.0F;
        break;

    case ChannelFormat::six_point_one:
        channel_map = x6_1_map;
        channel_count = 7;
        // NOTE: Excludes LFE.
        downmix_gain = 1.0F / 6.0F;
        break;

    case ChannelFormat::seven_point_one:
        channel_map = x7_1_map;
        channel_count = 8;
        // NOTE: Excludes LFE.
        downmix_gain = 1.0F / 7.0F;
        break;
    }

    // Non-HRTF rendering. Use normal panning to the output.
    for (int c = 0; c < channel_count; ++c)
    {
        float coeffs[max_ambi_coeffs];

        // Special-case LFE
        if (channel_map[c].channel_id == ChannelId::lfe)
        {
            source->direct_.channels_[c].target_gains_.fill(0.0F);

            const auto idx = get_channel_index(device->channel_names_, channel_map[c].channel_id);

            if (idx != -1)
            {
                source->direct_.channels_[c].target_gains_[idx] = dry_gain;
            }

            source->aux_.channels_[c].target_gains_.fill(0.0F);

            continue;
        }

        calc_angle_coeffs(channel_map[c].angle, channel_map[c].elevation, spread, coeffs);

        compute_panning_gains(device->channel_count_, device->dry_, coeffs, dry_gain, source->direct_.channels_[c].target_gains_.data());

        const auto slot = send_slot;

        if (slot)
        {
            compute_panning_gains_bf(
                max_effect_channels,
                coeffs,
                wet_gain,
                source->aux_.channels_[c].target_gains_.data());
        }
        else
        {
            source->aux_.channels_[c].target_gains_.fill(0.0F);
        }
    }

    auto hf_scale = source->direct_.hf_reference_ / frequency;
    auto lf_scale = source->direct_.lf_reference_ / frequency;
    auto gain_hf = std::max(dry_gain_hf, 0.001F); // Limit -60dB
    auto gain_lf = std::max(dry_gain_lf, 0.001F);

    source->direct_.filter_type_ = ActiveFilters::none;

    if (gain_hf != 1.0F)
    {
        source->direct_.filter_type_ = static_cast<ActiveFilters>(
            static_cast<int>(source->direct_.filter_type_) | static_cast<int>(ActiveFilters::low_pass));
    }

    if (gain_lf != 1.0F)
    {
        source->direct_.filter_type_ = static_cast<ActiveFilters>(
            static_cast<int>(source->direct_.filter_type_) | static_cast<int>(ActiveFilters::high_pass));
    }

    source->direct_.channels_[0].low_pass_.set_params(
        FilterType::high_shelf,
        gain_hf,
        hf_scale,
        FilterState::calc_rcp_q_from_slope(gain_hf, 1.0F));

    source->direct_.channels_[0].high_pass_.set_params(
        FilterType::low_shelf,
        gain_lf,
        lf_scale,
        FilterState::calc_rcp_q_from_slope(gain_lf, 1.0F));

    for (int c = 1; c < channel_count; ++c)
    {
        FilterState::copy_params(source->direct_.channels_[0].low_pass_, source->direct_.channels_[c].low_pass_);
        FilterState::copy_params(source->direct_.channels_[0].high_pass_, source->direct_.channels_[c].high_pass_);
    }

    hf_scale = source->aux_.hf_reference_ / frequency;
    lf_scale = source->aux_.lf_reference_ / frequency;
    gain_hf = std::max(wet_gain_hf, 0.001F);
    gain_lf = std::max(wet_gain_lf, 0.001F);

    source->aux_.filter_type_ = ActiveFilters::none;

    if (gain_hf != 1.0F)
    {
        source->aux_.filter_type_ = static_cast<ActiveFilters>(
            static_cast<int>(source->aux_.filter_type_) | static_cast<int>(ActiveFilters::low_pass));
    }

    if (gain_lf != 1.0F)
    {
        source->aux_.filter_type_ = static_cast<ActiveFilters>(
            static_cast<int>(source->aux_.filter_type_) | static_cast<int>(ActiveFilters::high_pass));
    }

    source->aux_.channels_[0].low_pass_.set_params(
        FilterType::high_shelf,
        gain_hf,
        hf_scale,
        FilterState::calc_rcp_q_from_slope(gain_hf, 1.0F));

    source->aux_.channels_[0].high_pass_.set_params(
        FilterType::low_shelf,
        gain_lf,
        lf_scale,
        FilterState::calc_rcp_q_from_slope(gain_lf, 1.0F));

    for (int c = 1; c < channel_count; ++c)
    {
        FilterState::copy_params(source->aux_.channels_[0].low_pass_, source->aux_.channels_[c].low_pass_);
        FilterState::copy_params(source->aux_.channels_[0].high_pass_, source->aux_.channels_[c].high_pass_);
    }
}

static void calc_non_attn_source_params(
    ALsource* source,
    ALCdevice* device)
{
    source->direct_.buffers_ = &device->sample_buffers_;
    source->direct_.channel_count_ = device->channel_count_;

    auto send_slot = g_effect_slot;

    if (!send_slot || send_slot->effect_.type_ == EffectType::null)
    {
        source->aux_.buffers_ = nullptr;
        source->aux_.channel_count_ = 0;
    }
    else
    {
        source->aux_.buffers_ = &send_slot->wet_buffer_;
        source->aux_.channel_count_ = max_effect_channels;
    }

    // Calculate gains
    auto dry_gain = 1.0F;
    dry_gain *= source->direct_.gain_;
    dry_gain = std::min(dry_gain, max_mix_gain);

    const auto dry_gain_hf = source->direct_.gain_hf_;
    const auto dry_gain_lf = source->direct_.gain_lf_;

    constexpr float dir[3] = {0.0F, 0.0F, -1.0F};

    const auto wet_gain = std::min(source->aux_.gain_, max_mix_gain);
    const auto wet_gain_hf = source->aux_.gain_hf_;
    const auto wet_gain_lf = source->aux_.gain_lf_;

    calc_panning_and_filters(
        source,
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
        device);
}

static void update_context_sources(
    ALCdevice* device)
{
    auto slot = g_effect_slot;
    auto source = g_source;

    const auto is_props_updated = calc_effect_slot_params(slot, device);

    if (source && is_props_updated)
    {
        calc_non_attn_source_params(source, device);
    }
}

static void write_f32(
    const SampleBuffers* src_buffers,
    void* dst_buffer,
    const int offset,
    const int sample_count,
    const int channel_count)
{
    for (int j = 0; j < channel_count; ++j)
    {
        const auto in = (*src_buffers)[j].data();
        auto out = static_cast<float*>(dst_buffer) + (offset * channel_count) + j;

        for (int i = 0; i < sample_count; ++i)
        {
            out[i * channel_count] = in[i];
        }
    }
}

void alu_mix_data(
    ALCdevice* device,
    void* dst_buffer,
    const int sample_count,
    const float* src_samples)
{
    device->source_samples_ = src_samples;

    for (int samples_done = 0; samples_done < sample_count; )
    {
        const auto samples_to_do = std::min(sample_count - samples_done, max_sample_buffer_size);

        for (int c = 0; c < device->channel_count_; ++c)
        {
            std::fill_n(device->sample_buffers_[c].begin(), samples_to_do, 0.0F);
        }

        update_context_sources(device);

        auto slot = g_effect_slot;

        for (int c = 0; c < max_effect_channels; ++c)
        {
            std::fill_n(slot->wet_buffer_[c].begin(), samples_to_do, 0.0F);
        }

        // source processing
        ApiImpl::mix_source(g_source, device, samples_to_do);

        // effect slot processing
        auto state = slot->effect_state_.get();

        state->process(samples_to_do, slot->wet_buffer_, *state->dst_buffers_, state->dst_channel_count_);

        if (dst_buffer)
        {
            auto buffers = &device->sample_buffers_;
            const auto channels = device->channel_count_;

            write_f32(buffers, dst_buffer, samples_done, samples_to_do, channels);
        }

        samples_done += samples_to_do;
    }
}
