/**
 * OpenAL cross platform audio library
 * Copyright (C) 2011 by Chris Robinson.
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
#include "alFilter.h"
#include "alu.h"


class DedicatedEffect :
    public IEffect
{
public:
    DedicatedEffect()
        :
        IEffect{},
        gains{}
    {
    }

    virtual ~DedicatedEffect()
    {
    }


    ALfloat gains[MAX_OUTPUT_CHANNELS];


protected:
    void do_construct() final;

    void do_destruct() final;

    ALboolean do_update_device(
        ALCdevice* device) final;

    void do_update(
        const ALCdevice* device,
        const struct ALeffectslot* slot,
        const union ALeffectProps *props) final;

    void do_process(
        ALsizei sample_count,
        const ALfloat(*src_samples)[BUFFERSIZE],
        ALfloat(*dst_samples)[BUFFERSIZE],
        ALsizei channel_count) final;
}; // DedicatedEffect


void DedicatedEffect::do_construct()
{
    for (int i = 0; i < MAX_OUTPUT_CHANNELS; ++i)
    {
        gains[i] = 0.0f;
    }
}

void DedicatedEffect::do_destruct()
{
}

ALboolean DedicatedEffect::do_update_device(
    ALCdevice* device)
{
    static_cast<void>(device);
    return AL_TRUE;
}

void DedicatedEffect::do_update(
    const ALCdevice* device,
    const struct ALeffectslot* slot,
    const union ALeffectProps *props)
{
    ALfloat Gain;
    ALuint i;

    for (i = 0; i < MAX_OUTPUT_CHANNELS; i++)
        gains[i] = 0.0f;

    Gain = props->dedicated.gain;
    if (slot->params.effect_type == AL_EFFECT_DEDICATED_LOW_FREQUENCY_EFFECT)
    {
        int idx;
        if ((idx = GetChannelIdxByName(device->real_out, LFE)) != -1)
        {
            out_buffer = device->real_out.buffer;
            out_channels = device->real_out.num_channels;
            gains[idx] = Gain;
        }
    }
    else if (slot->params.effect_type == AL_EFFECT_DEDICATED_DIALOGUE)
    {
        int idx;
        /* Dialog goes to the front-center speaker if it exists, otherwise it
        * plays from the front-center location. */
        if ((idx = GetChannelIdxByName(device->real_out, FrontCenter)) != -1)
        {
            out_buffer = device->real_out.buffer;
            out_channels = device->real_out.num_channels;
            gains[idx] = Gain;
        }
        else
        {
            ALfloat coeffs[MAX_AMBI_COEFFS];
            CalcAngleCoeffs(0.0f, 0.0f, 0.0f, coeffs);

            out_buffer = device->dry.buffer;
            out_channels = device->dry.num_channels;
            ComputePanningGains(device->dry, coeffs, Gain, gains);
        }
    }
}

void DedicatedEffect::do_process(
    ALsizei sample_count,
    const ALfloat(*src_samples)[BUFFERSIZE],
    ALfloat(*dst_samples)[BUFFERSIZE],
    ALsizei channel_count)
{
    for (int c = 0; c < channel_count; c++)
    {
        const ALfloat gain = gains[c];
        if (!(fabsf(gain) > GAIN_SILENCE_THRESHOLD))
            continue;

        for (int i = 0; i < sample_count; i++)
            dst_samples[c][i] += src_samples[0][i] * gain;
    }
}

IEffect* create_dedicated_effect()
{
    return create_effect<DedicatedEffect>();
}
