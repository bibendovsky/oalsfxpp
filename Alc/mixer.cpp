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


static MixerFunc MixSamples = Mix_C;


static const float *DoFilters(ALfilterState *lpfilter, ALfilterState *hpfilter,
                                float *dst, const float *src,
                                int numsamples, enum ActiveFilters type)
{
    int i;
    switch(type)
    {
        case AF_None:
            ALfilterState_processPassthru(lpfilter, src, numsamples);
            ALfilterState_processPassthru(hpfilter, src, numsamples);
            break;

        case AF_LowPass:
            ALfilterState_processC(lpfilter, dst, src, numsamples);
            ALfilterState_processPassthru(hpfilter, dst, numsamples);
            return dst;
        case AF_HighPass:
            ALfilterState_processPassthru(lpfilter, src, numsamples);
            ALfilterState_processC(hpfilter, dst, src, numsamples);
            return dst;

        case AF_BandPass:
            for(i = 0;i < numsamples;)
            {
                float temp[256];
                int todo = std::min(256, numsamples-i);

                ALfilterState_processC(lpfilter, temp, src+i, todo);
                ALfilterState_processC(hpfilter, dst+i, temp, todo);
                i += todo;
            }
            return dst;
    }
    return src;
}

ALboolean MixSource(ALvoice *voice, ALsource *Source, ALCdevice *Device, int SamplesToDo)
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
        std::uninitialized_copy_n(Device->source_samples, NumChannels * SamplesToDo, Device->source_data.begin());

        /* Now resample, then filter and mix to the appropriate outputs. */
        std::uninitialized_copy_n(Device->source_data.cbegin(), SamplesToDo, Device->resampled_data.begin());


        parms = &voice->direct.params[chan];

        samples = DoFilters(
            &parms->low_pass,
            &parms->high_pass,
            Device->filtered_data.data(),
            Device->resampled_data.data(),
            SamplesToDo,
            voice->direct.filter_type);

        memcpy(parms->gains.current, parms->gains.target, sizeof(parms->gains.current));

        MixSamples(
            samples,
            voice->direct.channels,
            *voice->direct.buffer,
            parms->gains.current,
            parms->gains.target,
            0,
            0,
            SamplesToDo);

        if (Device->num_aux_sends > 0)
        {
            SendParams *parms = &voice->send.params[chan];

            if (!voice->send.buffer)
            {
                continue;
            }

            samples = DoFilters(
                &parms->low_pass,
                &parms->high_pass,
                Device->filtered_data.data(),
                Device->resampled_data.data(),
                SamplesToDo,
                voice->send.filter_type);

            memcpy(parms->gains.current, parms->gains.target, sizeof(parms->gains.current));

            MixSamples(
                samples,
                voice->send.channels,
                *voice->send.buffer,
                parms->gains.current,
                parms->gains.target,
                0,
                0,
                SamplesToDo);
        }
    }

    return 1;
}
