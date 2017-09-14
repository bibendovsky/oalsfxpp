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


#include "alMain.h"


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


class EqualizerEffectState :
    public EffectState
{
public:
    EqualizerEffectState()
        :
        EffectState{},
        gains_{},
        filter_{},
        sample_buffer_{}
    {
    }

    virtual ~EqualizerEffectState()
    {
    }


protected:
    void EqualizerEffectState::do_construct()
    {
        // Initialize sample history only on filter creation to avoid
        // sound clicks if filter settings were changed in runtime.
        for (int it = 0; it < 4; ++it)
        {
            for (int ft = 0; ft < max_effect_channels; ++ft)
            {
                filter_[it][ft].clear();
            }
        }
    }

    void EqualizerEffectState::do_destruct()
    {
    }

    void EqualizerEffectState::do_update_device(
        ALCdevice* device)
    {
        static_cast<void>(device);
    }

    void EqualizerEffectState::do_update(
        ALCdevice* device,
        const EffectSlot* slot,
        const EffectProps* props)
    {
        const auto frequency = static_cast<float>(device->frequency_);
        float gain;
        float freq_mult;

        dst_buffers_ = &device->sample_buffers_;
        dst_channel_count_ = device->channel_count_;

        for (int i = 0; i < max_effect_channels; ++i)
        {
            Panning::compute_first_order_gains(device->channel_count_, device->foa_, mat4f_identity.m_[i], 1.0F, gains_[i].data());
        }

        // Calculate coefficients for the each type of filter. Note that the shelf
        // filters' gain is for the reference frequency, which is the centerpoint
        // of the transition band.
        gain = std::max(std::sqrt(props->equalizer_.low_gain_), 0.0625F); // Limit -24dB
        freq_mult = props->equalizer_.low_cutoff_ / frequency;

        filter_[0][0].set_params(
            FilterType::low_shelf,
            gain,
            freq_mult,
            FilterState::calc_rcp_q_from_slope(gain, 0.75F));

        // Copy the filter coefficients for the other input channels.
        for (int i = 1; i < max_effect_channels; ++i)
        {
            FilterState::copy_params(filter_[0][0], filter_[0][i]);
        }

        gain = std::max(props->equalizer_.mid1_gain_, 0.0625F);
        freq_mult = props->equalizer_.mid1_center_ / frequency;

        filter_[1][0].set_params(
            FilterType::peaking,
            gain,
            freq_mult,
            FilterState::calc_rcp_q_from_bandwidth(freq_mult, props->equalizer_.mid1_width_));

        for (int i = 1; i < max_effect_channels; ++i)
        {
            FilterState::copy_params(filter_[1][0], filter_[1][i]);
        }

        gain = std::max(props->equalizer_.mid2_gain_, 0.0625F);
        freq_mult = props->equalizer_.mid2_center_ / frequency;

        filter_[2][0].set_params(
            FilterType::peaking,
            gain,
            freq_mult,
            FilterState::calc_rcp_q_from_bandwidth(freq_mult, props->equalizer_.mid2_width_));

        for (int i = 1; i < max_effect_channels; ++i)
        {
            FilterState::copy_params(filter_[2][0], filter_[2][i]);
        }

        gain = std::max(std::sqrt(props->equalizer_.high_gain_), 0.0625F);
        freq_mult = props->equalizer_.high_cutoff_ / frequency;

        filter_[3][0].set_params(
            FilterType::high_shelf,
            gain,
            freq_mult,
            FilterState::calc_rcp_q_from_slope(gain, 0.75F));

        for (int i = 1; i < max_effect_channels; ++i)
        {
            FilterState::copy_params(filter_[3][0], filter_[3][i]);
        }
    }

    void EqualizerEffectState::do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count)
    {
        auto& samples = sample_buffer_;

        for (int base = 0; base < sample_count; )
        {
            const auto td = std::min(max_update_samples, sample_count - base);

            for (int ft = 0; ft < max_effect_channels; ++ft)
            {
                filter_[0][ft].process(td, &src_samples[ft][base], samples[0][ft].data());
            }

            for (int ft = 0; ft < max_effect_channels; ++ft)
            {
                filter_[1][ft].process(td, samples[0][ft].data(), samples[1][ft].data());
            }

            for (int ft = 0; ft < max_effect_channels; ++ft)
            {
                filter_[2][ft].process(td, samples[1][ft].data(), samples[2][ft].data());
            }

            for (int ft = 0; ft < max_effect_channels; ++ft)
            {
                filter_[3][ft].process(td, samples[2][ft].data(), samples[3][ft].data());
            }

            for (int ft = 0; ft < max_effect_channels; ++ft)
            {
                for (int kt = 0; kt < channel_count; ++kt)
                {
                    const auto gain = gains_[ft][kt];

                    if (!(std::abs(gain) > silence_threshold_gain))
                    {
                        continue;
                    }

                    for (int it = 0; it < td; ++it)
                    {
                        dst_samples[kt][base + it] += gain * samples[3][ft][it];
                    }
                }
            }

            base += td;
        }
    }


private:
    // The maximum number of sample frames per update.
    static constexpr auto max_update_samples = 256;

    using Gains = MdArray<float, max_effect_channels, max_channels>;
    using Filters = MdArray<FilterState, 4, max_effect_channels>;
    using SampleBuffers = MdArray<float, 4, max_effect_channels, max_update_samples>;


    // Effect gains for each channel
    Gains gains_;

    // Effect parameters
    Filters filter_;

    SampleBuffers sample_buffer_;
}; // EqualizerEffectState


EffectState* EffectStateFactory::create_equalizer()
{
    return create<EqualizerEffectState>();
}
