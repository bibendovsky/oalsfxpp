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
        process_{},
        index_{},
        step_{},
        gains_{},
        filters_{}
    {
    }

    virtual ~ModulatorEffect()
    {
    }


    void (*process_)(ALfloat*, const ALfloat* const, ALsizei, const ALsizei, const ALsizei);
    ALsizei index_;
    ALsizei step_;
    ALfloat gains_[MAX_EFFECT_CHANNELS][MAX_OUTPUT_CHANNELS];
    ALfilterState filters_[MAX_EFFECT_CHANNELS];


protected:
    void do_construct() final;

    void do_destruct() final;

    ALboolean do_update_device(
        ALCdevice* device) final;

    void do_update(
        ALCdevice* device,
        const struct ALeffectslot* slot,
        const union ALeffectProps *props) final;

    void do_process(
        const ALsizei sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const ALsizei channel_count) final;
}; // ModulatorEffect



constexpr auto WAVEFORM_FRACBITS = 24;
constexpr auto WAVEFORM_FRACONE = 1<<WAVEFORM_FRACBITS;
constexpr auto WAVEFORM_FRACMASK = WAVEFORM_FRACONE-1;


static inline ALfloat Sin(const ALsizei index)
{
    return sinf(index*(F_TAU/WAVEFORM_FRACONE) - F_PI)*0.5f + 0.5f;
}

static inline ALfloat Saw(const ALsizei index)
{
    return (ALfloat)index / WAVEFORM_FRACONE;
}

static inline ALfloat Square(const ALsizei index)
{
    return (ALfloat)((index >> (WAVEFORM_FRACBITS - 1)) & 1);
}


using ModulateFunc = ALfloat (*)(const ALsizei index);

static void Modulate(
    const ModulateFunc func,
    ALfloat* const dst,
    const ALfloat* const src,
    ALsizei index,
    const ALsizei step,
    const ALsizei todo)
{
    for (ALsizei i = 0; i < todo; ++i)
    {
        index += step;
        index &= WAVEFORM_FRACMASK;
        dst[i] = src[i] * func(index);
    }
}

static void ModulateSin(
    ALfloat* const dst,
    const ALfloat* const src,
    ALsizei index,
    const ALsizei step,
    const ALsizei todo)
{
    Modulate(Sin, dst, src, index, step, todo);
}

static void ModulateSaw(
    ALfloat* const dst,
    const ALfloat* const src,
    ALsizei index,
    const ALsizei step,
    const ALsizei todo)
{
    Modulate(Saw, dst, src, index, step, todo);
}

static void ModulateSquare(
    ALfloat* const dst,
    const ALfloat* const src,
    ALsizei index,
    const ALsizei step,
    const ALsizei todo)
{
    Modulate(Square, dst, src, index, step, todo);
}


void ModulatorEffect::do_construct()
{
    index_ = 0;
    step_ = 1;

    for (int i = 0; i < MAX_EFFECT_CHANNELS; ++i)
    {
        ALfilterState_clear(&filters_[i]);
    }
}

void ModulatorEffect::do_destruct()
{
}

ALboolean ModulatorEffect::do_update_device(
    ALCdevice* device)
{
    static_cast<void>(device);
    return AL_TRUE;
}

void ModulatorEffect::do_update(
    ALCdevice* device,
    const struct ALeffectslot* slot,
    const union ALeffectProps *props)
{
    ALfloat cw, a;
    ALsizei i;

    if (props->modulator.waveform == AL_RING_MODULATOR_SINUSOID)
        process_ = ModulateSin;
    else if (props->modulator.waveform == AL_RING_MODULATOR_SAWTOOTH)
        process_ = ModulateSaw;
    else /*if(Slot->Params.EffectProps.Modulator.Waveform == AL_RING_MODULATOR_SQUARE)*/
        process_ = ModulateSquare;

    step_ = fastf2i(props->modulator.frequency*WAVEFORM_FRACONE /
        device->frequency);
    if (step_ == 0) step_ = 1;

    /* Custom filter coeffs, which match the old version instead of a low-shelf. */
    cw = cosf(F_TAU * props->modulator.high_pass_cutoff / device->frequency);
    a = (2.0f - cw) - sqrtf(powf(2.0f - cw, 2.0f) - 1.0f);

    for (i = 0; i < MAX_EFFECT_CHANNELS; i++)
    {
        filters_[i].b0 = a;
        filters_[i].b1 = -a;
        filters_[i].b2 = 0.0f;
        filters_[i].a1 = -a;
        filters_[i].a2 = 0.0f;
    }

    out_buffer = device->foa_out.buffer;
    out_channels = device->foa_out.num_channels;
    for (i = 0; i < MAX_EFFECT_CHANNELS; i++)
        ComputeFirstOrderGains(device->foa_out, IdentityMatrixf.m[i],
            1.0F, gains_[i]);
}

void ModulatorEffect::do_process(
    const ALsizei sample_count,
    const SampleBuffers& src_samples,
    SampleBuffers& dst_samples,
    const ALsizei channel_count)
{
    ALsizei base;

    for (base = 0; base < sample_count;)
    {
        ALfloat temps[2][128];
        ALsizei td = mini(128, sample_count - base);
        ALsizei i, j, k;

        for (j = 0; j < MAX_EFFECT_CHANNELS; j++)
        {
            ALfilterState_processC(&filters_[j], temps[0], &src_samples[j][base], td);
            process_(temps[1], temps[0], index_, step_, td);

            for (k = 0; k < channel_count; k++)
            {
                ALfloat gain = gains_[j][k];
                if (!(fabsf(gain) > GAIN_SILENCE_THRESHOLD))
                    continue;

                for (i = 0; i < td; i++)
                    dst_samples[k][base + i] += gain * temps[1][i];
            }
        }

        for (i = 0; i < td; i++)
        {
            index_ += step_;
            index_ &= WAVEFORM_FRACMASK;
        }
        base += td;
    }
}

IEffect* create_modulator_effect()
{
    return create_effect<ModulatorEffect>();
}
