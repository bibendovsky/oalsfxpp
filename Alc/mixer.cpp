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


static const ALfloat *DoFilters(ALfilterState *lpfilter, ALfilterState *hpfilter,
                                ALfloat *dst, const ALfloat *src,
                                ALsizei numsamples, enum ActiveFilters type)
{
    ALsizei i;
    switch(type)
    {
        case AF_None:
            ALfilterState_processPassthru(lpfilter, src, numsamples);
            ALfilterState_processPassthru(hpfilter, src, numsamples);
            break;

        case AF_LowPass:
            ALfilterState_process(lpfilter, dst, src, numsamples);
            ALfilterState_processPassthru(hpfilter, dst, numsamples);
            return dst;
        case AF_HighPass:
            ALfilterState_processPassthru(lpfilter, src, numsamples);
            ALfilterState_process(hpfilter, dst, src, numsamples);
            return dst;

        case AF_BandPass:
            for(i = 0;i < numsamples;)
            {
                ALfloat temp[256];
                ALsizei todo = mini(256, numsamples-i);

                ALfilterState_process(lpfilter, temp, src+i, todo);
                ALfilterState_process(hpfilter, dst+i, temp, todo);
                i += todo;
            }
            return dst;
    }
    return src;
}

ALboolean MixSource(ALvoice *voice, ALsource *Source, ALCdevice *Device, ALsizei SamplesToDo)
{
    ALsizei NumChannels;
    ALsizei chan;
    ALsizei send;

    /* Get source info */
    NumChannels = voice->num_channels;

    for (chan = 0; chan < NumChannels; ++chan)
    {
        DirectParams* parms;
        const ALfloat* samples;

        /* Load what's left to play from the source buffer, and
            * clear the rest of the temp buffer */
        memcpy(Device->source_data, Device->source_samples, NumChannels * 4 * SamplesToDo);

        /* Now resample, then filter and mix to the appropriate outputs. */
        memcpy(Device->resampled_data, Device->source_data, SamplesToDo*sizeof(ALfloat));

        parms = &voice->direct.params[chan];

        samples = DoFilters(
            &parms->low_pass,
            &parms->high_pass,
            Device->filtered_data,
            Device->resampled_data,
            SamplesToDo,
            voice->direct.filter_type);

        memcpy(parms->gains.current, parms->gains.target, sizeof(parms->gains.current));

        MixSamples(
            samples,
            voice->direct.channels,
            voice->direct.buffer,
            parms->gains.current,
            parms->gains.target,
            0,
            0,
            SamplesToDo);

        for (send = 0; send < Device->num_aux_sends; ++send)
        {
            SendParams *parms = &voice->send[send].params[chan];

            if (!voice->send[send].buffer)
            {
                continue;
            }

            samples = DoFilters(
                &parms->low_pass,
                &parms->high_pass,
                Device->filtered_data,
                Device->resampled_data,
                SamplesToDo,
                voice->send[send].filter_type);

            memcpy(parms->gains.current, parms->gains.target, sizeof(parms->gains.current));

            MixSamples(
                samples,
                voice->send[send].channels,
                voice->send[send].buffer,
                parms->gains.current,
                parms->gains.target,
                0,
                0,
                SamplesToDo);
        }
    }

    return 1;
}
