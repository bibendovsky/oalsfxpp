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
#include "alEffect.h"


// ==========================================================================
// Effect

Effect::Effect()
    :
    type_{AL_EFFECT_NULL},
    props_{}
{
}

void Effect::initialize(
    const int type)
{
    switch (type)
    {
    case AL_EFFECT_EAXREVERB:
        props_.reverb.density = AL_EAXREVERB_DEFAULT_DENSITY;
        props_.reverb.diffusion = AL_EAXREVERB_DEFAULT_DIFFUSION;
        props_.reverb.gain = AL_EAXREVERB_DEFAULT_GAIN;
        props_.reverb.gain_hf = AL_EAXREVERB_DEFAULT_GAINHF;
        props_.reverb.gain_lf = AL_EAXREVERB_DEFAULT_GAINLF;
        props_.reverb.decay_time = AL_EAXREVERB_DEFAULT_DECAY_TIME;
        props_.reverb.decay_hf_ratio = AL_EAXREVERB_DEFAULT_DECAY_HFRATIO;
        props_.reverb.decay_lf_ratio = AL_EAXREVERB_DEFAULT_DECAY_LFRATIO;
        props_.reverb.reflections_gain = AL_EAXREVERB_DEFAULT_REFLECTIONS_GAIN;
        props_.reverb.reflections_delay = AL_EAXREVERB_DEFAULT_REFLECTIONS_DELAY;
        props_.reverb.reflections_pan[0] = AL_EAXREVERB_DEFAULT_REFLECTIONS_PAN_XYZ;
        props_.reverb.reflections_pan[1] = AL_EAXREVERB_DEFAULT_REFLECTIONS_PAN_XYZ;
        props_.reverb.reflections_pan[2] = AL_EAXREVERB_DEFAULT_REFLECTIONS_PAN_XYZ;
        props_.reverb.late_reverb_gain = AL_EAXREVERB_DEFAULT_LATE_REVERB_GAIN;
        props_.reverb.late_reverb_delay = AL_EAXREVERB_DEFAULT_LATE_REVERB_DELAY;
        props_.reverb.late_reverb_pan[0] = AL_EAXREVERB_DEFAULT_LATE_REVERB_PAN_XYZ;
        props_.reverb.late_reverb_pan[1] = AL_EAXREVERB_DEFAULT_LATE_REVERB_PAN_XYZ;
        props_.reverb.late_reverb_pan[2] = AL_EAXREVERB_DEFAULT_LATE_REVERB_PAN_XYZ;
        props_.reverb.echo_time = AL_EAXREVERB_DEFAULT_ECHO_TIME;
        props_.reverb.echo_depth = AL_EAXREVERB_DEFAULT_ECHO_DEPTH;
        props_.reverb.modulation_time = AL_EAXREVERB_DEFAULT_MODULATION_TIME;
        props_.reverb.modulation_depth = AL_EAXREVERB_DEFAULT_MODULATION_DEPTH;
        props_.reverb.air_absorption_gain_hf = AL_EAXREVERB_DEFAULT_AIR_ABSORPTION_GAINHF;
        props_.reverb.hf_reference = AL_EAXREVERB_DEFAULT_HFREFERENCE;
        props_.reverb.lf_reference = AL_EAXREVERB_DEFAULT_LFREFERENCE;
        props_.reverb.room_rolloff_factor = AL_EAXREVERB_DEFAULT_ROOM_ROLLOFF_FACTOR;
        props_.reverb.decay_hf_limit = AL_EAXREVERB_DEFAULT_DECAY_HFLIMIT;
        break;

    case AL_EFFECT_REVERB:
        props_.reverb.density = AL_REVERB_DEFAULT_DENSITY;
        props_.reverb.diffusion = AL_REVERB_DEFAULT_DIFFUSION;
        props_.reverb.gain = AL_REVERB_DEFAULT_GAIN;
        props_.reverb.gain_hf = AL_REVERB_DEFAULT_GAINHF;
        props_.reverb.gain_lf = 1.0F;
        props_.reverb.decay_time = AL_REVERB_DEFAULT_DECAY_TIME;
        props_.reverb.decay_hf_ratio = AL_REVERB_DEFAULT_DECAY_HFRATIO;
        props_.reverb.decay_lf_ratio = 1.0F;
        props_.reverb.reflections_gain = AL_REVERB_DEFAULT_REFLECTIONS_GAIN;
        props_.reverb.reflections_delay = AL_REVERB_DEFAULT_REFLECTIONS_DELAY;
        props_.reverb.reflections_pan[0] = 0.0F;
        props_.reverb.reflections_pan[1] = 0.0F;
        props_.reverb.reflections_pan[2] = 0.0F;
        props_.reverb.late_reverb_gain = AL_REVERB_DEFAULT_LATE_REVERB_GAIN;
        props_.reverb.late_reverb_delay = AL_REVERB_DEFAULT_LATE_REVERB_DELAY;
        props_.reverb.late_reverb_pan[0] = 0.0F;
        props_.reverb.late_reverb_pan[1] = 0.0F;
        props_.reverb.late_reverb_pan[2] = 0.0F;
        props_.reverb.echo_time = 0.25F;
        props_.reverb.echo_depth = 0.0F;
        props_.reverb.modulation_time = 0.25F;
        props_.reverb.modulation_depth = 0.0F;
        props_.reverb.air_absorption_gain_hf = AL_REVERB_DEFAULT_AIR_ABSORPTION_GAINHF;
        props_.reverb.hf_reference = 5000.0F;
        props_.reverb.lf_reference = 250.0F;
        props_.reverb.room_rolloff_factor = AL_REVERB_DEFAULT_ROOM_ROLLOFF_FACTOR;
        props_.reverb.decay_hf_limit = AL_REVERB_DEFAULT_DECAY_HFLIMIT;
        break;

    case AL_EFFECT_CHORUS:
        props_.chorus.waveform = AL_CHORUS_DEFAULT_WAVEFORM;
        props_.chorus.phase = AL_CHORUS_DEFAULT_PHASE;
        props_.chorus.rate = AL_CHORUS_DEFAULT_RATE;
        props_.chorus.depth = AL_CHORUS_DEFAULT_DEPTH;
        props_.chorus.feedback = AL_CHORUS_DEFAULT_FEEDBACK;
        props_.chorus.delay = AL_CHORUS_DEFAULT_DELAY;
        break;

    case AL_EFFECT_COMPRESSOR:
        props_.compressor.on_off = AL_COMPRESSOR_DEFAULT_ONOFF;
        break;

    case AL_EFFECT_DISTORTION:
        props_.distortion.edge = AL_DISTORTION_DEFAULT_EDGE;
        props_.distortion.gain = AL_DISTORTION_DEFAULT_GAIN;
        props_.distortion.lowpass_cutoff = AL_DISTORTION_DEFAULT_LOWPASS_CUTOFF;
        props_.distortion.eq_center = AL_DISTORTION_DEFAULT_EQCENTER;
        props_.distortion.eq_bandwidth = AL_DISTORTION_DEFAULT_EQBANDWIDTH;
        break;

    case AL_EFFECT_ECHO:
        props_.echo.delay = AL_ECHO_DEFAULT_DELAY;
        props_.echo.lr_delay = AL_ECHO_DEFAULT_LRDELAY;
        props_.echo.damping = AL_ECHO_DEFAULT_DAMPING;
        props_.echo.feedback = AL_ECHO_DEFAULT_FEEDBACK;
        props_.echo.spread = AL_ECHO_DEFAULT_SPREAD;
        break;

    case AL_EFFECT_EQUALIZER:
        props_.equalizer.low_cutoff = AL_EQUALIZER_DEFAULT_LOW_CUTOFF;
        props_.equalizer.low_gain = AL_EQUALIZER_DEFAULT_LOW_GAIN;
        props_.equalizer.mid1_center = AL_EQUALIZER_DEFAULT_MID1_CENTER;
        props_.equalizer.mid1_gain = AL_EQUALIZER_DEFAULT_MID1_GAIN;
        props_.equalizer.mid1_width = AL_EQUALIZER_DEFAULT_MID1_WIDTH;
        props_.equalizer.mid2_center = AL_EQUALIZER_DEFAULT_MID2_CENTER;
        props_.equalizer.mid2_gain = AL_EQUALIZER_DEFAULT_MID2_GAIN;
        props_.equalizer.mid2_width = AL_EQUALIZER_DEFAULT_MID2_WIDTH;
        props_.equalizer.high_cutoff = AL_EQUALIZER_DEFAULT_HIGH_CUTOFF;
        props_.equalizer.high_gain = AL_EQUALIZER_DEFAULT_HIGH_GAIN;
        break;

    case AL_EFFECT_FLANGER:
        props_.flanger.waveform = AL_FLANGER_DEFAULT_WAVEFORM;
        props_.flanger.phase = AL_FLANGER_DEFAULT_PHASE;
        props_.flanger.rate = AL_FLANGER_DEFAULT_RATE;
        props_.flanger.depth = AL_FLANGER_DEFAULT_DEPTH;
        props_.flanger.feedback = AL_FLANGER_DEFAULT_FEEDBACK;
        props_.flanger.delay = AL_FLANGER_DEFAULT_DELAY;
        break;

    case AL_EFFECT_RING_MODULATOR:
        props_.modulator.frequency = AL_RING_MODULATOR_DEFAULT_FREQUENCY;
        props_.modulator.high_pass_cutoff = AL_RING_MODULATOR_DEFAULT_HIGHPASS_CUTOFF;
        props_.modulator.waveform = AL_RING_MODULATOR_DEFAULT_WAVEFORM;
        break;

    case AL_EFFECT_DEDICATED_LOW_FREQUENCY_EFFECT:
    case AL_EFFECT_DEDICATED_DIALOGUE:
        props_.dedicated.gain = 1.0F;
        break;

    default:
        break;
    }

    type_ = type;
}

