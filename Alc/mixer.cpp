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
        case AF_None:
            al_filter_state_process_pass_through(lp_filter, src, num_samples);
            al_filter_state_process_pass_through(hp_filter, src, num_samples);
            break;

        case AF_LowPass:
            al_filter_state_process_c(lp_filter, dst, src, num_samples);
            al_filter_state_process_pass_through(hp_filter, dst, num_samples);
            return dst;
        case AF_HighPass:
            al_filter_state_process_pass_through(lp_filter, src, num_samples);
            al_filter_state_process_c(hp_filter, dst, src, num_samples);
            return dst;

        case AF_BandPass:
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
    int NumChannels;
    int chan;

    /* Get source info */
    NumChannels = voice->num_channels;

    for (chan = 0; chan < NumChannels; ++chan)
    {
        DirectParams* parms;
        const float* samples;

        /* Load what's left to play from the source buffer, and
            * clear the rest of the temp buffer */
        std::uninitialized_copy_n(device->source_samples, NumChannels * samples_to_do, device->source_data.begin());

        /* Now resample, then filter and mix to the appropriate outputs. */
        std::uninitialized_copy_n(device->source_data.cbegin(), samples_to_do, device->resampled_data.begin());


        parms = &voice->direct.params[chan];

        samples = do_filters(
            &parms->low_pass,
            &parms->high_pass,
            device->filtered_data.data(),
            device->resampled_data.data(),
            samples_to_do,
            voice->direct.filter_type);

        memcpy(parms->gains.current, parms->gains.target, sizeof(parms->gains.current));

        mix_samples(
            samples,
            voice->direct.channels,
            *voice->direct.buffer,
            parms->gains.current,
            parms->gains.target,
            0,
            0,
            samples_to_do);

        if (device->num_aux_sends > 0)
        {
            SendParams *parms = &voice->send.params[chan];

            if (!voice->send.buffer)
            {
                continue;
            }

            samples = do_filters(
                &parms->low_pass,
                &parms->high_pass,
                device->filtered_data.data(),
                device->resampled_data.data(),
                samples_to_do,
                voice->send.filter_type);

            memcpy(parms->gains.current, parms->gains.target, sizeof(parms->gains.current));

            mix_samples(
                samples,
                voice->send.channels,
                *voice->send.buffer,
                parms->gains.current,
                parms->gains.target,
                0,
                0,
                samples_to_do);
        }
    }

    return true;
}
