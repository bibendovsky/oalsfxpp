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


#include <algorithm>
#include <array>
#include <vector>
#include "config.h"
#include "alFilter.h"
#include "alu.h"


enum ChorusWaveForm {
    CWF_Triangle = AL_CHORUS_WAVEFORM_TRIANGLE,
    CWF_Sinusoid = AL_CHORUS_WAVEFORM_SINUSOID
};


using ChorusSampleBuffer = std::vector<ALfloat>;
using ChorusSampleBuffers = std::array<ChorusSampleBuffer, 2>;


class ChorusEffect :
    public IEffect
{
public:
    ChorusEffect()
        :
        IEffect{},
        sample_buffers{},
        buffer_length{},
        offset{},
        lfo_range{},
        lfo_scale{},
        lfo_disp{},
        gain{},
        waveform{},
        delay{},
        depth{},
        feedback{}
    {
    }

    virtual ~ChorusEffect()
    {
    }


    ChorusSampleBuffers sample_buffers;
    ALsizei buffer_length;
    ALsizei offset;
    ALsizei lfo_range;
    ALfloat lfo_scale;
    ALint lfo_disp;

    // Gains for left and right sides
    ALfloat gain[2][MAX_OUTPUT_CHANNELS];

    // effect parameters
    ChorusWaveForm waveform;
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
        ALsizei sample_count,
        const ALfloat(*src_samples)[BUFFERSIZE],
        ALfloat(*dst_samples)[BUFFERSIZE],
        ALsizei channel_count) final;
}; // ChorusEffect


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


void ChorusEffect::do_construct()
{
    buffer_length = 0;

    for (auto& buffer : sample_buffers)
    {
        buffer = ChorusSampleBuffer{};
    }

    offset = 0;
    lfo_range = 1;
    waveform = CWF_Triangle;
}

void ChorusEffect::do_destruct()
{
    for (auto& buffer : sample_buffers)
    {
        buffer = ChorusSampleBuffer{};
    }
}

ALboolean ChorusEffect::do_update_device(
    ALCdevice* device)
{
    ALsizei maxlen;

    maxlen = fastf2i(AL_CHORUS_MAX_DELAY * 2.0f * device->frequency) + 1;
    maxlen = NextPowerOf2(maxlen);

    if (maxlen != buffer_length)
    {
        void *temp = al_calloc(16, maxlen * sizeof(ALfloat) * 2);
        if (!temp) return AL_FALSE;

        sample_buffers[0].resize(maxlen);
        sample_buffers[1].resize(maxlen);

        buffer_length = maxlen;
    }

    for (auto& buffer : sample_buffers)
    {
        std::fill(buffer.begin(), buffer.end(), 0.0F);
    }

    return AL_TRUE;
}

void ChorusEffect::do_update(
    const ALCdevice* device,
    const struct ALeffectslot* slot,
    const union ALeffectProps *props)
{
    ALfloat frequency = (ALfloat)device->frequency;
    ALfloat coeffs[MAX_AMBI_COEFFS];
    ALfloat rate;
    ALint phase;

    switch (props->chorus.waveform)
    {
    case AL_CHORUS_WAVEFORM_TRIANGLE:
        waveform = CWF_Triangle;
        break;
    case AL_CHORUS_WAVEFORM_SINUSOID:
        waveform = CWF_Sinusoid;
        break;
    }
    feedback = props->chorus.feedback;
    delay = fastf2i(props->chorus.delay * frequency);
    /* The LFO depth is scaled to be relative to the sample delay. */
    depth = props->chorus.depth * delay;

    /* Gains for left and right sides */
    CalcAngleCoeffs(-F_PI_2, 0.0f, 0.0f, coeffs);
    ComputePanningGains(device->dry, coeffs, 1.0F, gain[0]);
    CalcAngleCoeffs(F_PI_2, 0.0f, 0.0f, coeffs);
    ComputePanningGains(device->dry, coeffs, 1.0F, gain[1]);

    phase = props->chorus.phase;
    rate = props->chorus.rate;
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
        case CWF_Triangle:
            lfo_scale = 4.0f / lfo_range;
            break;
        case CWF_Sinusoid:
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

void ChorusEffect::do_process(
    ALsizei sample_count,
    const ALfloat(*src_samples)[BUFFERSIZE],
    ALfloat(*dst_samples)[BUFFERSIZE],
    ALsizei channel_count)
{
    auto& leftbuf = sample_buffers[0];
    auto& rightbuf = sample_buffers[1];
    const ALsizei bufmask = buffer_length - 1;
    ALsizei i, c;
    ALsizei base;

    for (base = 0; base < sample_count;)
    {
        const ALsizei todo = mini(128, sample_count - base);
        ALfloat temps[128][2];
        ALint moddelays[2][128];

        switch (waveform)
        {
        case CWF_Triangle:
            GetTriangleDelays(moddelays[0], offset%lfo_range, lfo_range,
                lfo_scale, depth, delay, todo);
            GetTriangleDelays(moddelays[1], (offset + lfo_disp) % lfo_range,
                lfo_range, lfo_scale, depth, delay,
                todo);
            break;
        case CWF_Sinusoid:
            GetSinusoidDelays(moddelays[0], offset%lfo_range, lfo_range,
                lfo_scale, depth, delay, todo);
            GetSinusoidDelays(moddelays[1], (offset + lfo_disp) % lfo_range,
                lfo_range, lfo_scale, depth, delay,
                todo);
            break;
        }

        for (i = 0; i < todo; i++)
        {
            leftbuf[offset&bufmask] = src_samples[0][base + i];
            temps[i][0] = leftbuf[(offset - moddelays[0][i])&bufmask] * feedback;
            leftbuf[offset&bufmask] += temps[i][0];

            rightbuf[offset&bufmask] = src_samples[0][base + i];
            temps[i][1] = rightbuf[(offset - moddelays[1][i])&bufmask] * feedback;
            rightbuf[offset&bufmask] += temps[i][1];

            offset++;
        }

        for (c = 0; c < channel_count; c++)
        {
            ALfloat channel_gain = gain[0][c];
            if (fabsf(channel_gain) > GAIN_SILENCE_THRESHOLD)
            {
                for (i = 0; i < todo; i++)
                    dst_samples[c][i + base] += temps[i][0] * channel_gain;
            }

            channel_gain = gain[1][c];
            if (fabsf(channel_gain) > GAIN_SILENCE_THRESHOLD)
            {
                for (i = 0; i < todo; i++)
                    dst_samples[c][i + base] += temps[i][1] * channel_gain;
            }
        }

        base += todo;
    }
}

IEffect* create_chorus_effect()
{
    return create_effect<ChorusEffect>();
}
