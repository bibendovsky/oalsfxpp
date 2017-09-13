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
#include "mixer_defs.h"


static const float* do_filters(
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

void mix_source(
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

        auto samples = do_filters(
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

        samples = do_filters(
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
