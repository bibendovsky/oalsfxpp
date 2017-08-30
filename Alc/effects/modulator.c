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

#include <math.h>
#include <stdlib.h>

#include "alMain.h"
#include "alFilter.h"
#include "alAuxEffectSlot.h"
#include "alu.h"


typedef struct ALmodulatorState {
    DERIVE_FROM_TYPE(ALeffectState);

    void (*Process)(ALfloat*, const ALfloat*, ALsizei, const ALsizei, ALsizei);

    ALsizei index;
    ALsizei step;

    ALfloat Gain[MAX_EFFECT_CHANNELS][MAX_OUTPUT_CHANNELS];

    ALfilterState Filter[MAX_EFFECT_CHANNELS];
} ALmodulatorState;

static ALvoid ALmodulatorState_Destruct(ALmodulatorState *state);
static ALboolean ALmodulatorState_deviceUpdate(ALmodulatorState *state, ALCdevice *device);
static ALvoid ALmodulatorState_update(ALmodulatorState *state, const ALCdevice *Device, const ALeffectslot *Slot, const ALeffectProps *props);
static ALvoid ALmodulatorState_process(ALmodulatorState *state, ALsizei SamplesToDo, const ALfloat (*restrict SamplesIn)[BUFFERSIZE], ALfloat (*restrict SamplesOut)[BUFFERSIZE], ALsizei NumChannels);
DECLARE_DEFAULT_ALLOCATORS(ALmodulatorState)

DEFINE_ALEFFECTSTATE_VTABLE(ALmodulatorState);


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
static void Modulate##func(ALfloat *restrict dst, const ALfloat *restrict src,\
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


static void ALmodulatorState_Construct(ALmodulatorState *state)
{
    ALuint i;

    ALeffectState_Construct(STATIC_CAST(ALeffectState, state));
    SET_VTABLE2(ALmodulatorState, ALeffectState, state);

    state->index = 0;
    state->step = 1;

    for(i = 0;i < MAX_EFFECT_CHANNELS;i++)
        ALfilterState_clear(&state->Filter[i]);
}

static ALvoid ALmodulatorState_Destruct(ALmodulatorState *state)
{
    ALeffectState_Destruct(STATIC_CAST(ALeffectState,state));
}

static ALboolean ALmodulatorState_deviceUpdate(ALmodulatorState *UNUSED(state), ALCdevice *UNUSED(device))
{
    return AL_TRUE;
}

static ALvoid ALmodulatorState_update(ALmodulatorState *state, const ALCdevice *Device, const ALeffectslot *Slot, const ALeffectProps *props)
{
    ALfloat cw, a;
    ALsizei i;

    if(props->modulator.waveform == AL_RING_MODULATOR_SINUSOID)
        state->Process = ModulateSin;
    else if(props->modulator.waveform == AL_RING_MODULATOR_SAWTOOTH)
        state->Process = ModulateSaw;
    else /*if(Slot->Params.EffectProps.Modulator.Waveform == AL_RING_MODULATOR_SQUARE)*/
        state->Process = ModulateSquare;

    state->step = fastf2i(props->modulator.frequency*WAVEFORM_FRACONE /
                          Device->frequency);
    if(state->step == 0) state->step = 1;

    /* Custom filter coeffs, which match the old version instead of a low-shelf. */
    cw = cosf(F_TAU * props->modulator.high_pass_cutoff / Device->frequency);
    a = (2.0f-cw) - sqrtf(powf(2.0f-cw, 2.0f) - 1.0f);

    for(i = 0;i < MAX_EFFECT_CHANNELS;i++)
    {
        state->Filter[i].b0 = a;
        state->Filter[i].b1 = -a;
        state->Filter[i].b2 = 0.0f;
        state->Filter[i].a1 = -a;
        state->Filter[i].a2 = 0.0f;
    }

    STATIC_CAST(ALeffectState,state)->out_buffer = Device->foa_out.buffer;
    STATIC_CAST(ALeffectState,state)->out_channels = Device->foa_out.num_channels;
    for(i = 0;i < MAX_EFFECT_CHANNELS;i++)
        ComputeFirstOrderGains(Device->foa_out, IdentityMatrixf.m[i],
                               1.0F, state->Gain[i]);
}

static ALvoid ALmodulatorState_process(ALmodulatorState *state, ALsizei SamplesToDo, const ALfloat (*restrict SamplesIn)[BUFFERSIZE], ALfloat (*restrict SamplesOut)[BUFFERSIZE], ALsizei NumChannels)
{
    const ALsizei step = state->step;
    ALsizei index = state->index;
    ALsizei base;

    for(base = 0;base < SamplesToDo;)
    {
        ALfloat temps[2][128];
        ALsizei td = mini(128, SamplesToDo-base);
        ALsizei i, j, k;

        for(j = 0;j < MAX_EFFECT_CHANNELS;j++)
        {
            ALfilterState_process(&state->Filter[j], temps[0], &SamplesIn[j][base], td);
            state->Process(temps[1], temps[0], index, step, td);

            for(k = 0;k < NumChannels;k++)
            {
                ALfloat gain = state->Gain[j][k];
                if(!(fabsf(gain) > GAIN_SILENCE_THRESHOLD))
                    continue;

                for(i = 0;i < td;i++)
                    SamplesOut[k][base+i] += gain * temps[1][i];
            }
        }

        for(i = 0;i < td;i++)
        {
            index += step;
            index &= WAVEFORM_FRACMASK;
        }
        base += td;
    }
    state->index = index;
}


typedef struct ALmodulatorStateFactory {
    DERIVE_FROM_TYPE(ALeffectStateFactory);
} ALmodulatorStateFactory;

static ALeffectState *ALmodulatorStateFactory_create(ALmodulatorStateFactory *UNUSED(factory))
{
    ALmodulatorState *state;

    NEW_OBJ0(state, ALmodulatorState)();
    if(!state) return NULL;

    return STATIC_CAST(ALeffectState, state);
}

DEFINE_ALEFFECTSTATEFACTORY_VTABLE(ALmodulatorStateFactory);

ALeffectStateFactory *ALmodulatorStateFactory_getFactory(void)
{
    static ALmodulatorStateFactory ModulatorFactory = { { GET_VTABLE2(ALmodulatorStateFactory, ALeffectStateFactory) } };

    return STATIC_CAST(ALeffectStateFactory, &ModulatorFactory);
}
