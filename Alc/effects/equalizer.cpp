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


class EqualizerEffect :
    public IEffect
{
public:
    EqualizerEffect()
        :
        IEffect{},
        gains_{},
        filter_{},
        sample_buffer_{}
    {
    }

    virtual ~EqualizerEffect()
    {
    }


protected:
    void EqualizerEffect::do_construct()
    {
        // Initialize sample history only on filter creation to avoid
        // sound clicks if filter settings were changed in runtime.
        for (int it = 0; it < 4; ++it)
        {
            for (int ft = 0; ft < max_effect_channels; ++ft)
            {
                al_filter_state_clear(&filter_[it][ft]);
            }
        }
    }

    void EqualizerEffect::do_destruct()
    {
    }

    void EqualizerEffect::do_update_device(
        ALCdevice* device)
    {
        static_cast<void>(device);
    }

    void EqualizerEffect::do_update(
        ALCdevice* device,
        const struct ALeffectslot* slot,
        const union ALeffectProps* props)
    {
        const auto frequency = static_cast<float>(device->frequency);
        float gain;
        float freq_mult;

        out_buffer = device->foa_out.buffers;
        out_channels = device->foa_out.num_channels;

        for (int i = 0; i < max_effect_channels; ++i)
        {
            compute_first_order_gains(device->foa_out, identity_matrix_f.m[i], 1.0F, gains_[i].data());
        }

        // Calculate coefficients for the each type of filter. Note that the shelf
        // filters' gain is for the reference frequency, which is the centerpoint
        // of the transition band.
        gain = std::max(std::sqrt(props->equalizer.low_gain), 0.0625F); // Limit -24dB
        freq_mult = props->equalizer.low_cutoff / frequency;

        al_filter_state_set_params(
            &filter_[0][0],
            FilterType::low_shelf,
            gain,
            freq_mult,
            calc_rcp_q_from_slope(gain, 0.75F));

        // Copy the filter coefficients for the other input channels.
        for (int i = 1; i < max_effect_channels; ++i)
        {
            al_filter_state_copy_params(&filter_[0][i], &filter_[0][0]);
        }

        gain = std::max(props->equalizer.mid1_gain, 0.0625F);
        freq_mult = props->equalizer.mid1_center / frequency;

        al_filter_state_set_params(
            &filter_[1][0],
            FilterType::peaking,
            gain,
            freq_mult,
            calc_rcp_q_from_bandwidth(freq_mult, props->equalizer.mid1_width));

        for (int i = 1; i < max_effect_channels; ++i)
        {
            al_filter_state_copy_params(&filter_[1][i], &filter_[1][0]);
        }

        gain = std::max(props->equalizer.mid2_gain, 0.0625F);
        freq_mult = props->equalizer.mid2_center / frequency;

        al_filter_state_set_params(
            &filter_[2][0],
            FilterType::peaking,
            gain,
            freq_mult,
            calc_rcp_q_from_bandwidth(freq_mult, props->equalizer.mid2_width));

        for (int i = 1; i < max_effect_channels; ++i)
        {
            al_filter_state_copy_params(&filter_[2][i], &filter_[2][0]);
        }

        gain = std::max(std::sqrt(props->equalizer.high_gain), 0.0625F);
        freq_mult = props->equalizer.high_cutoff / frequency;

        al_filter_state_set_params(
            &filter_[3][0],
            FilterType::high_shelf,
            gain,
            freq_mult,
            calc_rcp_q_from_slope(gain, 0.75F));

        for (int i = 1; i < max_effect_channels; ++i)
        {
            al_filter_state_copy_params(&filter_[3][i], &filter_[3][0]);
        }
    }

    void EqualizerEffect::do_process(
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
                al_filter_state_process_c(&filter_[0][ft], samples[0][ft].data(), &src_samples[ft][base], td);
            }

            for (int ft = 0; ft < max_effect_channels; ++ft)
            {
                al_filter_state_process_c(&filter_[1][ft], samples[1][ft].data(), samples[0][ft].data(), td);
            }

            for (int ft = 0; ft < max_effect_channels; ++ft)
            {
                al_filter_state_process_c(&filter_[2][ft], samples[2][ft].data(), samples[1][ft].data(), td);
            }

            for (int ft = 0; ft < max_effect_channels; ++ft)
            {
                al_filter_state_process_c(&filter_[3][ft], samples[3][ft].data(), samples[2][ft].data(), td);
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

    using Gains = MdArray<float, max_effect_channels, max_output_channels>;
    using Filters = MdArray<FilterState, 4, max_effect_channels>;
    using SampleBuffers = MdArray<float, 4, max_effect_channels, max_update_samples>;


    // Effect gains for each channel
    Gains gains_;

    // Effect parameters
    Filters filter_;

    SampleBuffers sample_buffer_;
}; // EqualizerEffect


IEffect* create_equalizer_effect()
{
    return create_effect<EqualizerEffect>();
}
