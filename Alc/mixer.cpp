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


static MixerFunc mix_samples = mix_c;


static const float *do_filters(
    FilterState* lp_filter,
    FilterState* hp_filter,
    float* dst,
    const float* src,
    int num_samples,
    ActiveFilters type)
{
    int i;
    switch(type)
    {
        case ActiveFilters::none:
            lp_filter->process_pass_through(num_samples, src);
            hp_filter->process_pass_through(num_samples, src);
            break;

        case ActiveFilters::low_pass:
            lp_filter->process(num_samples, src, dst);
            hp_filter->process_pass_through(num_samples, dst);
            return dst;

        case ActiveFilters::high_pass:
            lp_filter->process_pass_through(num_samples, src);
            hp_filter->process(num_samples, src, dst);
            return dst;

        case ActiveFilters::band_pass:
            for(i = 0;i < num_samples;)
            {
                float temp[256];
                int todo = std::min(256, num_samples-i);

                lp_filter->process(todo, src+i, temp);
                hp_filter->process(todo, temp, dst+i);
                i += todo;
            }
            return dst;
    }
    return src;
}

bool mix_source(ALsource* source, ALCdevice* device, int samples_to_do)
{
    int chan;
    const auto channel_count = device->channel_count_;

    for (chan = 0; chan < channel_count; ++chan)
    {
        const float* samples;

        /* Load what's left to play from the source buffer, and
            * clear the rest of the temp buffer */
        std::uninitialized_copy_n(device->source_samples_, channel_count * samples_to_do, device->source_data_.begin());

        /* Now resample, then filter and mix to the appropriate outputs. */
        std::uninitialized_copy_n(device->source_data_.cbegin(), samples_to_do, device->resampled_data_.begin());


        auto parms = &source->direct_.channels_[chan];

        samples = do_filters(
            &parms->low_pass_,
            &parms->high_pass_,
            device->filtered_data_.data(),
            device->resampled_data_.data(),
            samples_to_do,
            source->direct_.filter_type_);

        parms->current_gains_ = parms->target_gains_;

        mix_samples(
            samples,
            source->direct_.channel_count_,
            *source->direct_.buffers_,
            parms->current_gains_.data(),
            parms->target_gains_.data(),
            0,
            0,
            samples_to_do);

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
            samples_to_do,
            source->aux_.filter_type_);

        parms->current_gains_ = parms->target_gains_;

        mix_samples(
            samples,
            source->aux_.channel_count_,
            *source->aux_.buffers_,
            parms->current_gains_.data(),
            parms->target_gains_.data(),
            0,
            0,
            samples_to_do);
    }

    return true;
}
