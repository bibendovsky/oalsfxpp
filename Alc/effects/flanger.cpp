/**
 * OpenAL cross platform audio library
 * Copyright (C) 2013 by Mike Gorchak
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


enum FlangerWaveForm {
    FWF_Triangle = AL_FLANGER_WAVEFORM_TRIANGLE,
    FWF_Sinusoid = AL_FLANGER_WAVEFORM_SINUSOID
};

class FlangerEffect :
    public IEffect
{
public:
    FlangerEffect()
        :
        IEffect{},
        SampleBuffer{},
        BufferLength{},
        offset{},
        lfo_range{},
        lfo_scale{},
        lfo_disp{},
        Gain{},
        waveform{},
        delay{},
        depth{},
        feedback{}
    {
    }

    virtual ~FlangerEffect()
    {
    }


    ALfloat *SampleBuffer[2];
    ALsizei BufferLength;
    ALsizei offset;
    ALsizei lfo_range;
    ALfloat lfo_scale;
    ALint lfo_disp;

    // Gains for left and right sides
    ALfloat Gain[2][MAX_OUTPUT_CHANNELS];

    // effect parameters
    enum FlangerWaveForm waveform;
    ALint delay;
    ALfloat depth;
    ALfloat feedback;


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
}; // FlangerEffect


static void GetTriangleDelays(ALint *delays, ALsizei offset, const ALsizei lfo_range,
                              const ALfloat lfo_scale, const ALfloat depth, const ALsizei delay,
                              const ALsizei todo)
{
    ALsizei i;
    for(i = 0;i < todo;i++)
    {
        delays[i] = fastf2i((1.0f - fabsf(2.0f - lfo_scale*offset)) * depth) + delay;
        offset = (offset+1)%lfo_range;
    }
}

static void GetSinusoidDelays(ALint *delays, ALsizei offset, const ALsizei lfo_range,
                              const ALfloat lfo_scale, const ALfloat depth, const ALsizei delay,
                              const ALsizei todo)
{
    ALsizei i;
    for(i = 0;i < todo;i++)
    {
        delays[i] = fastf2i(sinf(lfo_scale*offset) * depth) + delay;
        offset = (offset+1)%lfo_range;
    }
}


void FlangerEffect::do_construct()
{
    BufferLength = 0;
    SampleBuffer[0] = nullptr;
    SampleBuffer[1] = nullptr;
    offset = 0;
    lfo_range = 1;
    waveform = FWF_Triangle;
}

void FlangerEffect::do_destruct()
{
    al_free(SampleBuffer[0]);
    SampleBuffer[0] = NULL;
    SampleBuffer[1] = NULL;
}

ALboolean FlangerEffect::do_update_device(
    ALCdevice* Device)
{
    ALsizei maxlen;
    ALsizei it;

    maxlen = fastf2i(AL_FLANGER_MAX_DELAY * 2.0f * Device->frequency) + 1;
    maxlen = NextPowerOf2(maxlen);

    if (maxlen != BufferLength)
    {
        void *temp = al_calloc(16, maxlen * sizeof(ALfloat) * 2);
        if (!temp) return AL_FALSE;

        al_free(SampleBuffer[0]);
        SampleBuffer[0] = static_cast<ALfloat*>(temp);
        SampleBuffer[1] = SampleBuffer[0] + maxlen;

        BufferLength = maxlen;
    }

    for (it = 0; it < BufferLength; it++)
    {
        SampleBuffer[0][it] = 0.0f;
        SampleBuffer[1][it] = 0.0f;
    }

    return AL_TRUE;
}

void FlangerEffect::do_update(
    const ALCdevice* Device,
    const struct ALeffectslot* slot,
    const union ALeffectProps *props)
{
    ALfloat frequency = (ALfloat)Device->frequency;
    ALfloat coeffs[MAX_AMBI_COEFFS];
    ALfloat rate;
    ALint phase;

    switch (props->flanger.waveform)
    {
    case AL_FLANGER_WAVEFORM_TRIANGLE:
        waveform = FWF_Triangle;
        break;
    case AL_FLANGER_WAVEFORM_SINUSOID:
        waveform = FWF_Sinusoid;
        break;
    }
    feedback = props->flanger.feedback;
    delay = fastf2i(props->flanger.delay * frequency);
    /* The LFO depth is scaled to be relative to the sample delay. */
    depth = props->flanger.depth * delay;

    /* Gains for left and right sides */
    CalcAngleCoeffs(-F_PI_2, 0.0f, 0.0f, coeffs);
    ComputePanningGains(Device->dry, coeffs, 1.0F, Gain[0]);
    CalcAngleCoeffs(F_PI_2, 0.0f, 0.0f, coeffs);
    ComputePanningGains(Device->dry, coeffs, 1.0F, Gain[1]);

    phase = props->flanger.phase;
    rate = props->flanger.rate;
    if (!(rate > 0.0f))
    {
        lfo_scale = 0.0f;
        lfo_range = 1;
        lfo_disp = 0;
    }
    else
    {
        /* Calculate LFO coefficient */
        lfo_range = fastf2i(frequency / rate + 0.5f);
        switch (waveform)
        {
        case FWF_Triangle:
            lfo_scale = 4.0f / lfo_range;
            break;
        case FWF_Sinusoid:
            lfo_scale = F_TAU / lfo_range;
            break;
        }

        /* Calculate lfo phase displacement */
        if (phase >= 0)
            lfo_disp = fastf2i(lfo_range * (phase / 360.0f));
        else
            lfo_disp = fastf2i(lfo_range * ((360 + phase) / 360.0f));
    }
}