// Effect
// ==========================================================================


// ==========================================================================
// EffectState

EffectState::EffectState()
    :
    out_buffer{},
    out_channels{}
{
}

EffectState::~EffectState()
{
}

void EffectState::construct()
{
    do_construct();
}

void EffectState::destruct()
{
    do_destruct();
}

void EffectState::update_device(
    ALCdevice* device)
{
    do_update_device(device);
}

void EffectState::update(
    ALCdevice* device,
    const EffectSlot* slot,
    const EffectProps *props)
{
    do_update(device, slot, props);
}

void EffectState::process(
    int sample_count,
    const SampleBuffers& src_samples,
    SampleBuffers& dst_samples,
    const int channel_count)
{
    do_process(sample_count, src_samples, dst_samples, channel_count);
}

void EffectState::destroy(
    EffectState*& effect_state)
{
    if (!effect_state)
    {
        return;
    }

    effect_state->destruct();
    delete effect_state;
    effect_state = nullptr;
}

// EffectState
// ==========================================================================


// ==========================================================================
// EffectStateDeleter

void EffectStateDeleter::operator()(
    EffectState* effect_state)
{
    if (!effect_state)
    {
        return;
    }

    effect_state->destruct();
    delete effect_state;
}

// EffectStateDeleter
// ==========================================================================


// ==========================================================================
// EffectStateFactory

EffectState* EffectStateFactory::create_by_type(
    const int type)
{
    switch (type)
    {
    case AL_EFFECT_NULL:
        return create_null();

    case AL_EFFECT_EAXREVERB:
        return create_reverb();

    case AL_EFFECT_REVERB:
        return create_reverb();

    case AL_EFFECT_CHORUS:
        return create_chorus();

    case AL_EFFECT_COMPRESSOR:
        return create_compressor();

    case AL_EFFECT_DISTORTION:
        return create_distortion();

    case AL_EFFECT_ECHO:
        return create_echo();

    case AL_EFFECT_EQUALIZER:
        return create_equalizer();

    case AL_EFFECT_FLANGER:
        return create_flanger();

    case AL_EFFECT_RING_MODULATOR:
        return create_modulator();

    case AL_EFFECT_DEDICATED_DIALOGUE:
        return create_dedicated();

    case AL_EFFECT_DEDICATED_LOW_FREQUENCY_EFFECT:
        return create_dedicated();

    default:
        return nullptr;
    }
}

// EffectStateFactory
// ==========================================================================
