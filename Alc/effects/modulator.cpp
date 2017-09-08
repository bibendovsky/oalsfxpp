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


#include "config.h"
#include "alu.h"


class ModulatorEffect :
    public IEffect
{
public:
    ModulatorEffect()
        :
        IEffect{},
        process_func_{},
        index_{},
        step_{},
        gains_{},
        filters_{}
    {
    }

    virtual ~ModulatorEffect()
    {
    }


protected:
    void ModulatorEffect::do_construct() final
    {
        index_ = 0;
        step_ = 1;

        for (int i = 0; i < MAX_EFFECT_CHANNELS; ++i)
        {
            ALfilterState_clear(&filters_[i]);
        }
    }

    void ModulatorEffect::do_destruct() final
    {
    }

    bool ModulatorEffect::do_update_device(
        ALCdevice* device) final
    {
        static_cast<void>(device);
        return true;
    }

    void ModulatorEffect::do_update(
        ALCdevice* device,
        const struct ALeffectslot* slot,
        const union ALeffectProps* props) final
    {
        if (props->modulator.waveform == AL_RING_MODULATOR_SINUSOID)
        {
            process_func_ = modulate_sin;
        }
        else if (props->modulator.waveform == AL_RING_MODULATOR_SAWTOOTH)
        {
            process_func_ = modulate_saw;
        }
        else
        {
            process_func_ = modulate_square;
        }

        step_ = fastf2i(props->modulator.frequency * WAVEFORM_FRACONE / device->frequency);

        if (step_ == 0)
        {
            step_ = 1;
        }

        // Custom filter coeffs, which match the old version instead of a low-shelf.
        const auto cw = std::cos(F_TAU * props->modulator.high_pass_cutoff / device->frequency);
        const auto a = (2.0F - cw) - std::sqrt(std::pow(2.0F - cw, 2.0F) - 1.0F);

        for (int i = 0; i < MAX_EFFECT_CHANNELS; ++i)
        {
            filters_[i].b0 = a;
            filters_[i].b1 = -a;
            filters_[i].b2 = 0.0F;
            filters_[i].a1 = -a;
            filters_[i].a2 = 0.0F;
        }

        out_buffer = device->foa_out.buffers;
        out_channels = device->foa_out.num_channels;

        for (int i = 0; i < MAX_EFFECT_CHANNELS; ++i)
        {
            ComputeFirstOrderGains(device->foa_out, IdentityMatrixf.m[i], 1.0F, gains_[i].data());
        }
    }

    void ModulatorEffect::do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final
    {
        for (int base = 0; base < sample_count; )
        {
            float temps[2][128];
            const auto td = std::min(128, sample_count - base);

            for (int j = 0; j < MAX_EFFECT_CHANNELS; ++j)
            {
                ALfilterState_processC(&filters_[j], temps[0], &src_samples[j][base], td);
                process_func_(temps[1], temps[0], index_, step_, td);

                for (int k = 0; k < channel_count; ++k)
                {
                    const auto gain = gains_[j][k];

                    if (!(std::abs(gain) > GAIN_SILENCE_THRESHOLD))
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
                index_ &= WAVEFORM_FRACMASK;
            }

            base += td;
        }
    }


private:
    static constexpr auto WAVEFORM_FRACBITS = 24;
    static constexpr auto WAVEFORM_FRACONE = 1 << WAVEFORM_FRACBITS;
    static constexpr auto WAVEFORM_FRACMASK = WAVEFORM_FRACONE - 1;


    using Gains = MdArray<float, MAX_EFFECT_CHANNELS, MAX_OUTPUT_CHANNELS>;
    using Filters = std::array<ALfilterState, MAX_EFFECT_CHANNELS>;

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
    Gains gains_;
    Filters filters_;


    static float sin_func(
        const int index)
    {
        return std::sin(index * (F_TAU / WAVEFORM_FRACONE) - F_PI) * 0.5F + 0.5F;
    }

    static float saw_func(
        const int index)
    {
        return static_cast<float>(index) / WAVEFORM_FRACONE;
    }

    static float square_func(
        const int index)
    {
        return static_cast<float>((index >> (WAVEFORM_FRACBITS - 1)) & 1);
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
            index &= WAVEFORM_FRACMASK;
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
}; // ModulatorEffect


IEffect* create_modulator_effect()
{
    return create_effect<ModulatorEffect>();
}
