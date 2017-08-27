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

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "alMain.h"
#include "AL/al.h"
#include "AL/alc.h"
#include "alSource.h"
#include "alBuffer.h"
#include "alAuxEffectSlot.h"
#include "alu.h"

#include "mixer_defs.h"


static_assert((INT_MAX>>FRACTIONBITS)/MAX_PITCH > BUFFERSIZE,
              "MAX_PITCH and/or BUFFERSIZE are too large for FRACTIONBITS!");

extern inline void InitiatePositionArrays(ALsizei frac, ALint increment, ALsizei *restrict frac_arr, ALint *restrict pos_arr, ALsizei size);


static MixerFunc MixSamples = Mix_C;


static const ALfloat *DoFilters(ALfilterState *lpfilter, ALfilterState *hpfilter,
                                ALfloat *restrict dst, const ALfloat *restrict src,
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
    NumChannels = voice->NumChannels;

    for (chan = 0; chan < NumChannels; ++chan)
    {
        DirectParams* parms;
        const ALfloat* samples;

        /* Load what's left to play from the source buffer, and
            * clear the rest of the temp buffer */
        memcpy(Device->SourceData, Device->source_data, NumChannels * 4 * SamplesToDo);

        /* Now resample, then filter and mix to the appropriate outputs. */
        memcpy(Device->ResampledData, Device->SourceData, SamplesToDo*sizeof(ALfloat));

        parms = &voice->Direct.Params[chan];

        samples = DoFilters(
            &parms->LowPass,
            &parms->HighPass,
            Device->FilteredData,
            Device->ResampledData,
            SamplesToDo,
            voice->Direct.FilterType);

        memcpy(parms->Gains.Current, parms->Gains.Target, sizeof(parms->Gains.Current));

        MixSamples(
            samples,
            voice->Direct.Channels,
            voice->Direct.Buffer,
            parms->Gains.Current,
            parms->Gains.Target,
            0,
            0,
            SamplesToDo);

        for (send = 0; send < Device->NumAuxSends; ++send)
        {
            SendParams *parms = &voice->Send[send].Params[chan];

            if (!voice->Send[send].Buffer)
            {
                continue;
            }

            samples = DoFilters(
                &parms->LowPass,
                &parms->HighPass,
                Device->FilteredData,
                Device->ResampledData,
                SamplesToDo,
                voice->Send[send].FilterType);

            memcpy(parms->Gains.Current, parms->Gains.Target, sizeof(parms->Gains.Current));

            MixSamples(
                samples,
                voice->Send[send].Channels,
                voice->Send[send].Buffer,
                parms->Gains.Current,
                parms->Gains.Target,
                0,
                0,
                SamplesToDo);
        }
    }

    return 1;
}
