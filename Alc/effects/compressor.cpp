/**
 * OpenAL cross platform audio library
 * Copyright (C) 2013 by Anis A. Hireche
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
#include "alu.h"


class CompressorEffect :
    public IEffect
{
public:
    CompressorEffect()
        :
        IEffect{},
        gain{},
        is_enabled{},
        attack_rate{},
        release_rate{},
        gain_control{}
    {
    }

    virtual ~CompressorEffect()
    {
    }


    // Effect gains for each channel
    ALfloat gain[MAX_EFFECT_CHANNELS][MAX_OUTPUT_CHANNELS];

    // Effect parameters
    ALboolean is_enabled;
    ALfloat attack_rate;
    ALfloat release_rate;
    ALfloat gain_control;


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
}; // CompressorEffect


void CompressorEffect::do_construct()
{
    is_enabled = AL_TRUE;
    attack_rate = 0.0f;
    release_rate = 0.0f;
    gain_control = 1.0f;
}

void CompressorEffect::do_destruct()
{
}

ALboolean CompressorEffect::do_update_device(
    ALCdevice* device)
{
    const ALfloat attackTime = device->frequency * 0.2f; /* 200ms Attack */
    const ALfloat releaseTime = device->frequency * 0.4f; /* 400ms Release */

    attack_rate = 1.0f / attackTime;
    release_rate = 1.0f / releaseTime;

    return AL_TRUE;
}

void CompressorEffect::do_update(
    const ALCdevice* device,
    const struct ALeffectslot* slot,
    const union ALeffectProps *props)
{
    ALuint i;

    is_enabled = props->compressor.on_off;

    out_buffer = device->foa_out.buffer;
    out_channels = device->foa_out.num_channels;

    for (i = 0; i < 4; i++)
        ComputeFirstOrderGains(device->foa_out, IdentityMatrixf.m[i],
            1.0F, gain[i]);
}

void CompressorEffect::do_process(
    ALsizei sample_count,
    const ALfloat(*src_samples)[BUFFERSIZE],
    ALfloat(*dst_samples)[BUFFERSIZE],
    ALsizei channel_count)
{
    ALsizei i, j, k;
    ALsizei base;

    for (base = 0; base < sample_count;)
    {
        ALfloat temps[64][4];
        ALsizei td = mini(64, sample_count - base);

        /* Load samples into the temp buffer first. */
        for (j = 0; j < 4; j++)
        {
            for (i = 0; i < td; i++)
                temps[i][j] = src_samples[j][i + base];
        }

        if (is_enabled)
        {
            ALfloat gain = gain_control;
            ALfloat output, amplitude;

            for (i = 0; i < td; i++)
            {
                /* Roughly calculate the maximum amplitude from the 4-channel
                * signal, and attack or release the gain control to reach it.
                */
                amplitude = fabsf(temps[i][0]);
                amplitude = maxf(amplitude + fabsf(temps[i][1]),
                    maxf(amplitude + fabsf(temps[i][2]),
                        amplitude + fabsf(temps[i][3])));
                if (amplitude > gain)
                    gain = minf(gain + attack_rate, amplitude);
                else if (amplitude < gain)
                    gain = maxf(gain - release_rate, amplitude);

                /* Apply the inverse of the gain control to normalize/compress
                * the volume. */
                output = 1.0f / clampf(gain, 0.5f, 2.0f);
                for (j = 0; j < 4; j++)
                    temps[i][j] *= output;
            }

            gain_control = gain;
        }
        else
        {
            ALfloat gain = gain_control;
            ALfloat output, amplitude;

            for (i = 0; i < td; i++)
            {
                /* Same as above, except the amplitude is forced to 1. This
                * helps ensure smooth gain changes when the compressor is
                * turned on and off.
                */
                amplitude = 1.0f;
                if (amplitude > gain)
                    gain = minf(gain + attack_rate, amplitude);
                else if (amplitude < gain)
                    gain = maxf(gain - release_rate, amplitude);

                output = 1.0f / clampf(gain, 0.5f, 2.0f);
                for (j = 0; j < 4; j++)
                    temps[i][j] *= output;
            }

            gain_control = gain;
        }

        /* Now mix to the output. */
        for (j = 0; j < 4; j++)
        {
            for (k = 0; k < channel_count; k++)
            {
                ALfloat channel_gain = gain[j][k];
                if (!(fabsf(channel_gain) > GAIN_SILENCE_THRESHOLD))
                    continue;

                for (i = 0; i < td; i++)
                    dst_samples[k][base + i] += channel_gain * temps[i][j];
            }
        }

        base += td;
    }
}

IEffect* create_compressor_effect()
{
    return create_effect<CompressorEffect>();
}
