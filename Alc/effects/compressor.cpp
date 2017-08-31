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
        Gain{},
        Enabled{},
        AttackRate{},
        ReleaseRate{},
        GainCtrl{}
    {
    }

    virtual ~CompressorEffect()
    {
    }


    // Effect gains for each channel
    ALfloat Gain[MAX_EFFECT_CHANNELS][MAX_OUTPUT_CHANNELS];

    // Effect parameters
    ALboolean Enabled;
    ALfloat AttackRate;
    ALfloat ReleaseRate;
    ALfloat GainCtrl;


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
        ALsizei samplesToDo,
        const ALfloat(*samplesIn)[BUFFERSIZE],
        ALfloat(*samplesOut)[BUFFERSIZE],
        ALsizei numChannels) final;
}; // CompressorEffect


void CompressorEffect::do_construct()
{
    Enabled = AL_TRUE;
    AttackRate = 0.0f;
    ReleaseRate = 0.0f;
    GainCtrl = 1.0f;
}

void CompressorEffect::do_destruct()
{
}

ALboolean CompressorEffect::do_update_device(
    ALCdevice* device)
{
    const ALfloat attackTime = device->frequency * 0.2f; /* 200ms Attack */
    const ALfloat releaseTime = device->frequency * 0.4f; /* 400ms Release */

    AttackRate = 1.0f / attackTime;
    ReleaseRate = 1.0f / releaseTime;

    return AL_TRUE;
}

void CompressorEffect::do_update(
    const ALCdevice* device,
    const struct ALeffectslot* slot,
    const union ALeffectProps *props)
{
    ALuint i;

    Enabled = props->compressor.on_off;

    out_buffer = device->foa_out.buffer;
    out_channels = device->foa_out.num_channels;

    for (i = 0; i < 4; i++)
        ComputeFirstOrderGains(device->foa_out, IdentityMatrixf.m[i],
            1.0F, Gain[i]);
}

void CompressorEffect::do_process(
    ALsizei SamplesToDo,
    const ALfloat(*SamplesIn)[BUFFERSIZE],
    ALfloat(*SamplesOut)[BUFFERSIZE],
    ALsizei NumChannels)
{
    ALsizei i, j, k;
    ALsizei base;

    for (base = 0; base < SamplesToDo;)
    {
        ALfloat temps[64][4];
        ALsizei td = mini(64, SamplesToDo - base);

        /* Load samples into the temp buffer first. */
        for (j = 0; j < 4; j++)
        {
            for (i = 0; i < td; i++)
                temps[i][j] = SamplesIn[j][i + base];
        }

        if (Enabled)
        {
            ALfloat gain = GainCtrl;
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
                    gain = minf(gain + AttackRate, amplitude);
                else if (amplitude < gain)
                    gain = maxf(gain - ReleaseRate, amplitude);

                /* Apply the inverse of the gain control to normalize/compress
                * the volume. */
                output = 1.0f / clampf(gain, 0.5f, 2.0f);
                for (j = 0; j < 4; j++)
                    temps[i][j] *= output;
            }

            GainCtrl = gain;
        }
        else
        {
            ALfloat gain = GainCtrl;
            ALfloat output, amplitude;

            for (i = 0; i < td; i++)
            {
                /* Same as above, except the amplitude is forced to 1. This
                * helps ensure smooth gain changes when the compressor is
                * turned on and off.
                */
                amplitude = 1.0f;
                if (amplitude > gain)
                    gain = minf(gain + AttackRate, amplitude);
                else if (amplitude < gain)
                    gain = maxf(gain - ReleaseRate, amplitude);

                output = 1.0f / clampf(gain, 0.5f, 2.0f);
                for (j = 0; j < 4; j++)
                    temps[i][j] *= output;
            }

            GainCtrl = gain;
        }

        /* Now mix to the output. */
        for (j = 0; j < 4; j++)
        {
            for (k = 0; k < NumChannels; k++)
            {
                ALfloat gain = Gain[j][k];
                if (!(fabsf(gain) > GAIN_SILENCE_THRESHOLD))
                    continue;

                for (i = 0; i < td; i++)
                    SamplesOut[k][base + i] += gain * temps[i][j];
            }
        }

        base += td;
    }
}

IEffect* create_compressor_effect()
{
    return create_effect<CompressorEffect>();
}
