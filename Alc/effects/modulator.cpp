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
        process{},
        index{},
        step{},
        Gain{},
        Filter{}
    {
    }

    virtual ~ModulatorEffect()
    {
    }


    void (*process)(ALfloat*, const ALfloat*, ALsizei, const ALsizei, ALsizei);

    ALsizei index;
    ALsizei step;

    ALfloat Gain[MAX_EFFECT_CHANNELS][MAX_OUTPUT_CHANNELS];

    ALfilterState Filter[MAX_EFFECT_CHANNELS];


protected:
    void do_construct() final;

    void do_destruct() final;

    ALboolean do_update_device(
        ALCdevice* device) final;

    void do_update(
        const ALCdevice* device,
        const struct ALeffectslot* slot,
        const union ALeffectProps *props) final;

    void do_process(
        ALsizei samplesToDo,
        const ALfloat(*samplesIn)[BUFFERSIZE],
        ALfloat(*samplesOut)[BUFFERSIZE],
        ALsizei numChannels) final;
}; // ModulatorEffect



#define WAVEFORM_FRACBITS  24
#define WAVEFORM_FRACONE   (1<<WAVEFORM_FRACBITS)
#define WAVEFORM_FRACMASK  (WAVEFORM_FRACONE-1)

static inline ALfloat Sin(ALsizei index)
{
    return sinf(index*(F_TAU/WAVEFORM_FRACONE) - F_PI)*0.5f + 0.5f;
}

static inline ALfloat Saw(ALsizei index)
{
    return (ALfloat)index / WAVEFORM_FRACONE;
}

static inline ALfloat Square(ALsizei index)
{
    return (ALfloat)((index >> (WAVEFORM_FRACBITS - 1)) & 1);
}

#define DECL_TEMPLATE(func)                                                   \
static void Modulate##func(ALfloat *dst, const ALfloat *src,\
                           ALsizei index, const ALsizei step, ALsizei todo)   \
{                                                                             \
    ALsizei i;                                                                \
    for(i = 0;i < todo;i++)                                                   \
    {                                                                         \
        index += step;                                                        \
        index &= WAVEFORM_FRACMASK;                                           \
        dst[i] = src[i] * func(index);                                        \
    }                                                                         \
}

DECL_TEMPLATE(Sin)
DECL_TEMPLATE(Saw)
DECL_TEMPLATE(Square)

#undef DECL_TEMPLATE


void ModulatorEffect::do_construct()
{
    index = 0;
    step = 1;

    for (int i = 0; i < MAX_EFFECT_CHANNELS; ++i)
    {
        ALfilterState_clear(&Filter[i]);
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
    const ALCdevice* Device,
    const struct ALeffectslot* slot,
    const union ALeffectProps *props)
{
    ALfloat cw, a;
    ALsizei i;

    if (props->modulator.waveform == AL_RING_MODULATOR_SINUSOID)
        process = ModulateSin;
    else if (props->modulator.waveform == AL_RING_MODULATOR_SAWTOOTH)
        process = ModulateSaw;
    else /*if(Slot->Params.EffectProps.Modulator.Waveform == AL_RING_MODULATOR_SQUARE)*/
        process = ModulateSquare;

    step = fastf2i(props->modulator.frequency*WAVEFORM_FRACONE /
        Device->frequency);
    if (step == 0) step = 1;

    /* Custom filter coeffs, which match the old version instead of a low-shelf. */
    cw = cosf(F_TAU * props->modulator.high_pass_cutoff / Device->frequency);
    a = (2.0f - cw) - sqrtf(powf(2.0f - cw, 2.0f) - 1.0f);

    for (i = 0; i < MAX_EFFECT_CHANNELS; i++)
    {
        Filter[i].b0 = a;
        Filter[i].b1 = -a;
        Filter[i].b2 = 0.0f;
        Filter[i].a1 = -a;
        Filter[i].a2 = 0.0f;
    }

    out_buffer = Device->foa_out.buffer;
    out_channels = Device->foa_out.num_channels;
    for (i = 0; i < MAX_EFFECT_CHANNELS; i++)
        ComputeFirstOrderGains(Device->foa_out, IdentityMatrixf.m[i],
            1.0F, Gain[i]);
}

void ModulatorEffect::do_process(
    ALsizei SamplesToDo,
    const ALfloat(*SamplesIn)[BUFFERSIZE],
    ALfloat(*SamplesOut)[BUFFERSIZE],
    ALsizei NumChannels)
{
    ALsizei base;

    for (base = 0; base < SamplesToDo;)
    {
        ALfloat temps[2][128];
        ALsizei td = mini(128, SamplesToDo - base);
        ALsizei i, j, k;

        for (j = 0; j < MAX_EFFECT_CHANNELS; j++)
        {
            ALfilterState_process(&Filter[j], temps[0], &SamplesIn[j][base], td);
            process(temps[1], temps[0], index, step, td);

            for (k = 0; k < NumChannels; k++)
            {
                ALfloat gain = Gain[j][k];
                if (!(fabsf(gain) > GAIN_SILENCE_THRESHOLD))
                    continue;

                for (i = 0; i < td; i++)
                    SamplesOut[k][base + i] += gain * temps[1][i];
            }
        }

        for (i = 0; i < td; i++)
        {
            index += step;
            index &= WAVEFORM_FRACMASK;
        }
        base += td;
    }
    index = index;
}

IEffect* create_modulator_effect()
{
    return create_effect<ModulatorEffect>();
}
