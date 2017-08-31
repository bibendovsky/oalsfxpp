/**
 * OpenAL cross platform audio library
 * Copyright (C) 2009 by Chris Robinson.
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


class EchoEffect :
    public IEffect
{
public:
    struct Tap
    {
        ALsizei delay;
    };


    EchoEffect()
        :
        IEffect{},
        sample_buffer{},
        buffer_length{},
        tap{},
        offset{},
        gains{},
        feed_gain{},
        filter{}
    {
    }

    virtual ~EchoEffect()
    {
    }


    ALfloat *sample_buffer;
    ALsizei buffer_length;

    // The echo is two tap. The delay is the number of samples from before the
    // current offset
    Tap tap[2];

    ALsizei offset;

    // The panning gains for the two taps
    ALfloat gains[2][MAX_OUTPUT_CHANNELS];

    ALfloat feed_gain;

    ALfilterState filter;


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
}; // EchoEffect


void EchoEffect::do_construct()
{
    buffer_length = 0;
    sample_buffer = NULL;

    tap[0].delay = 0;
    tap[1].delay = 0;
    offset = 0;

    ALfilterState_clear(&filter);
}

void EchoEffect::do_destruct()
{
    al_free(sample_buffer);
    sample_buffer = NULL;
}

ALboolean EchoEffect::do_update_device(
    ALCdevice* device)
{
    ALsizei maxlen, i;

    // Use the next power of 2 for the buffer length, so the tap offsets can be
    // wrapped using a mask instead of a modulo
    maxlen = fastf2i(AL_ECHO_MAX_DELAY * device->frequency) + 1;
    maxlen += fastf2i(AL_ECHO_MAX_LRDELAY * device->frequency) + 1;
    maxlen = NextPowerOf2(maxlen);

    if (maxlen != buffer_length)
    {
        void *temp = al_calloc(16, maxlen * sizeof(ALfloat));
        if (!temp) return AL_FALSE;

        al_free(sample_buffer);
        sample_buffer = static_cast<ALfloat*>(temp);
        buffer_length = maxlen;
    }
    for (i = 0; i < buffer_length; i++)
        sample_buffer[i] = 0.0f;

    return AL_TRUE;
}

void EchoEffect::do_update(
    const ALCdevice* device,
    const struct ALeffectslot* slot,
    const union ALeffectProps *props)
{
    ALuint frequency = device->frequency;
    ALfloat coeffs[MAX_AMBI_COEFFS];
    ALfloat effect_gain, lrpan, spread;

    tap[0].delay = fastf2i(props->echo.delay * frequency) + 1;
    tap[1].delay = fastf2i(props->echo.lr_delay * frequency);
    tap[1].delay += tap[0].delay;

    spread = props->echo.spread;
    if (spread < 0.0f) lrpan = -1.0f;
    else lrpan = 1.0f;
    /* Convert echo spread (where 0 = omni, +/-1 = directional) to coverage
    * spread (where 0 = point, tau = omni).
    */
    spread = asinf(1.0f - fabsf(spread))*4.0f;

    feed_gain = props->echo.feedback;

    effect_gain = maxf(1.0f - props->echo.damping, 0.0625f); /* Limit -24dB */
    ALfilterState_setParams(&filter, ALfilterType_HighShelf,
        effect_gain, LOWPASSFREQREF / frequency,
        calc_rcpQ_from_slope(effect_gain, 1.0f));

    effect_gain = 1.0F;

    /* First tap panning */
    CalcAngleCoeffs(-F_PI_2*lrpan, 0.0f, spread, coeffs);
    ComputePanningGains(device->dry, coeffs, effect_gain, gains[0]);

    /* Second tap panning */
    CalcAngleCoeffs(F_PI_2*lrpan, 0.0f, spread, coeffs);
    ComputePanningGains(device->dry, coeffs, effect_gain, gains[1]);
}

void EchoEffect::do_process(
    ALsizei sample_count,
    const ALfloat(*src_samples)[BUFFERSIZE],
    ALfloat(*dst_samples)[BUFFERSIZE],
    ALsizei channel_count)
{
    const ALsizei mask = buffer_length - 1;
    const ALsizei tap1 = tap[0].delay;
    const ALsizei tap2 = tap[1].delay;
    ALfloat x[2], y[2], in, out;
    ALsizei base, k;
    ALsizei i;

    x[0] = filter.x[0];
    x[1] = filter.x[1];
    y[0] = filter.y[0];
    y[1] = filter.y[1];
    for (base = 0; base < sample_count;)
    {
        ALfloat temps[128][2];
        ALsizei td = mini(128, sample_count - base);

        for (i = 0; i < td; i++)
        {
            /* First tap */
            temps[i][0] = sample_buffer[(offset - tap1) & mask];
            /* Second tap */
            temps[i][1] = sample_buffer[(offset - tap2) & mask];

            // Apply damping and feedback gain to the second tap, and mix in the
            // new sample
            in = temps[i][1] + src_samples[0][i + base];
            out = in*filter.b0 +
                x[0] * filter.b1 + x[1] * filter.b2 -
                y[0] * filter.a1 - y[1] * filter.a2;
            x[1] = x[0]; x[0] = in;
            y[1] = y[0]; y[0] = out;

            sample_buffer[offset&mask] = out * feed_gain;
            offset++;
        }

        for (k = 0; k < channel_count; k++)
        {
            ALfloat channel_gain = gains[0][k];
            if (fabsf(channel_gain) > GAIN_SILENCE_THRESHOLD)
            {
                for (i = 0; i < td; i++)
                    dst_samples[k][i + base] += temps[i][0] * channel_gain;
            }

            channel_gain = gains[1][k];
            if (fabsf(channel_gain) > GAIN_SILENCE_THRESHOLD)
            {
                for (i = 0; i < td; i++)
                    dst_samples[k][i + base] += temps[i][1] * channel_gain;
            }
        }

        base += td;
    }
    filter.x[0] = x[0];
    filter.x[1] = x[1];
    filter.y[0] = y[0];
    filter.y[1] = y[1];
}

IEffect* create_echo_effect()
{
    return create_effect<EchoEffect>();
}
