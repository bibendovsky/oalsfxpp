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


#include "alMain.h"


class ModulatorEffectState :
    public EffectState
{
public:
    ModulatorEffectState()
        :
        EffectState{},
        process_func_{},
        index_{},
        step_{},
        channels_gains_{},
        filters_{}
    {
    }

    virtual ~ModulatorEffectState()
    {
    }


protected:
    void ModulatorEffectState::do_construct() final
    {
        index_ = 0;
        step_ = 1;

        for (int i = 0; i < max_effect_channels; ++i)
        {
            filters_[i].clear();
        }
    }

    void ModulatorEffectState::do_destruct() final
    {
    }

    void ModulatorEffectState::do_update_device(
        ALCdevice* device) final
    {
        static_cast<void>(device);
    }

    void ModulatorEffectState::do_update(
        ALCdevice* device,
        const EffectSlot* slot,
        const EffectProps* props) final
    {
        if (props->modulator_.waveform_ == EffectProps::Modulator::waveform_sinusoid)
        {
            process_func_ = modulate_sin;
        }
        else if (props->modulator_.waveform_ == EffectProps::Modulator::waveform_sawtooth)
        {
            process_func_ = modulate_saw;
        }
        else
        {
            process_func_ = modulate_square;
        }

        step_ = static_cast<int>(props->modulator_.frequency_ * waveform_frac_one / device->frequency_);

        if (step_ == 0)
        {
            step_ = 1;
        }

        // Custom filter coeffs, which match the old version instead of a low-shelf.
        const auto cw = std::cos(Math::tau * props->modulator_.high_pass_cutoff_ / device->frequency_);
        const auto a = (2.0F - cw) - std::sqrt(std::pow(2.0F - cw, 2.0F) - 1.0F);

        for (int i = 0; i < max_effect_channels; ++i)
        {
            filters_[i].b0_ = a;
            filters_[i].b1_ = -a;
            filters_[i].b2_ = 0.0F;
            filters_[i].a1_ = -a;
            filters_[i].a2_ = 0.0F;
        }

        dst_buffers_ = &device->sample_buffers_;
        dst_channel_count_ = device->channel_count_;

        for (int i = 0; i < max_effect_channels; ++i)
        {
            Panning::compute_first_order_gains(device->channel_count_, device->foa_, mat4f_identity.m_[i], 1.0F, channels_gains_[i].data());
        }
    }

    void ModulatorEffectState::do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final
    {
        for (int base = 0; base < sample_count; )
        {
            float temps[2][128];
            const auto td = std::min(128, sample_count - base);

            for (int j = 0; j < max_effect_channels; ++j)
            {
                filters_[j].process(td, &src_samples[j][base], temps[0]);
                process_func_(temps[1], temps[0], index_, step_, td);

                for (int k = 0; k < channel_count; ++k)
                {
                    const auto gain = channels_gains_[j][k];

                    if (!(std::abs(gain) > silence_threshold_gain))
                    {
                        continue;
                    }

                    for (int i = 0; i < td; ++i)
                    {
                        dst_samples[k][base + i] += gain * temps[1][i];
                    }
                }
            }

            for (int i = 0; i < td; ++i)
            {
                index_ += step_;
                index_ &= waveform_frac_mask;
            }

            base += td;
        }
    }


private:
    static constexpr auto waveform_frac_bits = 24;
    static constexpr auto waveform_frac_one = 1 << waveform_frac_bits;
    static constexpr auto waveform_frac_mask = waveform_frac_one - 1;


    using ChannelsGains = std::array<Gains, max_effect_channels>;
    using Filters = std::array<FilterState, max_effect_channels>;

    using ModulateFunc = float (*)(
        const int index);

    using ProcessFunc = void (*)(
        float* const dst,
        const float* const src,
        int index,
        const int step,
        const int todo);


    ProcessFunc process_func_;
    int index_;
    int step_;
    ChannelsGains channels_gains_;
    Filters filters_;


    static float sin_func(
        const int index)
    {
        return std::sin(index * (Math::tau / waveform_frac_one) - Math::pi) * 0.5F + 0.5F;
    }

    static float saw_func(
        const int index)
    {
        return static_cast<float>(index) / waveform_frac_one;
    }

    static float square_func(
        const int index)
    {
        return static_cast<float>((index >> (waveform_frac_bits - 1)) & 1);
    }

    static void modulate(
        const ModulateFunc func,
        float* const dst,
        const float* const src,
        int index,
        const int step,
        const int todo)
    {
        for (int i = 0; i < todo; ++i)
        {
            index += step;
            index &= waveform_frac_mask;
            dst[i] = src[i] * func(index);
        }
    }

    static void modulate_sin(
        float* const dst,
        const float* const src,
        int index,
        const int step,
        const int todo)
    {
        modulate(sin_func, dst, src, index, step, todo);
    }

    static void modulate_saw(
        float* const dst,
        const float* const src,
        int index,
        const int step,
        const int todo)
    {
        modulate(saw_func, dst, src, index, step, todo);
    }

    static void modulate_square(
        float* const dst,
        const float* const src,
        int index,
        const int step,
        const int todo)
    {
        modulate(square_func, dst, src, index, step, todo);
    }
}; // ModulatorEffectState


EffectState* EffectStateFactory::create_modulator()
{
    return create<ModulatorEffectState>();
}
