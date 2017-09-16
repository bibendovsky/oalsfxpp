/**
 * OpenAL cross platform audio library
 * Copyright (C) 2013 by Anis A. Hireche
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
#include "alMain.h"


class CompressorEffectState :
    public EffectState
{
public:
    CompressorEffectState()
        :
        EffectState{},
        channels_gains_{},
        is_enabled_{},
        attack_rate_{},
        release_rate_{},
        gain_control_{}
    {
    }

    virtual ~CompressorEffectState()
    {
    }


protected:
    void CompressorEffectState::do_construct() final
    {
        is_enabled_ = true;
        attack_rate_ = 0.0F;
        release_rate_ = 0.0F;
        gain_control_ = 1.0F;
    }

    void CompressorEffectState::do_destruct() final
    {
    }

    void CompressorEffectState::do_update_device(
        ALCdevice& device) final
    {
        const auto attackTime = device.frequency_ * 0.2F; // 200ms Attack
        const auto releaseTime = device.frequency_ * 0.4F; // 400ms Release

        attack_rate_ = 1.0F / attackTime;
        release_rate_ = 1.0F / releaseTime;
    }

    void CompressorEffectState::do_update(
        ALCdevice& device,
        const EffectSlot& effect_slot,
        const EffectProps& effect_props) final
    {
        is_enabled_ = effect_props.compressor_.on_off_;

        dst_buffers_ = &device.sample_buffers_;
        dst_channel_count_ = device.channel_count_;

        for (int i = 0; i < 4; ++i)
        {
            Panning::compute_first_order_gains(
                device.channel_count_,
                device.foa_,
                mat4f_identity.m_[i],
                1.0F,
                channels_gains_[i]);
        }
    }

    void CompressorEffectState::do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final
    {
        for (int base = 0; base < sample_count; )
        {
            float temps[64][4];

            const auto td = std::min(64, sample_count - base);

            // Load samples into the temp buffer first.
            for (int j = 0; j < 4; ++j)
            {
                for (int i = 0; i < td; ++i)
                {
                    temps[i][j] = src_samples[j][i + base];
                }
            }

            if (is_enabled_)
            {
                for (int i = 0; i < td; ++i)
                {
                    // Roughly calculate the maximum amplitude from the 4-channel
                    // signal, and attack or release the gain control to reach it.
                    auto amplitude = std::abs(temps[i][0]);

                    amplitude = std::max(amplitude + std::abs(temps[i][1]),
                        std::max(amplitude + std::abs(temps[i][2]),
                            amplitude + std::abs(temps[i][3])));

                    if (amplitude > gain_control_)
                    {
                        gain_control_ = std::min(gain_control_ + attack_rate_, amplitude);
                    }
                    else if (amplitude < gain_control_)
                    {
                        gain_control_ = std::max(gain_control_ - release_rate_, amplitude);
                    }

                    // Apply the inverse of the gain control to normalize/compress
                    // the volume.
                    const auto output = 1.0F / Math::clamp(gain_control_, 0.5F, 2.0F);

                    for (int j = 0; j < 4; ++j)
                    {
                        temps[i][j] *= output;
                    }
                }
            }
            else
            {
                for (int i = 0; i < td; ++i)
                {
                    // Same as above, except the amplitude is forced to 1. This
                    // helps ensure smooth gain changes when the compressor is
                    // turned on and off.

                    const auto amplitude = 1.0F;

                    if (amplitude > gain_control_)
                    {
                        gain_control_ = std::min(gain_control_ + attack_rate_, amplitude);
                    }
                    else if (amplitude < gain_control_)
                    {
                        gain_control_ = std::max(gain_control_ - release_rate_, amplitude);
                    }

                    const auto output = 1.0F / Math::clamp(gain_control_, 0.5F, 2.0F);

                    for (int j = 0; j < 4; ++j)
                    {
                        temps[i][j] *= output;
                    }
                }
            }

            // Now mix to the output.
            for (int j = 0; j < 4; ++j)
            {
                for (int k = 0; k < channel_count; ++k)
                {
                    const auto channel_gain = channels_gains_[j][k];

                    if (!(std::abs(channel_gain) > silence_threshold_gain))
                    {
                        continue;
                    }

                    for (int i = 0; i < td; ++i)
                    {
                        dst_samples[k][base + i] += channel_gain * temps[i][j];
                    }
                }
            }

            base += td;
        }
    }


private:
    using ChannelsGains = std::array<Gains, max_effect_channels>;


    // Effect gains for each channel
    ChannelsGains channels_gains_;

    // Effect parameters
    bool is_enabled_;
    float attack_rate_;
    float release_rate_;
    float gain_control_;
}; // CompressorEffectState


EffectState* EffectStateFactory::create_compressor()
{
    return create<CompressorEffectState>();
}
