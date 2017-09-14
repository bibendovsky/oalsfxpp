#include "oalsfxpp_api_impl.h"
#include <new>
#include "alEffect.h"
#include "alAuxEffectSlot.h"
#include "alSource.h"


ALCdevice* g_device = nullptr;
ALsource* g_source = nullptr;
Effect* g_effect = nullptr;
EffectSlot* g_effect_slot = nullptr;


bool ApiImpl::initialize(
    const ChannelFormat channel_format,
    const int sampling_rate)
{
    g_device = new (std::nothrow) ALCdevice{};
    g_source = new (std::nothrow) ALsource{};
    g_effect = new (std::nothrow) Effect{};
    g_effect_slot = new (std::nothrow) EffectSlot{};

    if (!g_device || !g_source || !g_effect || !g_effect_slot)
    {
        uninitialize();
        return false;
    }

    g_device->initialize(channel_format, sampling_rate);
    g_effect->initialize();

    auto effect_state = g_effect_slot->effect_state_.get();
    effect_state->dst_buffers_ = &g_device->sample_buffers_;
    effect_state->dst_channel_count_ = g_device->channel_count_;
    effect_state->update_device(g_device);
    g_effect_slot->is_props_updated_ = true;

    auto source = g_source;

    for (int i = 0; i < g_device->channel_count_; ++i)
    {
        source->direct_.channels_[i].reset();
        source->aux_.channels_[i].reset();
    }

    return true;
}

void ApiImpl::uninitialize()
{
    delete g_effect;
    delete g_effect_slot;
    delete g_source;
    delete g_device;
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

void ApiImpl::mix_source(
    ALsource* source,
    ALCdevice* device,
    const int sample_count)
{
    const auto channel_count = device->channel_count_;

    for (int chan = 0; chan < channel_count; ++chan)
    {
        for (int i = 0; i < sample_count; ++i)
        {
            device->resampled_data_[i] = device->source_samples_[(i * channel_count) + chan];
        }


        auto parms = &source->direct_.channels_[chan];

        auto samples = apply_filters(
            &parms->low_pass_,
            &parms->high_pass_,
            device->filtered_data_.data(),
            device->resampled_data_.data(),
            sample_count,
            source->direct_.filter_type_);

        parms->current_gains_ = parms->target_gains_;

        mix_c(
            samples,
            source->direct_.channel_count_,
            *source->direct_.buffers_,
            parms->current_gains_.data(),
            parms->target_gains_.data(),
            0,
            0,
            sample_count);

        if (!source->aux_.buffers_)
        {
            continue;
        }

        parms = &source->aux_.channels_[chan];

        samples = apply_filters(
            &parms->low_pass_,
            &parms->high_pass_,
            device->filtered_data_.data(),
            device->resampled_data_.data(),
            sample_count,
            source->aux_.filter_type_);

        parms->current_gains_ = parms->target_gains_;

        mix_c(
            samples,
            source->aux_.channel_count_,
            *source->aux_.buffers_,
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