void FlangerEffect::do_process(
    ALsizei SamplesToDo,
    const ALfloat(*SamplesIn)[BUFFERSIZE],
    ALfloat(*SamplesOut)[BUFFERSIZE],
    ALsizei NumChannels)
{
    ALfloat *leftbuf = SampleBuffer[0];
    ALfloat *rightbuf = SampleBuffer[1];
    const ALsizei bufmask = BufferLength - 1;
    ALsizei i, c;
    ALsizei base;

    for (base = 0; base < SamplesToDo;)
    {
        const ALsizei todo = mini(128, SamplesToDo - base);
        ALfloat temps[128][2];
        ALint moddelays[2][128];

        switch (waveform)
        {
        case FWF_Triangle:
            GetTriangleDelays(moddelays[0], offset%lfo_range, lfo_range,
                lfo_scale, depth, delay, todo);
            GetTriangleDelays(moddelays[1], (offset + lfo_disp) % lfo_range,
                lfo_range, lfo_scale, depth, delay,
                todo);
            break;
        case FWF_Sinusoid:
            GetSinusoidDelays(moddelays[0], offset%lfo_range, lfo_range,
                lfo_scale, depth, delay, todo);
            GetSinusoidDelays(moddelays[1], (offset + lfo_disp) % lfo_range,
                lfo_range, lfo_scale, depth, delay,
                todo);
            break;
        }

        for (i = 0; i < todo; i++)
        {
            leftbuf[offset&bufmask] = SamplesIn[0][base + i];
            temps[i][0] = leftbuf[(offset - moddelays[0][i])&bufmask] * feedback;
            leftbuf[offset&bufmask] += temps[i][0];

            rightbuf[offset&bufmask] = SamplesIn[0][base + i];
            temps[i][1] = rightbuf[(offset - moddelays[1][i])&bufmask] * feedback;
            rightbuf[offset&bufmask] += temps[i][1];

            offset++;
        }

        for (c = 0; c < NumChannels; c++)
        {
            ALfloat gain = Gain[0][c];
            if (fabsf(gain) > GAIN_SILENCE_THRESHOLD)
            {
                for (i = 0; i < todo; i++)
                    SamplesOut[c][i + base] += temps[i][0] * gain;
            }

            gain = Gain[1][c];
            if (fabsf(gain) > GAIN_SILENCE_THRESHOLD)
            {
                for (i = 0; i < todo; i++)
                    SamplesOut[c][i + base] += temps[i][1] * gain;
            }
        }

        base += todo;
    }

    offset = offset;
}

IEffect* create_flanger_effect()
{
    return create_effect<FlangerEffect>();
}
