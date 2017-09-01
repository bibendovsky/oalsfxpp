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


/*  The document  "Effects Extension Guide.pdf"  says that low and high  *
 *  frequencies are cutoff frequencies. This is not fully correct, they  *
 *  are corner frequencies for low and high shelf filters. If they were  *
 *  just cutoff frequencies, there would be no need in cutoff frequency  *
 *  gains, which are present.  Documentation for  "Creative Proteus X2"  *
 *  software describes  4-band equalizer functionality in a much better  *
 *  way.  This equalizer seems  to be a predecessor  of  OpenAL  4-band  *
 *  equalizer.  With low and high  shelf filters  we are able to cutoff  *
 *  frequencies below and/or above corner frequencies using attenuation  *
 *  gains (below 1.0) and amplify all low and/or high frequencies using  *
 *  gains above 1.0.                                                     *
 *                                                                       *
 *     Low-shelf       Low Mid Band      High Mid Band     High-shelf    *
 *      corner            center             center          corner      *
 *     frequency        frequency          frequency       frequency     *
 *    50Hz..800Hz     200Hz..3000Hz      1000Hz..8000Hz  4000Hz..16000Hz *
 *                                                                       *
 *          |               |                  |               |         *
 *          |               |                  |               |         *
 *   B -----+            /--+--\            /--+--\            +-----    *
 *   O      |\          |   |   |          |   |   |          /|         *
 *   O      | \        -    |    -        -    |    -        / |         *
 *   S +    |  \      |     |     |      |     |     |      /  |         *
 *   T      |   |    |      |      |    |      |      |    |   |         *
 * ---------+---------------+------------------+---------------+-------- *
 *   C      |   |    |      |      |    |      |      |    |   |         *
 *   U -    |  /      |     |     |      |     |     |      \  |         *
 *   T      | /        -    |    -        -    |    -        \ |         *
 *   O      |/          |   |   |          |   |   |          \|         *
 *   F -----+            \--+--/            \--+--/            +-----    *
 *   F      |               |                  |               |         *
 *          |               |                  |               |         *
 *                                                                       *
 * Gains vary from 0.126 up to 7.943, which means from -18dB attenuation *
 * up to +18dB amplification. Band width varies from 0.01 up to 1.0 in   *
 * octaves for two mid bands.                                            *
 *                                                                       *
 * Implementation is based on the "Cookbook formulae for audio EQ biquad *
 * filter coefficients" by Robert Bristow-Johnson                        *
 * http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt                   */


/* The maximum number of sample frames per update. */
constexpr auto MAX_UPDATE_SAMPLES = 256;


class EqualizerEffect :
    public IEffect
{
public:
    EqualizerEffect()
        :
        IEffect{},
        gains{},
        filter{},
        sample_buffer{}
    {
    }

    virtual ~EqualizerEffect()
    {
    }


    // Effect gains for each channel
    ALfloat gains[MAX_EFFECT_CHANNELS][MAX_OUTPUT_CHANNELS];

    // Effect parameters
    ALfilterState filter[4][MAX_EFFECT_CHANNELS];

    ALfloat sample_buffer[4][MAX_EFFECT_CHANNELS][MAX_UPDATE_SAMPLES];


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
}; // EqualizerEffect


void EqualizerEffect::do_construct()
{
    // Initialize sample history only on filter creation to avoid
    // sound clicks if filter settings were changed in runtime.
    for (int it = 0; it < 4; ++it)
    {
        for (int ft = 0; ft < MAX_EFFECT_CHANNELS; ++ft)
        {
            ALfilterState_clear(&filter[it][ft]);
        }
    }
}

void EqualizerEffect::do_destruct()
{
}

ALboolean EqualizerEffect::do_update_device(
    ALCdevice* device)
{
    static_cast<void>(device);
    return AL_TRUE;
}

