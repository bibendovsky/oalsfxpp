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


typedef struct ALdistortionState {
    DERIVE_FROM_TYPE(ALeffectState);

    /* Effect gains for each channel */
    ALfloat Gain[MAX_OUTPUT_CHANNELS];

    /* Effect parameters */
    ALfilterState lowpass;
    ALfilterState bandpass;
    ALfloat attenuation;
    ALfloat edge_coeff;
} ALdistortionState;

static ALvoid ALdistortionState_Destruct(ALdistortionState *state);
static ALboolean ALdistortionState_deviceUpdate(ALdistortionState *state, ALCdevice *device);
static ALvoid ALdistortionState_update(ALdistortionState *state, const ALCdevice *Device, const ALeffectslot *Slot, const ALeffectProps *props);
static ALvoid ALdistortionState_process(ALdistortionState *state, ALsizei SamplesToDo, const ALfloat (*restrict SamplesIn)[BUFFERSIZE], ALfloat (*restrict SamplesOut)[BUFFERSIZE], ALsizei NumChannels);
DECLARE_DEFAULT_ALLOCATORS(ALdistortionState)

DEFINE_ALEFFECTSTATE_VTABLE(ALdistortionState);


static void ALdistortionState_Construct(ALdistortionState *state)
{
    ALeffectState_Construct(STATIC_CAST(ALeffectState, state));
    SET_VTABLE2(ALdistortionState, ALeffectState, state);

    ALfilterState_clear(&state->lowpass);
    ALfilterState_clear(&state->bandpass);
}

static ALvoid ALdistortionState_Destruct(ALdistortionState *state)
{
    ALeffectState_Destruct(STATIC_CAST(ALeffectState,state));
}

static ALboolean ALdistortionState_deviceUpdate(ALdistortionState *UNUSED(state), ALCdevice *UNUSED(device))
{
    return AL_TRUE;
}

static ALvoid ALdistortionState_update(ALdistortionState *state, const ALCdevice *Device, const ALeffectslot *Slot, const ALeffectProps *props)
{
    ALfloat frequency = (ALfloat)Device->frequency;
    ALfloat bandwidth;
    ALfloat cutoff;
    ALfloat edge;

    /* Store distorted signal attenuation settings. */
    state->attenuation = props->distortion.gain;

    /* Store waveshaper edge settings. */
    edge = sinf(props->distortion.edge * (F_PI_2));
    edge = minf(edge, 0.99f);
    state->edge_coeff = 2.0f * edge / (1.0f-edge);

    cutoff = props->distortion.lowpass_cutoff;
    /* Bandwidth value is constant in octaves. */
    bandwidth = (cutoff / 2.0f) / (cutoff * 0.67f);
    /* Multiply sampling frequency by the amount of oversampling done during
     * processing.
     */
    ALfilterState_setParams(&state->lowpass, ALfilterType_LowPass, 1.0f,
        cutoff / (frequency*4.0f), calc_rcpQ_from_bandwidth(cutoff / (frequency*4.0f), bandwidth)
    );

    cutoff = props->distortion.eq_center;
    /* Convert bandwidth in Hz to octaves. */
    bandwidth = props->distortion.eq_bandwidth / (cutoff * 0.67f);
    ALfilterState_setParams(&state->bandpass, ALfilterType_BandPass, 1.0f,
        cutoff / (frequency*4.0f), calc_rcpQ_from_bandwidth(cutoff / (frequency*4.0f), bandwidth)
    );

    ComputeAmbientGains(Device->dry, 1.0F, state->Gain);
}

static ALvoid ALdistortionState_process(ALdistortionState *state, ALsizei SamplesToDo, const ALfloat (*restrict SamplesIn)[BUFFERSIZE], ALfloat (*restrict SamplesOut)[BUFFERSIZE], ALsizei NumChannels)
{
    const ALfloat fc = state->edge_coeff;
    ALsizei it, kt;
    ALsizei base;

    for(base = 0;base < SamplesToDo;)
    {
        float buffer[2][64 * 4];
        ALsizei td = mini(64, SamplesToDo-base);

        /* Perform 4x oversampling to avoid aliasing. Oversampling greatly
         * improves distortion quality and allows to implement lowpass and
         * bandpass filters using high frequencies, at which classic IIR
         * filters became unstable.
         */

        /* Fill oversample buffer using zero stuffing. */
        for(it = 0;it < td;it++)
        {
            /* Multiply the sample by the amount of oversampling to maintain
             * the signal's power.
             */
            buffer[0][it*4 + 0] = SamplesIn[0][it+base] * 4.0f;
            buffer[0][it*4 + 1] = 0.0f;
            buffer[0][it*4 + 2] = 0.0f;
            buffer[0][it*4 + 3] = 0.0f;
        }

        /* First step, do lowpass filtering of original signal. Additionally
         * perform buffer interpolation and lowpass cutoff for oversampling
         * (which is fortunately first step of distortion). So combine three
         * operations into the one.
         */
        ALfilterState_process(&state->lowpass, buffer[1], buffer[0], td*4);

        /* Second step, do distortion using waveshaper function to emulate
         * signal processing during tube overdriving. Three steps of
         * waveshaping are intended to modify waveform without boost/clipping/
         * attenuation process.
         */
        for(it = 0;it < td*4;it++)
        {
            ALfloat smp = buffer[1][it];

            smp = (1.0f + fc) * smp/(1.0f + fc*fabsf(smp));
            smp = (1.0f + fc) * smp/(1.0f + fc*fabsf(smp)) * -1.0f;
            smp = (1.0f + fc) * smp/(1.0f + fc*fabsf(smp));

            buffer[0][it] = smp;
        }

        /* Third step, do bandpass filtering of distorted signal. */
        ALfilterState_process(&state->bandpass, buffer[1], buffer[0], td*4);

        for(kt = 0;kt < NumChannels;kt++)
        {
            /* Fourth step, final, do attenuation and perform decimation,
             * store only one sample out of 4.
             */
            ALfloat gain = state->Gain[kt] * state->attenuation;
            if(!(fabsf(gain) > GAIN_SILENCE_THRESHOLD))
                continue;

            for(it = 0;it < td;it++)
                SamplesOut[kt][base+it] += gain * buffer[1][it*4];
        }

        base += td;
    }
}


typedef struct ALdistortionStateFactory {
    DERIVE_FROM_TYPE(ALeffectStateFactory);
} ALdistortionStateFactory;

static ALeffectState *ALdistortionStateFactory_create(ALdistortionStateFactory *UNUSED(factory))
{
    ALdistortionState *state;

    NEW_OBJ0(state, ALdistortionState)();
    if(!state) return NULL;

    return STATIC_CAST(ALeffectState, state);
}

DEFINE_ALEFFECTSTATEFACTORY_VTABLE(ALdistortionStateFactory);


ALeffectStateFactory *ALdistortionStateFactory_getFactory(void)
{
    static ALdistortionStateFactory DistortionFactory = { { GET_VTABLE2(ALdistortionStateFactory, ALeffectStateFactory) } };

    return STATIC_CAST(ALeffectStateFactory, &DistortionFactory);
}
