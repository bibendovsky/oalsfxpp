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


class ChorusEffect :
    public IEffect
{
public:
    ChorusEffect()
        :
        IEffect{},
        sample_buffers_{},
        buffer_length_{},
        offset_{},
        lfo_range_{},
        lfo_scale_{},
        lfo_disp_{},
        gain_{},
        waveform_{},
        delay_{},
        depth_{},
        feedback_{}
    {
    }

    virtual ~ChorusEffect()
    {
    }


protected:
    void ChorusEffect::do_construct() final
    {
        buffer_length_ = 0;

        for (auto& buffer : sample_buffers_)
        {
            buffer = ChorusSampleBuffer{};
        }

        offset_ = 0;
        lfo_range_ = 1;
        waveform_ = CWF_Triangle;
    }

    void ChorusEffect::do_destruct() final
    {
        for (auto& buffer : sample_buffers_)
        {
            buffer = ChorusSampleBuffer{};
        }
    }

    ALboolean ChorusEffect::do_update_device(
        ALCdevice* device) final
    {
        ALsizei maxlen;

        maxlen = fastf2i(AL_CHORUS_MAX_DELAY * 2.0f * device->frequency) + 1;
        maxlen = NextPowerOf2(maxlen);

        if (maxlen != buffer_length_)
        {
            sample_buffers_[0].resize(maxlen);
            sample_buffers_[1].resize(maxlen);

            buffer_length_ = maxlen;
        }

        for (auto& buffer : sample_buffers_)
        {
            std::fill(buffer.begin(), buffer.end(), 0.0F);
        }

        return AL_TRUE;
    }

    void ChorusEffect::do_update(
        ALCdevice* device,
        const struct ALeffectslot* slot,
        const union ALeffectProps *props) final
    {
        ALfloat frequency = (ALfloat)device->frequency;
        ALfloat coeffs[MAX_AMBI_COEFFS];
        ALfloat rate;
        ALint phase;

        switch (props->chorus.waveform)
        {
        case AL_CHORUS_WAVEFORM_TRIANGLE:
            waveform_ = CWF_Triangle;
            break;
        case AL_CHORUS_WAVEFORM_SINUSOID:
            waveform_ = CWF_Sinusoid;
            break;
        }
        feedback_ = props->chorus.feedback;
        delay_ = fastf2i(props->chorus.delay * frequency);
        /* The LFO depth is scaled to be relative to the sample delay. */
        depth_ = props->chorus.depth * delay_;

        /* Gains for left and right sides */
        CalcAngleCoeffs(-F_PI_2, 0.0f, 0.0f, coeffs);
        ComputePanningGains(device->dry, coeffs, 1.0F, gain_[0]);
        CalcAngleCoeffs(F_PI_2, 0.0f, 0.0f, coeffs);
        ComputePanningGains(device->dry, coeffs, 1.0F, gain_[1]);

        phase = props->chorus.phase;
        rate = props->chorus.rate;
        if (!(rate > 0.0f))
        {
            lfo_scale_ = 0.0f;
            lfo_range_ = 1;
            lfo_disp_ = 0;
        }
        else
        {
            /* Calculate LFO coefficient */
            lfo_range_ = fastf2i(frequency / rate + 0.5f);
            switch (waveform_)
            {
            case CWF_Triangle:
                lfo_scale_ = 4.0f / lfo_range_;
                break;
            case CWF_Sinusoid:
                lfo_scale_ = F_TAU / lfo_range_;
                break;
            }

            /* Calculate lfo phase displacement */
            if (phase >= 0)
                lfo_disp_ = fastf2i(lfo_range_ * (phase / 360.0f));
            else
                lfo_disp_ = fastf2i(lfo_range_ * ((360 + phase) / 360.0f));
        }
    }

    void ChorusEffect::do_process(
        const ALsizei sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const ALsizei channel_count) final
    {
        auto& leftbuf = sample_buffers_[0];
        auto& rightbuf = sample_buffers_[1];
        const ALsizei bufmask = buffer_length_ - 1;
        ALsizei i, c;
        ALsizei base;

        for (base = 0; base < sample_count;)
        {
            const ALsizei todo = mini(128, sample_count - base);
            ALfloat temps[128][2];
            ALint moddelays[2][128];

            switch (waveform_)
            {
            case CWF_Triangle:
                GetTriangleDelays(moddelays[0], offset_%lfo_range_, lfo_range_,
                    lfo_scale_, depth_, delay_, todo);
                GetTriangleDelays(moddelays[1], (offset_ + lfo_disp_) % lfo_range_,
                    lfo_range_, lfo_scale_, depth_, delay_,
                    todo);
                break;
            case CWF_Sinusoid:
                GetSinusoidDelays(moddelays[0], offset_%lfo_range_, lfo_range_,
                    lfo_scale_, depth_, delay_, todo);
                GetSinusoidDelays(moddelays[1], (offset_ + lfo_disp_) % lfo_range_,
                    lfo_range_, lfo_scale_, depth_, delay_,
                    todo);
                break;
            }

            for (i = 0; i < todo; i++)
            {
                leftbuf[offset_&bufmask] = src_samples[0][base + i];
                temps[i][0] = leftbuf[(offset_ - moddelays[0][i])&bufmask] * feedback_;
                leftbuf[offset_&bufmask] += temps[i][0];

                rightbuf[offset_&bufmask] = src_samples[0][base + i];
                temps[i][1] = rightbuf[(offset_ - moddelays[1][i])&bufmask] * feedback_;
                rightbuf[offset_&bufmask] += temps[i][1];

                offset_++;
            }

            for (c = 0; c < channel_count; c++)
            {
                ALfloat channel_gain = gain_[0][c];
                if (fabsf(channel_gain) > GAIN_SILENCE_THRESHOLD)
                {
                    for (i = 0; i < todo; i++)
                        dst_samples[c][i + base] += temps[i][0] * channel_gain;
                }

                channel_gain = gain_[1][c];
                if (fabsf(channel_gain) > GAIN_SILENCE_THRESHOLD)
                {
                    for (i = 0; i < todo; i++)
                        dst_samples[c][i + base] += temps[i][1] * channel_gain;
                }
            }

            base += todo;
        }
    }


private:
    enum ChorusWaveForm {
        CWF_Triangle = AL_CHORUS_WAVEFORM_TRIANGLE,
        CWF_Sinusoid = AL_CHORUS_WAVEFORM_SINUSOID
    };


    using ChorusSampleBuffer = EffectSampleBuffer;
    using ChorusSampleBuffers = std::array<ChorusSampleBuffer, 2>;


    ChorusSampleBuffers sample_buffers_;
    ALsizei buffer_length_;
    ALsizei offset_;
    ALsizei lfo_range_;
    ALfloat lfo_scale_;
    ALint lfo_disp_;

    // Gains for left and right sides
    ALfloat gain_[2][MAX_OUTPUT_CHANNELS];

    // effect parameters
    ChorusWaveForm waveform_;
    ALint delay_;
    ALfloat depth_;
    ALfloat feedback_;


    static void GetTriangleDelays(
        ALint *delays,
        ALsizei offset,
        const ALsizei lfo_range,
        const ALfloat lfo_scale,
        const ALfloat depth,
        const ALsizei delay,
        const ALsizei todo)
    {
        ALsizei i;
        for (i = 0; i < todo; i++)
        {
            delays[i] = fastf2i((1.0f - fabsf(2.0f - lfo_scale*offset)) * depth) + delay;
            offset = (offset + 1) % lfo_range;
        }
    }

    static void GetSinusoidDelays(
        ALint *delays,
        ALsizei offset,
        const ALsizei lfo_range,
        const ALfloat lfo_scale,
        const ALfloat depth,
        const ALsizei delay,
        const ALsizei todo)
    {
        ALsizei i;
        for (i = 0; i < todo; i++)
        {
            delays[i] = fastf2i(sinf(lfo_scale*offset) * depth) + delay;
            offset = (offset + 1) % lfo_range;
        }
    }
}; // ChorusEffect


IEffect* create_chorus_effect()
{
    return create_effect<ChorusEffect>();
}