void EqualizerEffect::do_update(
    const ALCdevice* device,
    const struct ALeffectslot* slot,
    const union ALeffectProps *props)
{
    ALfloat frequency = (ALfloat)device->frequency;
    ALfloat gain, freq_mult;
    ALuint i;

    out_buffer = device->foa_out.buffer;
    out_channels = device->foa_out.num_channels;
    for (i = 0; i < MAX_EFFECT_CHANNELS; i++)
        ComputeFirstOrderGains(device->foa_out, IdentityMatrixf.m[i],
            1.0F, gains[i]);

    /* Calculate coefficients for the each type of filter. Note that the shelf
    * filters' gain is for the reference frequency, which is the centerpoint
    * of the transition band.
    */
    gain = maxf(sqrtf(props->equalizer.low_gain), 0.0625f); /* Limit -24dB */
    freq_mult = props->equalizer.low_cutoff / frequency;
    ALfilterState_setParams(&filter[0][0], ALfilterType_LowShelf,
        gain, freq_mult, calc_rcpQ_from_slope(gain, 0.75f)
    );
    /* Copy the filter coefficients for the other input channels. */
    for (i = 1; i < MAX_EFFECT_CHANNELS; i++)
        ALfilterState_copyParams(&filter[0][i], &filter[0][0]);

    gain = maxf(props->equalizer.mid1_gain, 0.0625f);
    freq_mult = props->equalizer.mid1_center / frequency;
    ALfilterState_setParams(&filter[1][0], ALfilterType_Peaking,
        gain, freq_mult, calc_rcpQ_from_bandwidth(
            freq_mult, props->equalizer.mid1_width
        )
    );
    for (i = 1; i < MAX_EFFECT_CHANNELS; i++)
        ALfilterState_copyParams(&filter[1][i], &filter[1][0]);

    gain = maxf(props->equalizer.mid2_gain, 0.0625f);
    freq_mult = props->equalizer.mid2_center / frequency;
    ALfilterState_setParams(&filter[2][0], ALfilterType_Peaking,
        gain, freq_mult, calc_rcpQ_from_bandwidth(
            freq_mult, props->equalizer.mid2_width
        )
    );
    for (i = 1; i < MAX_EFFECT_CHANNELS; i++)
        ALfilterState_copyParams(&filter[2][i], &filter[2][0]);

    gain = maxf(sqrtf(props->equalizer.high_gain), 0.0625f);
    freq_mult = props->equalizer.high_cutoff / frequency;
    ALfilterState_setParams(&filter[3][0], ALfilterType_HighShelf,
        gain, freq_mult, calc_rcpQ_from_slope(gain, 0.75f)
    );
    for (i = 1; i < MAX_EFFECT_CHANNELS; i++)
        ALfilterState_copyParams(&filter[3][i], &filter[3][0]);
}

void EqualizerEffect::do_process(
    ALsizei sample_count,
    const ALfloat(*src_samples)[BUFFERSIZE],
    ALfloat(*dst_samples)[BUFFERSIZE],
    ALsizei channel_count)
{
    const auto samples = sample_buffer;
    ALsizei it, kt, ft;
    ALsizei base;

    for (base = 0; base < sample_count;)
    {
        ALsizei td = mini(MAX_UPDATE_SAMPLES, sample_count - base);

        for (ft = 0; ft < MAX_EFFECT_CHANNELS; ft++)
            ALfilterState_processC(&filter[0][ft], samples[0][ft], &src_samples[ft][base], td);
        for (ft = 0; ft < MAX_EFFECT_CHANNELS; ft++)
            ALfilterState_processC(&filter[1][ft], samples[1][ft], samples[0][ft], td);
        for (ft = 0; ft < MAX_EFFECT_CHANNELS; ft++)
            ALfilterState_processC(&filter[2][ft], samples[2][ft], samples[1][ft], td);
        for (ft = 0; ft < MAX_EFFECT_CHANNELS; ft++)
            ALfilterState_processC(&filter[3][ft], samples[3][ft], samples[2][ft], td);

        for (ft = 0; ft < MAX_EFFECT_CHANNELS; ft++)
        {
            for (kt = 0; kt < channel_count; kt++)
            {
                ALfloat gain = gains[ft][kt];
                if (!(fabsf(gain) > GAIN_SILENCE_THRESHOLD))
                    continue;

                for (it = 0; it < td; it++)
                    dst_samples[kt][base + it] += gain * samples[3][ft][it];
            }
        }

        base += td;
    }
}

IEffect* create_equalizer_effect()
{
    return create_effect<EqualizerEffect>();
}
