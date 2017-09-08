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


class DedicatedEffect :
    public IEffect
{
public:
    DedicatedEffect()
        :
        IEffect{},
        gains_{}
    {
    }

    virtual ~DedicatedEffect()
    {
    }


protected:
    void DedicatedEffect::do_construct() final
    {
        gains_.fill(0.0F);
    }

    void DedicatedEffect::do_destruct() final
    {
    }

    ALboolean DedicatedEffect::do_update_device(
        ALCdevice* device) final
    {
        static_cast<void>(device);
        return AL_TRUE;
    }

    void DedicatedEffect::do_update(
        ALCdevice* device,
        const struct ALeffectslot* slot,
        const union ALeffectProps* props)
    {
        gains_.fill(0.0F);

        const auto gain = props->dedicated.gain;

        if (slot->params.effect_type == AL_EFFECT_DEDICATED_LOW_FREQUENCY_EFFECT)
        {
            const auto idx = GetChannelIdxByName(device->real_out, LFE);

            if (idx != -1)
            {
                out_buffer = device->real_out.buffer;
                out_channels = device->real_out.num_channels;
                gains_[idx] = gain;
            }
        }
        else if (slot->params.effect_type == AL_EFFECT_DEDICATED_DIALOGUE)
        {
            const auto idx = GetChannelIdxByName(device->real_out, FrontCenter);

            // Dialog goes to the front-center speaker if it exists, otherwise it
            // plays from the front-center location.

            if (idx != -1)
            {
                out_buffer = device->real_out.buffer;
                out_channels = device->real_out.num_channels;
                gains_[idx] = gain;
            }
            else
            {
                float coeffs[MAX_AMBI_COEFFS];

                CalcAngleCoeffs(0.0F, 0.0F, 0.0F, coeffs);

                out_buffer = &device->dry.buffer;
                out_channels = device->dry.num_channels;

                ComputePanningGains(device->dry, coeffs, gain, gains_.data());
            }
        }
    }

    void DedicatedEffect::do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final
    {
        for (int c = 0; c < channel_count; ++c)
        {
            const auto gain = gains_[c];

            if (!(std::abs(gain) > GAIN_SILENCE_THRESHOLD))
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
    using Gains = std::array<float, MAX_OUTPUT_CHANNELS>;

    Gains gains_;
}; // DedicatedEffect


IEffect* create_dedicated_effect()
{
    return create_effect<DedicatedEffect>();
}
