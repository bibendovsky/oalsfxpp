/**
 * OpenAL cross platform audio library
 * Copyright (C) 1999-2007 by authors.
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

#include <stdlib.h>
#include <math.h>
#include <float.h>

#include "AL/al.h"
#include "AL/alc.h"
#include "alMain.h"
#include "alEffect.h"


void InitEffectParams(ALeffect *effect, ALenum type);


ALenum InitEffect(ALeffect *effect)
{
    InitEffectParams(effect, AL_EFFECT_NULL);
    return AL_NO_ERROR;
}

void InitEffectParams(ALeffect *effect, ALenum type)
{
    switch(type)
    {
    case AL_EFFECT_EAXREVERB:
        effect->Props.Reverb.Density   = AL_EAXREVERB_DEFAULT_DENSITY;
        effect->Props.Reverb.Diffusion = AL_EAXREVERB_DEFAULT_DIFFUSION;
        effect->Props.Reverb.Gain   = AL_EAXREVERB_DEFAULT_GAIN;
        effect->Props.Reverb.GainHF = AL_EAXREVERB_DEFAULT_GAINHF;
        effect->Props.Reverb.GainLF = AL_EAXREVERB_DEFAULT_GAINLF;
        effect->Props.Reverb.DecayTime    = AL_EAXREVERB_DEFAULT_DECAY_TIME;
        effect->Props.Reverb.DecayHFRatio = AL_EAXREVERB_DEFAULT_DECAY_HFRATIO;
        effect->Props.Reverb.DecayLFRatio = AL_EAXREVERB_DEFAULT_DECAY_LFRATIO;
        effect->Props.Reverb.ReflectionsGain   = AL_EAXREVERB_DEFAULT_REFLECTIONS_GAIN;
        effect->Props.Reverb.ReflectionsDelay  = AL_EAXREVERB_DEFAULT_REFLECTIONS_DELAY;
        effect->Props.Reverb.ReflectionsPan[0] = AL_EAXREVERB_DEFAULT_REFLECTIONS_PAN_XYZ;
        effect->Props.Reverb.ReflectionsPan[1] = AL_EAXREVERB_DEFAULT_REFLECTIONS_PAN_XYZ;
        effect->Props.Reverb.ReflectionsPan[2] = AL_EAXREVERB_DEFAULT_REFLECTIONS_PAN_XYZ;
        effect->Props.Reverb.LateReverbGain   = AL_EAXREVERB_DEFAULT_LATE_REVERB_GAIN;
        effect->Props.Reverb.LateReverbDelay  = AL_EAXREVERB_DEFAULT_LATE_REVERB_DELAY;
        effect->Props.Reverb.LateReverbPan[0] = AL_EAXREVERB_DEFAULT_LATE_REVERB_PAN_XYZ;
        effect->Props.Reverb.LateReverbPan[1] = AL_EAXREVERB_DEFAULT_LATE_REVERB_PAN_XYZ;
        effect->Props.Reverb.LateReverbPan[2] = AL_EAXREVERB_DEFAULT_LATE_REVERB_PAN_XYZ;
        effect->Props.Reverb.EchoTime  = AL_EAXREVERB_DEFAULT_ECHO_TIME;
        effect->Props.Reverb.EchoDepth = AL_EAXREVERB_DEFAULT_ECHO_DEPTH;
        effect->Props.Reverb.ModulationTime  = AL_EAXREVERB_DEFAULT_MODULATION_TIME;
        effect->Props.Reverb.ModulationDepth = AL_EAXREVERB_DEFAULT_MODULATION_DEPTH;
        effect->Props.Reverb.AirAbsorptionGainHF = AL_EAXREVERB_DEFAULT_AIR_ABSORPTION_GAINHF;
        effect->Props.Reverb.HFReference = AL_EAXREVERB_DEFAULT_HFREFERENCE;
        effect->Props.Reverb.LFReference = AL_EAXREVERB_DEFAULT_LFREFERENCE;
        effect->Props.Reverb.RoomRolloffFactor = AL_EAXREVERB_DEFAULT_ROOM_ROLLOFF_FACTOR;
        effect->Props.Reverb.DecayHFLimit = AL_EAXREVERB_DEFAULT_DECAY_HFLIMIT;
        SET_VTABLE1(ALeaxreverb, effect);
        break;
    case AL_EFFECT_REVERB:
        effect->Props.Reverb.Density   = AL_REVERB_DEFAULT_DENSITY;
        effect->Props.Reverb.Diffusion = AL_REVERB_DEFAULT_DIFFUSION;
        effect->Props.Reverb.Gain   = AL_REVERB_DEFAULT_GAIN;
        effect->Props.Reverb.GainHF = AL_REVERB_DEFAULT_GAINHF;
        effect->Props.Reverb.GainLF = 1.0f;
        effect->Props.Reverb.DecayTime    = AL_REVERB_DEFAULT_DECAY_TIME;
        effect->Props.Reverb.DecayHFRatio = AL_REVERB_DEFAULT_DECAY_HFRATIO;
        effect->Props.Reverb.DecayLFRatio = 1.0f;
        effect->Props.Reverb.ReflectionsGain   = AL_REVERB_DEFAULT_REFLECTIONS_GAIN;
        effect->Props.Reverb.ReflectionsDelay  = AL_REVERB_DEFAULT_REFLECTIONS_DELAY;
        effect->Props.Reverb.ReflectionsPan[0] = 0.0f;
        effect->Props.Reverb.ReflectionsPan[1] = 0.0f;
        effect->Props.Reverb.ReflectionsPan[2] = 0.0f;
        effect->Props.Reverb.LateReverbGain   = AL_REVERB_DEFAULT_LATE_REVERB_GAIN;
        effect->Props.Reverb.LateReverbDelay  = AL_REVERB_DEFAULT_LATE_REVERB_DELAY;
        effect->Props.Reverb.LateReverbPan[0] = 0.0f;
        effect->Props.Reverb.LateReverbPan[1] = 0.0f;
        effect->Props.Reverb.LateReverbPan[2] = 0.0f;
        effect->Props.Reverb.EchoTime  = 0.25f;
        effect->Props.Reverb.EchoDepth = 0.0f;
        effect->Props.Reverb.ModulationTime  = 0.25f;
        effect->Props.Reverb.ModulationDepth = 0.0f;
        effect->Props.Reverb.AirAbsorptionGainHF = AL_REVERB_DEFAULT_AIR_ABSORPTION_GAINHF;
        effect->Props.Reverb.HFReference = 5000.0f;
        effect->Props.Reverb.LFReference = 250.0f;
        effect->Props.Reverb.RoomRolloffFactor = AL_REVERB_DEFAULT_ROOM_ROLLOFF_FACTOR;
        effect->Props.Reverb.DecayHFLimit = AL_REVERB_DEFAULT_DECAY_HFLIMIT;
        SET_VTABLE1(ALreverb, effect);
        break;
    case AL_EFFECT_CHORUS:
        effect->Props.Chorus.Waveform = AL_CHORUS_DEFAULT_WAVEFORM;
        effect->Props.Chorus.Phase = AL_CHORUS_DEFAULT_PHASE;
        effect->Props.Chorus.Rate = AL_CHORUS_DEFAULT_RATE;
        effect->Props.Chorus.Depth = AL_CHORUS_DEFAULT_DEPTH;
        effect->Props.Chorus.Feedback = AL_CHORUS_DEFAULT_FEEDBACK;
        effect->Props.Chorus.Delay = AL_CHORUS_DEFAULT_DELAY;
        SET_VTABLE1(ALchorus, effect);
        break;
    case AL_EFFECT_COMPRESSOR:
        effect->Props.Compressor.OnOff = AL_COMPRESSOR_DEFAULT_ONOFF;
        SET_VTABLE1(ALcompressor, effect);
        break;
    case AL_EFFECT_DISTORTION:
        effect->Props.Distortion.Edge = AL_DISTORTION_DEFAULT_EDGE;
        effect->Props.Distortion.Gain = AL_DISTORTION_DEFAULT_GAIN;
        effect->Props.Distortion.LowpassCutoff = AL_DISTORTION_DEFAULT_LOWPASS_CUTOFF;
        effect->Props.Distortion.EQCenter = AL_DISTORTION_DEFAULT_EQCENTER;
        effect->Props.Distortion.EQBandwidth = AL_DISTORTION_DEFAULT_EQBANDWIDTH;
        SET_VTABLE1(ALdistortion, effect);
        break;
    case AL_EFFECT_ECHO:
        effect->Props.Echo.Delay    = AL_ECHO_DEFAULT_DELAY;
        effect->Props.Echo.LRDelay  = AL_ECHO_DEFAULT_LRDELAY;
        effect->Props.Echo.Damping  = AL_ECHO_DEFAULT_DAMPING;
        effect->Props.Echo.Feedback = AL_ECHO_DEFAULT_FEEDBACK;
        effect->Props.Echo.Spread   = AL_ECHO_DEFAULT_SPREAD;
        SET_VTABLE1(ALecho, effect);
        break;
    case AL_EFFECT_EQUALIZER:
        effect->Props.Equalizer.LowCutoff = AL_EQUALIZER_DEFAULT_LOW_CUTOFF;
        effect->Props.Equalizer.LowGain = AL_EQUALIZER_DEFAULT_LOW_GAIN;
        effect->Props.Equalizer.Mid1Center = AL_EQUALIZER_DEFAULT_MID1_CENTER;
        effect->Props.Equalizer.Mid1Gain = AL_EQUALIZER_DEFAULT_MID1_GAIN;
        effect->Props.Equalizer.Mid1Width = AL_EQUALIZER_DEFAULT_MID1_WIDTH;
        effect->Props.Equalizer.Mid2Center = AL_EQUALIZER_DEFAULT_MID2_CENTER;
        effect->Props.Equalizer.Mid2Gain = AL_EQUALIZER_DEFAULT_MID2_GAIN;
        effect->Props.Equalizer.Mid2Width = AL_EQUALIZER_DEFAULT_MID2_WIDTH;
        effect->Props.Equalizer.HighCutoff = AL_EQUALIZER_DEFAULT_HIGH_CUTOFF;
        effect->Props.Equalizer.HighGain = AL_EQUALIZER_DEFAULT_HIGH_GAIN;
        SET_VTABLE1(ALequalizer, effect);
        break;
    case AL_EFFECT_FLANGER:
        effect->Props.Flanger.Waveform = AL_FLANGER_DEFAULT_WAVEFORM;
        effect->Props.Flanger.Phase = AL_FLANGER_DEFAULT_PHASE;
        effect->Props.Flanger.Rate = AL_FLANGER_DEFAULT_RATE;
        effect->Props.Flanger.Depth = AL_FLANGER_DEFAULT_DEPTH;
        effect->Props.Flanger.Feedback = AL_FLANGER_DEFAULT_FEEDBACK;
        effect->Props.Flanger.Delay = AL_FLANGER_DEFAULT_DELAY;
        SET_VTABLE1(ALflanger, effect);
        break;
    case AL_EFFECT_RING_MODULATOR:
        effect->Props.Modulator.Frequency      = AL_RING_MODULATOR_DEFAULT_FREQUENCY;
        effect->Props.Modulator.HighPassCutoff = AL_RING_MODULATOR_DEFAULT_HIGHPASS_CUTOFF;
        effect->Props.Modulator.Waveform       = AL_RING_MODULATOR_DEFAULT_WAVEFORM;
        SET_VTABLE1(ALmodulator, effect);
        break;
    case AL_EFFECT_DEDICATED_LOW_FREQUENCY_EFFECT:
    case AL_EFFECT_DEDICATED_DIALOGUE:
        effect->Props.Dedicated.Gain = 1.0f;
        SET_VTABLE1(ALdedicated, effect);
        break;
    default:
        SET_VTABLE1(ALnull, effect);
        break;
    }
    effect->type = type;
}
