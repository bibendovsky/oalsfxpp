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


class DistortionEffect :
    public IEffect
{
public:
    DistortionEffect()
        :
        IEffect{},
        gains{},
        lowpass{},
        bandpass{},
        attenuation{},
        edge_coeff{}
    {
    }

    virtual ~DistortionEffect()
    {
    }


    // Effect gains for each channel
    ALfloat gains[MAX_OUTPUT_CHANNELS];

    // Effect parameters
    ALfilterState lowpass;
    ALfilterState bandpass;
    ALfloat attenuation;
    ALfloat edge_coeff;


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
}; // DistortionEffect


void DistortionEffect::do_construct()
{
    ALfilterState_clear(&lowpass);
    ALfilterState_clear(&bandpass);
}

void DistortionEffect::do_destruct()
{
}

ALboolean DistortionEffect::do_update_device(
    ALCdevice* device)
{
    static_cast<void>(device);
    return AL_TRUE;
}

void DistortionEffect::do_update(
    const ALCdevice* device,
    const struct ALeffectslot* slot,
    const union ALeffectProps *props)
{
    const auto frequency = static_cast<ALfloat>(device->frequency);
    ALfloat bandwidth;
    ALfloat cutoff;
    ALfloat edge;

    /* Store distorted signal attenuation settings. */
    attenuation = props->distortion.gain;

    /* Store waveshaper edge settings. */
    edge = sinf(props->distortion.edge * (F_PI_2));
    edge = minf(edge, 0.99f);
    edge_coeff = 2.0f * edge / (1.0f - edge);

    cutoff = props->distortion.lowpass_cutoff;
    /* Bandwidth value is constant in octaves. */
    bandwidth = (cutoff / 2.0f) / (cutoff * 0.67f);
    /* Multiply sampling frequency by the amount of oversampling done during
    * processing.
    */
    ALfilterState_setParams(&lowpass, ALfilterType_LowPass, 1.0f,
        cutoff / (frequency*4.0f), calc_rcpQ_from_bandwidth(cutoff / (frequency*4.0f), bandwidth)
    );

    cutoff = props->distortion.eq_center;
    /* Convert bandwidth in Hz to octaves. */
    bandwidth = props->distortion.eq_bandwidth / (cutoff * 0.67f);
    ALfilterState_setParams(&bandpass, ALfilterType_BandPass, 1.0f,
        cutoff / (frequency*4.0f), calc_rcpQ_from_bandwidth(cutoff / (frequency*4.0f), bandwidth)
    );

    ComputeAmbientGains(device->dry, 1.0F, gains);
}

void DistortionEffect::do_process(
    ALsizei sample_count,
    const ALfloat(*src_samples)[BUFFERSIZE],
    ALfloat(*dst_samples)[BUFFERSIZE],
    ALsizei channel_count)
{
    const ALfloat fc = edge_coeff;
    ALsizei it, kt;
    ALsizei base;

    for (base = 0; base < sample_count;)
    {
        float buffer[2][64 * 4];
        ALsizei td = mini(64, sample_count - base);

        /* Perform 4x oversampling to avoid aliasing. Oversampling greatly
        * improves distortion quality and allows to implement lowpass and
        * bandpass filters using high frequencies, at which classic IIR
        * filters became unstable.
        */

        /* Fill oversample buffer using zero stuffing. */
        for (it = 0; it < td; it++)
        {
            /* Multiply the sample by the amount of oversampling to maintain
            * the signal's power.
            */
            buffer[0][it * 4 + 0] = src_samples[0][it + base] * 4.0f;
            buffer[0][it * 4 + 1] = 0.0f;
            buffer[0][it * 4 + 2] = 0.0f;
            buffer[0][it * 4 + 3] = 0.0f;
        }

        /* First step, do lowpass filtering of original signal. Additionally
        * perform buffer interpolation and lowpass cutoff for oversampling
        * (which is fortunately first step of distortion). So combine three
        * operations into the one.
        */
        ALfilterState_processC(&lowpass, buffer[1], buffer[0], td * 4);

        /* Second step, do distortion using waveshaper function to emulate
        * signal processing during tube overdriving. Three steps of
        * waveshaping are intended to modify waveform without boost/clipping/
        * attenuation process.
        */
        for (it = 0; it < td * 4; it++)
        {
            ALfloat smp = buffer[1][it];

            smp = (1.0f + fc) * smp / (1.0f + fc*fabsf(smp));
            smp = (1.0f + fc) * smp / (1.0f + fc*fabsf(smp)) * -1.0f;
            smp = (1.0f + fc) * smp / (1.0f + fc*fabsf(smp));

            buffer[0][it] = smp;
        }

        /* Third step, do bandpass filtering of distorted signal. */
        ALfilterState_processC(&bandpass, buffer[1], buffer[0], td * 4);

        for (kt = 0; kt < channel_count; kt++)
        {
            /* Fourth step, final, do attenuation and perform decimation,
            * store only one sample out of 4.
            */
            ALfloat gain = gains[kt] * attenuation;
            if (!(fabsf(gain) > GAIN_SILENCE_THRESHOLD))
                continue;

            for (it = 0; it < td; it++)
                dst_samples[kt][base + it] += gain * buffer[1][it * 4];
        }

        base += td;
    }
}

IEffect* create_distortion_effect()
{
    return create_effect<DistortionEffect>();
}
