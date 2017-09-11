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


#include <algorithm>
#include <vector>
#include "config.h"
#include "alu.h"


class EchoEffectState :
    public EffectState
{
public:
    struct Tap
    {
        int delay;
    };


    EchoEffectState()
        :
        EffectState{},
        sample_buffer_{},
        buffer_length_{},
        taps_{},
        offset_{},
        gains_{},
        feed_gain_{},
        filter_{}
    {
    }

    virtual ~EchoEffectState()
    {
    }


protected:
    void EchoEffectState::do_construct() final
    {
        buffer_length_ = 0;
        sample_buffer_ = EffectSampleBuffer{};

        taps_[0].delay = 0;
        taps_[1].delay = 0;
        offset_ = 0;

        al_filter_state_clear(&filter_);
    }

    void EchoEffectState::do_destruct() final
    {
        sample_buffer_ = EffectSampleBuffer{};
    }

    void EchoEffectState::do_update_device(
        ALCdevice* device) final
    {
        // Use the next power of 2 for the buffer length, so the tap offsets can be
        // wrapped using a mask instead of a modulo
        auto maxlen = static_cast<int>(AL_ECHO_MAX_DELAY * device->frequency) + 1;
        maxlen += static_cast<int>(AL_ECHO_MAX_LRDELAY * device->frequency) + 1;
        maxlen = next_power_of_2(maxlen);

        if (maxlen != buffer_length_)
        {
            sample_buffer_.resize(maxlen);
            buffer_length_ = maxlen;
        }

        std::fill(sample_buffer_.begin(), sample_buffer_.end(), 0.0F);
    }

    void EchoEffectState::do_update(
        ALCdevice* device,
        const EffectSlot* slot,
        const EffectProps* props) final
    {
        float coeffs[max_ambi_coeffs];
        float effect_gain, lrpan, spread;

        const auto frequency = device->frequency;

        taps_[0].delay = static_cast<int>(props->echo.delay * frequency) + 1;
        taps_[1].delay = static_cast<int>(props->echo.lr_delay * frequency);
        taps_[1].delay += taps_[0].delay;

        spread = props->echo.spread;

        if (spread < 0.0F)
        {
            lrpan = -1.0F;
        }
        else
        {
            lrpan = 1.0F;
        }

        // Convert echo spread (where 0 = omni, +/-1 = directional) to coverage
        // spread (where 0 = point, tau = omni).
        spread = std::asin(1.0F - std::abs(spread)) * 4.0F;

        feed_gain_ = props->echo.feedback;

        effect_gain = std::max(1.0F - props->echo.damping, 0.0625F); // Limit -24dB

        al_filter_state_set_params(
            &filter_,
            FilterType::high_shelf,
            effect_gain,
            lp_frequency_reference / frequency,
            calc_rcp_q_from_slope(effect_gain, 1.0F));

        effect_gain = 1.0F;

        // First tap panning
        calc_angle_coeffs(-pi_2 * lrpan, 0.0F, spread, coeffs);
        compute_panning_gains(device, coeffs, effect_gain, gains_[0].data());

        // Second tap panning
        calc_angle_coeffs(pi_2 * lrpan, 0.0F, spread, coeffs);
        compute_panning_gains(device, coeffs, effect_gain, gains_[1].data());
    }

    void EchoEffectState::do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final
    {
        const auto mask = buffer_length_ - 1;
        const auto tap1 = taps_[0].delay;
        const auto tap2 = taps_[1].delay;
        float x[2] = {filter_.x[0], filter_.x[1],};
        float y[2] = {filter_.y[0], filter_.y[1],};

        for (int base = 0; base < sample_count; )
        {
            float temps[128][2];

            const auto td = std::min(128, sample_count - base);

            for (int i = 0; i < td; ++i)
            {
                // First tap
                temps[i][0] = sample_buffer_[(offset_ - tap1) & mask];

                // Second tap
                temps[i][1] = sample_buffer_[(offset_ - tap2) & mask];

                // Apply damping and feedback gain to the second tap, and mix in the
                // new sample
                auto in = temps[i][1] + src_samples[0][i + base];

                auto out = (in * filter_.b0) +
                    (x[0] * filter_.b1) + (x[1] * filter_.b2) -
                    (y[0] * filter_.a1) - (y[1] * filter_.a2);

                x[1] = x[0];
                x[0] = in;

                y[1] = y[0];
                y[0] = out;

                sample_buffer_[offset_&mask] = out * feed_gain_;

                offset_ += 1;
            }

            for (int k = 0; k < channel_count; ++k)
            {
                auto channel_gain = gains_[0][k];

                if (std::abs(channel_gain) > silence_threshold_gain)
                {
                    for (int i = 0; i < td; ++i)
                    {
                        dst_samples[k][i + base] += temps[i][0] * channel_gain;
                    }
                }

                channel_gain = gains_[1][k];

                if (std::abs(channel_gain) > silence_threshold_gain)
                {
                    for (int i = 0; i < td; ++i)
                    {
                        dst_samples[k][i + base] += temps[i][1] * channel_gain;
                    }
                }
            }

            base += td;
        }

        filter_.x[0] = x[0];
        filter_.x[1] = x[1];
        filter_.y[0] = y[0];
        filter_.y[1] = y[1];
    }


private:
    using Taps = std::array<Tap, 2>;
    using Gains = MdArray<float, 2, max_output_channels>;


    EffectSampleBuffer sample_buffer_;
    int buffer_length_;

    // The echo is two tap. The delay is the number of samples from before the
    // current offset
    Taps taps_;

    int offset_;

    // The panning gains for the two taps
    Gains gains_;

    float feed_gain_;

    FilterState filter_;
}; // EchoEffectState


EffectState* EffectStateFactory::create_echo()
{
    return create<EchoEffectState>();
}
