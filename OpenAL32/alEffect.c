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
        effect->props.reverb.density   = AL_EAXREVERB_DEFAULT_DENSITY;
        effect->props.reverb.diffusion = AL_EAXREVERB_DEFAULT_DIFFUSION;
        effect->props.reverb.gain   = AL_EAXREVERB_DEFAULT_GAIN;
        effect->props.reverb.gain_hf = AL_EAXREVERB_DEFAULT_GAINHF;
        effect->props.reverb.gain_lf = AL_EAXREVERB_DEFAULT_GAINLF;
        effect->props.reverb.decay_time    = AL_EAXREVERB_DEFAULT_DECAY_TIME;
        effect->props.reverb.decay_hf_ratio = AL_EAXREVERB_DEFAULT_DECAY_HFRATIO;
        effect->props.reverb.decay_lf_ratio = AL_EAXREVERB_DEFAULT_DECAY_LFRATIO;
        effect->props.reverb.reflections_gain   = AL_EAXREVERB_DEFAULT_REFLECTIONS_GAIN;
        effect->props.reverb.reflections_delay  = AL_EAXREVERB_DEFAULT_REFLECTIONS_DELAY;
        effect->props.reverb.reflections_pan[0] = AL_EAXREVERB_DEFAULT_REFLECTIONS_PAN_XYZ;
        effect->props.reverb.reflections_pan[1] = AL_EAXREVERB_DEFAULT_REFLECTIONS_PAN_XYZ;
        effect->props.reverb.reflections_pan[2] = AL_EAXREVERB_DEFAULT_REFLECTIONS_PAN_XYZ;
        effect->props.reverb.late_reverb_gain   = AL_EAXREVERB_DEFAULT_LATE_REVERB_GAIN;
        effect->props.reverb.late_reverb_delay  = AL_EAXREVERB_DEFAULT_LATE_REVERB_DELAY;
        effect->props.reverb.late_reverb_pan[0] = AL_EAXREVERB_DEFAULT_LATE_REVERB_PAN_XYZ;
        effect->props.reverb.late_reverb_pan[1] = AL_EAXREVERB_DEFAULT_LATE_REVERB_PAN_XYZ;
        effect->props.reverb.late_reverb_pan[2] = AL_EAXREVERB_DEFAULT_LATE_REVERB_PAN_XYZ;
        effect->props.reverb.echo_time  = AL_EAXREVERB_DEFAULT_ECHO_TIME;
        effect->props.reverb.echo_depth = AL_EAXREVERB_DEFAULT_ECHO_DEPTH;
        effect->props.reverb.modulation_time  = AL_EAXREVERB_DEFAULT_MODULATION_TIME;
        effect->props.reverb.modulation_depth = AL_EAXREVERB_DEFAULT_MODULATION_DEPTH;
        effect->props.reverb.air_absorption_gain_hf = AL_EAXREVERB_DEFAULT_AIR_ABSORPTION_GAINHF;
        effect->props.reverb.hf_reference = AL_EAXREVERB_DEFAULT_HFREFERENCE;
        effect->props.reverb.lf_reference = AL_EAXREVERB_DEFAULT_LFREFERENCE;
        effect->props.reverb.room_rolloff_factor = AL_EAXREVERB_DEFAULT_ROOM_ROLLOFF_FACTOR;
        effect->props.reverb.decay_hf_limit = AL_EAXREVERB_DEFAULT_DECAY_HFLIMIT;
        break;
    case AL_EFFECT_REVERB:
        effect->props.reverb.density   = AL_REVERB_DEFAULT_DENSITY;
        effect->props.reverb.diffusion = AL_REVERB_DEFAULT_DIFFUSION;
        effect->props.reverb.gain   = AL_REVERB_DEFAULT_GAIN;
        effect->props.reverb.gain_hf = AL_REVERB_DEFAULT_GAINHF;
        effect->props.reverb.gain_lf = 1.0f;
        effect->props.reverb.decay_time    = AL_REVERB_DEFAULT_DECAY_TIME;
        effect->props.reverb.decay_hf_ratio = AL_REVERB_DEFAULT_DECAY_HFRATIO;
        effect->props.reverb.decay_lf_ratio = 1.0f;
        effect->props.reverb.reflections_gain   = AL_REVERB_DEFAULT_REFLECTIONS_GAIN;
        effect->props.reverb.reflections_delay  = AL_REVERB_DEFAULT_REFLECTIONS_DELAY;
        effect->props.reverb.reflections_pan[0] = 0.0f;
        effect->props.reverb.reflections_pan[1] = 0.0f;
        effect->props.reverb.reflections_pan[2] = 0.0f;
        effect->props.reverb.late_reverb_gain   = AL_REVERB_DEFAULT_LATE_REVERB_GAIN;
        effect->props.reverb.late_reverb_delay  = AL_REVERB_DEFAULT_LATE_REVERB_DELAY;
        effect->props.reverb.late_reverb_pan[0] = 0.0f;
        effect->props.reverb.late_reverb_pan[1] = 0.0f;
        effect->props.reverb.late_reverb_pan[2] = 0.0f;
        effect->props.reverb.echo_time  = 0.25f;
        effect->props.reverb.echo_depth = 0.0f;
        effect->props.reverb.modulation_time  = 0.25f;
        effect->props.reverb.modulation_depth = 0.0f;
        effect->props.reverb.air_absorption_gain_hf = AL_REVERB_DEFAULT_AIR_ABSORPTION_GAINHF;
        effect->props.reverb.hf_reference = 5000.0f;
        effect->props.reverb.lf_reference = 250.0f;
        effect->props.reverb.room_rolloff_factor = AL_REVERB_DEFAULT_ROOM_ROLLOFF_FACTOR;
        effect->props.reverb.decay_hf_limit = AL_REVERB_DEFAULT_DECAY_HFLIMIT;
        break;
    case AL_EFFECT_CHORUS:
        effect->props.chorus.waveform = AL_CHORUS_DEFAULT_WAVEFORM;
        effect->props.chorus.phase = AL_CHORUS_DEFAULT_PHASE;
        effect->props.chorus.rate = AL_CHORUS_DEFAULT_RATE;
        effect->props.chorus.depth = AL_CHORUS_DEFAULT_DEPTH;
        effect->props.chorus.feedback = AL_CHORUS_DEFAULT_FEEDBACK;
        effect->props.chorus.delay = AL_CHORUS_DEFAULT_DELAY;
        break;
    case AL_EFFECT_COMPRESSOR:
        effect->props.compressor.on_off = AL_COMPRESSOR_DEFAULT_ONOFF;
        break;
    case AL_EFFECT_DISTORTION:
        effect->props.distortion.edge = AL_DISTORTION_DEFAULT_EDGE;
        effect->props.distortion.gain = AL_DISTORTION_DEFAULT_GAIN;
        effect->props.distortion.lowpass_cutoff = AL_DISTORTION_DEFAULT_LOWPASS_CUTOFF;
        effect->props.distortion.eq_center = AL_DISTORTION_DEFAULT_EQCENTER;
        effect->props.distortion.eq_bandwidth = AL_DISTORTION_DEFAULT_EQBANDWIDTH;
        break;
    case AL_EFFECT_ECHO:
        effect->props.echo.delay    = AL_ECHO_DEFAULT_DELAY;
        effect->props.echo.lr_delay  = AL_ECHO_DEFAULT_LRDELAY;
        effect->props.echo.damping  = AL_ECHO_DEFAULT_DAMPING;
        effect->props.echo.feedback = AL_ECHO_DEFAULT_FEEDBACK;
        effect->props.echo.spread   = AL_ECHO_DEFAULT_SPREAD;
        break;
    case AL_EFFECT_EQUALIZER:
        effect->props.equalizer.low_cutoff = AL_EQUALIZER_DEFAULT_LOW_CUTOFF;
        effect->props.equalizer.low_gain = AL_EQUALIZER_DEFAULT_LOW_GAIN;
        effect->props.equalizer.mid1_center = AL_EQUALIZER_DEFAULT_MID1_CENTER;
        effect->props.equalizer.mid1_gain = AL_EQUALIZER_DEFAULT_MID1_GAIN;
        effect->props.equalizer.mid1_width = AL_EQUALIZER_DEFAULT_MID1_WIDTH;
        effect->props.equalizer.mid2_center = AL_EQUALIZER_DEFAULT_MID2_CENTER;
        effect->props.equalizer.mid2_gain = AL_EQUALIZER_DEFAULT_MID2_GAIN;
        effect->props.equalizer.mid2_width = AL_EQUALIZER_DEFAULT_MID2_WIDTH;
        effect->props.equalizer.high_cutoff = AL_EQUALIZER_DEFAULT_HIGH_CUTOFF;
        effect->props.equalizer.high_gain = AL_EQUALIZER_DEFAULT_HIGH_GAIN;
        break;
    case AL_EFFECT_FLANGER:
        effect->props.flanger.waveform = AL_FLANGER_DEFAULT_WAVEFORM;
        effect->props.flanger.phase = AL_FLANGER_DEFAULT_PHASE;
        effect->props.flanger.rate = AL_FLANGER_DEFAULT_RATE;
        effect->props.flanger.depth = AL_FLANGER_DEFAULT_DEPTH;
        effect->props.flanger.feedback = AL_FLANGER_DEFAULT_FEEDBACK;
        effect->props.flanger.delay = AL_FLANGER_DEFAULT_DELAY;
        break;
    case AL_EFFECT_RING_MODULATOR:
        effect->props.modulator.frequency      = AL_RING_MODULATOR_DEFAULT_FREQUENCY;
        effect->props.modulator.high_pass_cutoff = AL_RING_MODULATOR_DEFAULT_HIGHPASS_CUTOFF;
        effect->props.modulator.waveform       = AL_RING_MODULATOR_DEFAULT_WAVEFORM;
        break;
    case AL_EFFECT_DEDICATED_LOW_FREQUENCY_EFFECT:
    case AL_EFFECT_DEDICATED_DIALOGUE:
        effect->props.dedicated.gain = 1.0f;
        break;
    default:
        break;
    }
    effect->type = type;
}
