#include "oalsfxpp_api_impl.h"
#include <new>


ApiImpl::ApiImpl()
    :
    device_{},
    source_{},
    effect_{},
    effect_slot_{}
{
}

bool ApiImpl::initialize(
    const ChannelFormat channel_format,
    const int sampling_rate)
{
    uninitialize();

    device_ = new (std::nothrow) ALCdevice{};
    source_ = new (std::nothrow) ALsource{};
    effect_ = new (std::nothrow) Effect{};
    effect_slot_ = new (std::nothrow) EffectSlot{};

    if (!device_ || !source_ || !effect_ || !effect_slot_)
    {
        uninitialize();
        return false;
    }

    device_->initialize(channel_format, sampling_rate);
    effect_->initialize();

    auto effect_state = effect_slot_->effect_state_.get();
    effect_state->dst_buffers_ = &device_->sample_buffers_;
    effect_state->dst_channel_count_ = device_->channel_count_;
    effect_state->update_device(device_);
    effect_slot_->is_props_updated_ = true;

    auto source = source_;

    for (int i = 0; i < device_->channel_count_; ++i)
    {
        source->direct_.channels_[i].reset();
        source->aux_.channels_[i].reset();
    }

    return true;
}

void ApiImpl::uninitialize()
{
    delete effect_;
    effect_ = nullptr;

    delete effect_slot_;
    effect_slot_ = nullptr;

    delete source_;
    source_ = nullptr;

    delete device_;
    device_ = nullptr;
}

void ApiImpl::mix_c(
    const float* data,
    const int channel_count,
    SampleBuffers& dst_buffers,
    float* current_gains,
    const float* target_gains,
    const int counter,
    const int dst_position,
    const int buffer_size)
{
    const auto delta = ((counter > 0) ? 1.0F / static_cast<float>(counter) : 0.0F);

    for (int c = 0; c < channel_count; ++c)
    {
        auto pos = 0;
        auto gain = current_gains[c];
        const auto step = (target_gains[c] - gain) * delta;

        if (std::abs(step) > FLT_EPSILON)
        {
            const auto size = std::min(buffer_size, counter);

            for ( ; pos < size; ++pos)
            {
                dst_buffers[c][dst_position + pos] += data[pos] * gain;
                gain += step;
            }

            if (pos == counter)
            {
                gain = target_gains[c];
            }

            current_gains[c] = gain;
        }

        if (!(std::abs(gain) > silence_threshold_gain))
        {
            continue;
        }

        for ( ; pos < buffer_size; ++pos)
        {
            dst_buffers[c][dst_position + pos] += data[pos] * gain;
        }
    }
}

void ApiImpl::alu_mix_data(
        void* dst_buffer,
        const int sample_count,
        const float* src_samples)
{
    device_->source_samples_ = src_samples;

    for (int samples_done = 0; samples_done < sample_count; )
    {
        const auto samples_to_do = std::min(sample_count - samples_done, max_sample_buffer_size);

        for (int c = 0; c < device_->channel_count_; ++c)
        {
            std::fill_n(device_->sample_buffers_[c].begin(), samples_to_do, 0.0F);
        }

        update_context_sources();

        auto slot = effect_slot_;

        for (int c = 0; c < max_effect_channels; ++c)
        {
            std::fill_n(slot->wet_buffer_[c].begin(), samples_to_do, 0.0F);
        }

        // source processing
        mix_source(samples_to_do);

        // effect slot processing
        auto state = slot->effect_state_.get();

        state->process(samples_to_do, slot->wet_buffer_, *state->dst_buffers_, state->dst_channel_count_);

        if (dst_buffer)
        {
            auto buffers = &device_->sample_buffers_;
            const auto channels = device_->channel_count_;

            write_f32(buffers, dst_buffer, samples_done, samples_to_do, channels);
        }

        samples_done += samples_to_do;
    }
}

void ApiImpl::mix_source(
    const int sample_count)
{
    const auto channel_count = device_->channel_count_;

    for (int chan = 0; chan < channel_count; ++chan)
    {
        for (int i = 0; i < sample_count; ++i)
        {
            device_->resampled_data_[i] = device_->source_samples_[(i * channel_count) + chan];
        }


        auto parms = &source_->direct_.channels_[chan];

        auto samples = apply_filters(
            &parms->low_pass_,
            &parms->high_pass_,
            device_->filtered_data_.data(),
            device_->resampled_data_.data(),
            sample_count,
            source_->direct_.filter_type_);

        parms->current_gains_ = parms->target_gains_;

        mix_c(
            samples,
            source_->direct_.channel_count_,
            *source_->direct_.buffers_,
            parms->current_gains_.data(),
            parms->target_gains_.data(),
            0,
            0,
            sample_count);

        if (!source_->aux_.buffers_)
        {
            continue;
        }

        parms = &source_->aux_.channels_[chan];

        samples = apply_filters(
            &parms->low_pass_,
            &parms->high_pass_,
            device_->filtered_data_.data(),
            device_->resampled_data_.data(),
            sample_count,
            source_->aux_.filter_type_);

        parms->current_gains_ = parms->target_gains_;

        mix_c(
            samples,
            source_->aux_.channel_count_,
            *source_->aux_.buffers_,
            parms->current_gains_.data(),
            parms->target_gains_.data(),
            0,
            0,
            sample_count);
    }
}

