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
            al_filter_state_process_pass_through(lp_filter, src, num_samples);
            al_filter_state_process_pass_through(hp_filter, src, num_samples);
            break;

        case ActiveFilters::low_pass:
            al_filter_state_process_c(lp_filter, dst, src, num_samples);
            al_filter_state_process_pass_through(hp_filter, dst, num_samples);
            return dst;
        case ActiveFilters::high_pass:
            al_filter_state_process_pass_through(lp_filter, src, num_samples);
            al_filter_state_process_c(hp_filter, dst, src, num_samples);
            return dst;

        case ActiveFilters::band_pass:
            for(i = 0;i < num_samples;)
            {
                float temp[256];
                int todo = std::min(256, num_samples-i);

                al_filter_state_process_c(lp_filter, temp, src+i, todo);
                al_filter_state_process_c(hp_filter, dst+i, temp, todo);
                i += todo;
            }
            return dst;
    }
    return src;
}

bool mix_source(ALvoice* voice, ALsource* source, ALCdevice* device, int samples_to_do)
{
    int chan;
    const auto channel_count = device->channel_count_;

    for (chan = 0; chan < channel_count; ++chan)
    {
        DirectParams* parms;
        const float* samples;

        /* Load what's left to play from the source buffer, and
            * clear the rest of the temp buffer */
        std::uninitialized_copy_n(device->source_samples_, channel_count * samples_to_do, device->source_data_.begin());

        /* Now resample, then filter and mix to the appropriate outputs. */
        std::uninitialized_copy_n(device->source_data_.cbegin(), samples_to_do, device->resampled_data_.begin());


        parms = &voice->direct_.params_[chan];

        samples = do_filters(
            &parms->low_pass_,
            &parms->high_pass_,
            device->filtered_data_.data(),
            device->resampled_data_.data(),
            samples_to_do,
            voice->direct_.filter_type_);

        memcpy(parms->gains_.current_, parms->gains_.target_, sizeof(parms->gains_.current_));

        mix_samples(
            samples,
            voice->direct_.channel_count_,
            *voice->direct_.buffers_,
            parms->gains_.current_,
            parms->gains_.target_,
            0,
            0,
            samples_to_do);

        if (!voice->send_.buffers_)
        {
            continue;
        }

        parms = &voice->send_.params_[chan];

        samples = do_filters(
            &parms->low_pass_,
            &parms->high_pass_,
            device->filtered_data_.data(),
            device->resampled_data_.data(),
            samples_to_do,
            voice->send_.filter_type_);

        memcpy(parms->gains_.current_, parms->gains_.target_, sizeof(parms->gains_.current_));

        mix_samples(
            samples,
            voice->send_.channel_count_,
            *voice->send_.buffers_,
            parms->gains_.current_,
            parms->gains_.target_,
            0,
            0,
            samples_to_do);
    }

    return true;
}
