/**
 * OpenAL cross platform audio library
 * Copyright (C) 2011 by Chris Robinson.
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


#include <array>
#include "config.h"
#include "alFilter.h"
#include "alu.h"


class DedicatedEffectState :
    public EffectState
{
public:
    DedicatedEffectState()
        :
        EffectState{},
        gains_{}
    {
    }

    virtual ~DedicatedEffectState()
    {
    }


protected:
    void DedicatedEffectState::do_construct() final
    {
        gains_.fill(0.0F);
    }

    void DedicatedEffectState::do_destruct() final
    {
    }

    void DedicatedEffectState::do_update_device(
        ALCdevice* device) final
    {
        static_cast<void>(device);
    }

    void DedicatedEffectState::do_update(
        ALCdevice* device,
        const EffectSlot* slot,
        const EffectProps* props)
    {
        gains_.fill(0.0F);

        const auto gain = props->dedicated.gain;

        if (slot->effect_.type_ == EffectType::dedicated_low_frequency)
        {
            const auto idx = get_channel_index(device->channel_names, LFE);

            if (idx != -1)
            {
                out_buffer = &device->sample_buffers;
                out_channels = device->num_channels;
                gains_[idx] = gain;
            }
        }
        else if (slot->effect_.type_ == EffectType::dedicated_dialog)
        {
            const auto idx = get_channel_index(device->channel_names, FrontCenter);

            // Dialog goes to the front-center speaker if it exists, otherwise it
            // plays from the front-center location.

            if (idx != -1)
            {
                out_buffer = &device->sample_buffers;
                out_channels = device->num_channels;
                gains_[idx] = gain;
            }
            else
            {
                float coeffs[max_ambi_coeffs];

                calc_angle_coeffs(0.0F, 0.0F, 0.0F, coeffs);

                out_buffer = &device->sample_buffers;
                out_channels = device->num_channels;

                compute_panning_gains(device, coeffs, gain, gains_.data());
            }
        }
    }

    void DedicatedEffectState::do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final
    {
        for (int c = 0; c < channel_count; ++c)
        {
            const auto gain = gains_[c];

            if (!(std::abs(gain) > silence_threshold_gain))
            {
                continue;
            }

            for (int i = 0; i < sample_count; ++i)
            {
                dst_samples[c][i] += src_samples[0][i] * gain;
            }
        }
    }


private:
    using Gains = std::array<float, max_output_channels>;

    Gains gains_;
}; // DedicatedEffectState


EffectState* EffectStateFactory::create_dedicated()
{
    return create<DedicatedEffectState>();
}