// Basically the inverse of the "mix". Rather than one input going to multiple
// outputs (each with its own gain), it's multiple inputs (each with its own
// gain) going to one output. This applies one row (vs one column) of a matrix
// transform. And as the matrices are more or less static once set up, no
// stepping is necessary.
void ApiImpl::mix_row_c(
    float* dst_buffer,
    const float* gains,
    const SampleBuffers& src_buffers,
    const int channel_count,
    const int src_position,
    const int buffer_size)
{
    for (int c = 0; c < channel_count; ++c)
    {
        const auto gain = gains[c];

        if (!(std::abs(gain) > silence_threshold_gain))
        {
            continue;
        }

        for (int i = 0; i < buffer_size; ++i)
        {
            dst_buffer[i] += src_buffers[c][src_position + i] * gain;
        }
    }
}

const float* ApiImpl::apply_filters(
    FilterState* lp_filter,
    FilterState* hp_filter,
    float* dst_samples,
    const float* src_samples,
    const int sample_count,
    const ActiveFilters filter_type)
{
    switch (filter_type)
    {
    case ActiveFilters::none:
        lp_filter->process_pass_through(sample_count, src_samples);
        hp_filter->process_pass_through(sample_count, src_samples);
        break;

    case ActiveFilters::low_pass:
        lp_filter->process(sample_count, src_samples, dst_samples);
        hp_filter->process_pass_through(sample_count, dst_samples);
        return dst_samples;

    case ActiveFilters::high_pass:
        lp_filter->process_pass_through(sample_count, src_samples);
        hp_filter->process(sample_count, src_samples, dst_samples);
        return dst_samples;

    case ActiveFilters::band_pass:
        for (int i = 0; i < sample_count; )
        {
            float temp[256];

            const auto todo = std::min(256, sample_count - i);

            lp_filter->process(todo, src_samples + i, temp);
            hp_filter->process(todo, temp, dst_samples + i);

            i += todo;
        }

        return dst_samples;
    }

    return src_samples;
}

bool ApiImpl::calc_effect_slot_params(
    EffectSlot* slot)
{
    if (!slot->is_props_updated_)
    {
        return false;
    }

    slot->is_props_updated_ = false;
    slot->effect_state_->update(device_, slot, &slot->effect_.props_);

    return true;
}

