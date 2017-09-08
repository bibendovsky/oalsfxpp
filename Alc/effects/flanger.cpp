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
#include "alu.h"


class FlangerEffect :
    public IEffect
{
public:
    FlangerEffect()
        :
        IEffect{},
        sample_buffers_{},
        buffer_length_{},
        offset_{},
        lfo_range_{},
        lfo_scale_{},
        lfo_disp_{},
        gains_{},
        waveform_{},
        delay_{},
        depth_{},
        feedback_{}
    {
    }

    virtual ~FlangerEffect()
    {
    }


protected:
    void FlangerEffect::do_construct() final
    {
        buffer_length_ = 0;

        for (auto& buffer : sample_buffers_)
        {
            buffer = SampleBuffer{};
        }

        offset_ = 0;
        lfo_range_ = 1;
        waveform_ = Waveform::triangle;
    }

    void FlangerEffect::do_destruct() final
    {
        for (auto& buffer : sample_buffers_)
        {
            buffer = SampleBuffer{};
        }
    }

    void FlangerEffect::do_update_device(
        ALCdevice* device) final
    {
        auto maxlen = fastf2i(AL_FLANGER_MAX_DELAY * 2.0F * device->frequency) + 1;
        maxlen = next_power_of_2(maxlen);

        if (maxlen != buffer_length_)
        {
            for (auto& buffer : sample_buffers_)
            {
                buffer.resize(maxlen);
            }

            buffer_length_ = maxlen;
        }

        for (auto& buffer : sample_buffers_)
        {
            std::fill(buffer.begin(), buffer.end(), 0.0F);
        }
    }

    void FlangerEffect::do_update(
        ALCdevice* device,
        const struct ALeffectslot* slot,
        const union ALeffectProps* props) final
    {
        const auto frequency = static_cast<float>(device->frequency);
        float coeffs[max_ambi_coeffs];

        switch (props->flanger.waveform)
        {
        case AL_FLANGER_WAVEFORM_TRIANGLE:
            waveform_ = Waveform::triangle;
            break;

        case AL_FLANGER_WAVEFORM_SINUSOID:
            waveform_ = Waveform::sinusoid;
            break;
        }

        feedback_ = props->flanger.feedback;
        delay_ = fastf2i(props->flanger.delay * frequency);

        // The LFO depth is scaled to be relative to the sample delay.
        depth_ = props->flanger.depth * delay_;

        // Gains for left and right sides
        calc_angle_coeffs(-pi_2, 0.0F, 0.0F, coeffs);
        compute_panning_gains(device->dry, coeffs, 1.0F, gains_[0].data());
        calc_angle_coeffs(pi_2, 0.0F, 0.0F, coeffs);
        compute_panning_gains(device->dry, coeffs, 1.0F, gains_[1].data());

        const auto phase = props->flanger.phase;
        const auto rate = props->flanger.rate;

        if (!(rate > 0.0F))
        {
            lfo_scale_ = 0.0F;
            lfo_range_ = 1;
            lfo_disp_ = 0;
        }
        else
        {
            // Calculate LFO coefficient
            lfo_range_ = fastf2i(frequency / rate + 0.5F);

            switch (waveform_)
            {
            case Waveform::triangle:
                lfo_scale_ = 4.0F / lfo_range_;
                break;

            case Waveform::sinusoid:
                lfo_scale_ = tau / lfo_range_;
                break;
            }

            // Calculate lfo phase displacement
            if (phase >= 0)
            {
                lfo_disp_ = fastf2i(lfo_range_ * (phase / 360.0F));
            }
            else
            {
                lfo_disp_ = fastf2i(lfo_range_ * ((360 + phase) / 360.0F));
            }
        }
    }

    void FlangerEffect::do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final
    {
        auto& left_buf = sample_buffers_[0];
        auto& right_buf = sample_buffers_[1];
        const auto buf_mask = buffer_length_ - 1;

        for (int base = 0; base < sample_count; )
        {
            float temps[128][2];
            int mod_delays[2][128];

            const auto todo = std::min(128, sample_count - base);

            switch (waveform_)
            {
            case Waveform::triangle:
                GetTriangleDelays(
                    mod_delays[0],
                    offset_ % lfo_range_,
                    lfo_range_,
                    lfo_scale_,
                    depth_,
                    delay_,
                    todo);

                GetTriangleDelays(
                    mod_delays[1],
                    (offset_ + lfo_disp_) % lfo_range_,
                    lfo_range_,
                    lfo_scale_,
                    depth_,
                    delay_,
                    todo);

                break;

            case Waveform::sinusoid:
                GetSinusoidDelays(
                    mod_delays[0],
                    offset_ % lfo_range_,
                    lfo_range_,
                    lfo_scale_,
                    depth_,
                    delay_,
                    todo);

                GetSinusoidDelays(
                    mod_delays[1],
                    (offset_ + lfo_disp_) % lfo_range_,
                    lfo_range_,
                    lfo_scale_,
                    depth_,
                    delay_,
                    todo);

                break;
            }

            for (int i = 0; i < todo; ++i)
            {
                left_buf[offset_ & buf_mask] = src_samples[0][base + i];
                temps[i][0] = left_buf[(offset_ - mod_delays[0][i]) & buf_mask] * feedback_;
                left_buf[offset_ & buf_mask] += temps[i][0];

                right_buf[offset_ & buf_mask] = src_samples[0][base + i];
                temps[i][1] = right_buf[(offset_ - mod_delays[1][i]) & buf_mask] * feedback_;
                right_buf[offset_ & buf_mask] += temps[i][1];

                offset_ += 1;
            }

            for (int c = 0; c < channel_count; ++c)
            {
                auto gain = gains_[0][c];

                if (std::abs(gain) > silence_threshold_gain)
                {
                    for (int i = 0; i < todo; i++)
                    {
                        dst_samples[c][i + base] += temps[i][0] * gain;
                    }
                }

                gain = gains_[1][c];

                if (std::abs(gain) > silence_threshold_gain)
                {
                    for (int i = 0; i < todo; ++i)
                    {
                        dst_samples[c][i + base] += temps[i][1] * gain;
                    }
                }
            }

            base += todo;
        }
    }


private:
    enum class Waveform
    {
        triangle = AL_FLANGER_WAVEFORM_TRIANGLE,
        sinusoid = AL_FLANGER_WAVEFORM_SINUSOID
    }; // Waveform

    using SampleBuffer = EffectSampleBuffer;
    using SampleBuffers = std::array<SampleBuffer, 2>;
    using Gains = MdArray<float, 2, max_output_channels>;


    SampleBuffers sample_buffers_;
    int buffer_length_;
    int offset_;
    int lfo_range_;
    float lfo_scale_;
    int lfo_disp_;

    // Gains for left and right sides
    Gains gains_;

    // effect parameters
    Waveform waveform_;
    int delay_;
    float depth_;
    float feedback_;


    static void GetTriangleDelays(
        int* delays,
        int offset,
        const int lfo_range,
        const float lfo_scale,
        const float depth,
        const int delay,
        const int todo)
    {
        for (int i = 0; i < todo; ++i)
        {
            delays[i] = fastf2i((1.0F - std::abs(2.0F - (lfo_scale * offset))) * depth) + delay;
            offset = (offset + 1) % lfo_range;
        }
    }

    static void GetSinusoidDelays(
        int* delays,
        int offset,
        const int lfo_range,
        const float lfo_scale,
        const float depth,
        const int delay,
        const int todo)
    {
        for (int i = 0; i < todo; ++i)
        {
            delays[i] = fastf2i(std::sin(lfo_scale * offset) * depth) + delay;
            offset = (offset + 1) % lfo_range;
        }
    }
}; // FlangerEffect


IEffect* create_flanger_effect()
{
    return create_effect<FlangerEffect>();
}
