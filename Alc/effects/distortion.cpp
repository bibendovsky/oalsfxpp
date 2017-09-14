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
#include "alAuxEffectSlot.h"
#include "alFilter.h"


class DistortionEffectState :
    public EffectState
{
public:
    DistortionEffectState()
        :
        EffectState{},
        gains_{},
        low_pass_{},
        band_pass_{},
        attenuation_{},
        edge_coeff_{}
    {
    }

    virtual ~DistortionEffectState()
    {
    }


protected:
    void DistortionEffectState::do_construct() final
    {
        low_pass_.clear();
        band_pass_.clear();
    }

    void DistortionEffectState::do_destruct() final
    {
    }

    void DistortionEffectState::do_update_device(
        ALCdevice* device) final
    {
        static_cast<void>(device);
    }

    void DistortionEffectState::do_update(
        ALCdevice* device,
        const EffectSlot* slot,
        const EffectProps* props) final
    {
        const auto frequency = static_cast<float>(device->frequency_);

        // Store distorted signal attenuation settings.
        attenuation_ = props->distortion_.gain_;

        // Store waveshaper edge settings.
        auto edge = std::sin(props->distortion_.edge_ * (Math::pi_2));
        edge = std::min(edge, 0.99F);
        edge_coeff_ = 2.0F * edge / (1.0F - edge);

        auto cutoff = props->distortion_.low_pass_cutoff_;

        // Bandwidth value is constant in octaves.
        auto bandwidth = (cutoff / 2.0F) / (cutoff * 0.67F);

        // Multiply sampling frequency by the amount of oversampling done during
        // processing.
        low_pass_.set_params(
            FilterType::low_pass,
            1.0F,
            cutoff / (frequency * 4.0F),
            FilterState::calc_rcp_q_from_bandwidth(cutoff / (frequency * 4.0F), bandwidth));

        cutoff = props->distortion_.eq_center_;

        // Convert bandwidth in Hz to octaves.
        bandwidth = props->distortion_.eq_bandwidth_ / (cutoff * 0.67F);

        band_pass_.set_params(
            FilterType::band_pass,
            1.0F,
            cutoff / (frequency * 4.0F),
            FilterState::calc_rcp_q_from_bandwidth(cutoff / (frequency * 4.0F), bandwidth));

        Panning::compute_ambient_gains(device->channel_count_, device->dry_, 1.0F, gains_.data());
    }

    void DistortionEffectState::do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final
    {
        const auto fc = edge_coeff_;

        for (int base = 0; base < sample_count; )
        {
            float buffer[2][64 * 4];

            const auto td = std::min(64, sample_count - base);

            // Perform 4x oversampling to avoid aliasing. Oversampling greatly
            // improves distortion quality and allows to implement lowpass and
            // bandpass filters using high frequencies, at which classic IIR
            // filters became unstable.

            // Fill oversample buffer using zero stuffing.
            for (int it = 0; it < td; ++it)
            {
                // Multiply the sample by the amount of oversampling to maintain
                // the signal's power.

                buffer[0][(it * 4) + 0] = src_samples[0][it + base] * 4.0F;
                buffer[0][(it * 4) + 1] = 0.0F;
                buffer[0][(it * 4) + 2] = 0.0F;
                buffer[0][(it * 4) + 3] = 0.0F;
            }

            // First step, do lowpass filtering of original signal. Additionally
            // perform buffer interpolation and lowpass cutoff for oversampling
            // (which is fortunately first step of distortion). So combine three
            // operations into the one.
            low_pass_.process(td * 4, buffer[0], buffer[1]);

            // Second step, do distortion using waveshaper function to emulate
            // signal processing during tube overdriving. Three steps of
            // waveshaping are intended to modify waveform without boost/clipping/
            // attenuation process.
            for (int it = 0; it < td * 4; it++)
            {
                auto smp = buffer[1][it];

                smp = (1.0F + fc) * smp / (1.0F + (fc * std::abs(smp)));
                smp = (1.0F + fc) * smp / (1.0F + (fc * std::abs(smp))) * -1.0F;
                smp = (1.0F + fc) * smp / (1.0F + (fc * std::abs(smp)));

                buffer[0][it] = smp;
            }

            // Third step, do bandpass filtering of distorted signal.
            band_pass_.process(td * 4, buffer[0], buffer[1]);

            for (int kt = 0; kt < channel_count; ++kt)
            {
                // Fourth step, final, do attenuation and perform decimation,
                // store only one sample out of 4.

                const auto gain = gains_[kt] * attenuation_;

                if (!(std::abs(gain) > silence_threshold_gain))
                {
                    continue;
                }

                for (int it = 0; it < td; ++it)
                {
                    dst_samples[kt][base + it] += gain * buffer[1][it * 4];
                }
            }

            base += td;
        }
    }


private:
    using Gains = std::array<float, max_channels>;


    // Effect gains for each channel
    Gains gains_;

    // Effect parameters
    FilterState low_pass_;
    FilterState band_pass_;
    float attenuation_;
    float edge_coeff_;
}; // DistortionEffectState


EffectState* EffectStateFactory::create_distortion()
{
    return create<DistortionEffectState>();
}