void ApiImpl::calc_panning_and_filters(
    const float distance,
    const float* dir,
    const float spread,
    const float dry_gain,
    const float dry_gain_hf,
    const float dry_gain_lf,
    const float wet_gain,
    const float wet_gain_lf,
    const float wet_gain_hf,
    EffectSlot* send_slot)
{
    const auto frequency = device_->frequency_;
    const ChannelMap* channel_map = nullptr;
    auto channel_count = 0;
    auto downmix_gain = 1.0F;

    switch (device_->channel_format_)
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
        AmbiCoeffs coeffs;

        // Special-case LFE
        if (channel_map[c].channel_id == ChannelId::lfe)
        {
            source_->direct_.channels_[c].target_gains_.fill(0.0F);

            const auto idx = get_channel_index(device_->channel_names_, channel_map[c].channel_id);

            if (idx != -1)
            {
                source_->direct_.channels_[c].target_gains_[idx] = dry_gain;
            }

            source_->aux_.channels_[c].target_gains_.fill(0.0F);

            continue;
        }

        Panning::calc_angle_coeffs(channel_map[c].angle, channel_map[c].elevation, spread, coeffs);

        Panning::compute_panning_gains(
            device_->channel_count_,
            device_->dry_,
            coeffs,
            dry_gain,
            source_->direct_.channels_[c].target_gains_);

        const auto slot = send_slot;

        if (slot)
        {
            Panning::compute_panning_gains_bf(
                max_effect_channels,
                coeffs,
                wet_gain,
                source_->aux_.channels_[c].target_gains_);
        }
        else
        {
            source_->aux_.channels_[c].target_gains_.fill(0.0F);
        }
    }

    auto hf_scale = source_->direct_.hf_reference_ / frequency;
    auto lf_scale = source_->direct_.lf_reference_ / frequency;
    auto gain_hf = std::max(dry_gain_hf, 0.001F); // Limit -60dB
    auto gain_lf = std::max(dry_gain_lf, 0.001F);

    source_->direct_.filter_type_ = ActiveFilters::none;

    if (gain_hf != 1.0F)
    {
        source_->direct_.filter_type_ = static_cast<ActiveFilters>(
            static_cast<int>(source_->direct_.filter_type_) | static_cast<int>(ActiveFilters::low_pass));
    }

    if (gain_lf != 1.0F)
    {
        source_->direct_.filter_type_ = static_cast<ActiveFilters>(
            static_cast<int>(source_->direct_.filter_type_) | static_cast<int>(ActiveFilters::high_pass));
    }

    source_->direct_.channels_[0].low_pass_.set_params(
        FilterType::high_shelf,
        gain_hf,
        hf_scale,
        FilterState::calc_rcp_q_from_slope(gain_hf, 1.0F));

    source_->direct_.channels_[0].high_pass_.set_params(
        FilterType::low_shelf,
        gain_lf,
        lf_scale,
        FilterState::calc_rcp_q_from_slope(gain_lf, 1.0F));

    for (int c = 1; c < channel_count; ++c)
    {
        FilterState::copy_params(source_->direct_.channels_[0].low_pass_, source_->direct_.channels_[c].low_pass_);
        FilterState::copy_params(source_->direct_.channels_[0].high_pass_, source_->direct_.channels_[c].high_pass_);
    }

    hf_scale = source_->aux_.hf_reference_ / frequency;
    lf_scale = source_->aux_.lf_reference_ / frequency;
    gain_hf = std::max(wet_gain_hf, 0.001F);
    gain_lf = std::max(wet_gain_lf, 0.001F);

    source_->aux_.filter_type_ = ActiveFilters::none;

    if (gain_hf != 1.0F)
    {
        source_->aux_.filter_type_ = static_cast<ActiveFilters>(
            static_cast<int>(source_->aux_.filter_type_) | static_cast<int>(ActiveFilters::low_pass));
    }

    if (gain_lf != 1.0F)
    {
        source_->aux_.filter_type_ = static_cast<ActiveFilters>(
            static_cast<int>(source_->aux_.filter_type_) | static_cast<int>(ActiveFilters::high_pass));
    }

    source_->aux_.channels_[0].low_pass_.set_params(
        FilterType::high_shelf,
        gain_hf,
        hf_scale,
        FilterState::calc_rcp_q_from_slope(gain_hf, 1.0F));

    source_->aux_.channels_[0].high_pass_.set_params(
        FilterType::low_shelf,
        gain_lf,
        lf_scale,
        FilterState::calc_rcp_q_from_slope(gain_lf, 1.0F));

    for (int c = 1; c < channel_count; ++c)
    {
        FilterState::copy_params(source_->aux_.channels_[0].low_pass_, source_->aux_.channels_[c].low_pass_);
        FilterState::copy_params(source_->aux_.channels_[0].high_pass_, source_->aux_.channels_[c].high_pass_);
    }
}

void ApiImpl::calc_non_attn_source_params()
{
    source_->direct_.buffers_ = &device_->sample_buffers_;
    source_->direct_.channel_count_ = device_->channel_count_;

    auto send_slot = effect_slot_;

    if (!send_slot || send_slot->effect_.type_ == EffectType::null)
    {
        source_->aux_.buffers_ = nullptr;
        source_->aux_.channel_count_ = 0;
    }
    else
    {
        source_->aux_.buffers_ = &send_slot->wet_buffer_;
        source_->aux_.channel_count_ = max_effect_channels;
    }

    // Calculate gains
    auto dry_gain = 1.0F;
    dry_gain *= source_->direct_.gain_;
    dry_gain = std::min(dry_gain, max_mix_gain);

    const auto dry_gain_hf = source_->direct_.gain_hf_;
    const auto dry_gain_lf = source_->direct_.gain_lf_;

    constexpr float dir[3] = {0.0F, 0.0F, -1.0F};

    const auto wet_gain = std::min(source_->aux_.gain_, max_mix_gain);
    const auto wet_gain_hf = source_->aux_.gain_hf_;
    const auto wet_gain_lf = source_->aux_.gain_lf_;

    calc_panning_and_filters(
        0.0F,
        dir,
        0.0F,
        dry_gain,
        dry_gain_hf,
        dry_gain_lf,
        wet_gain,
        wet_gain_lf,
        wet_gain_hf,
        send_slot);
}

void ApiImpl::update_context_sources()
{
    auto slot = effect_slot_;
    auto source = source_;

    const auto is_props_updated = calc_effect_slot_params(slot);

    if (source && is_props_updated)
    {
        calc_non_attn_source_params();
    }
}

void ApiImpl::write_f32(
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
