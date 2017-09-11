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
    type_{},
    props_{}
{
}

void Effect::initialize(
    const EffectType type)
{
    switch (type)
    {
    case EffectType::reverb:
    case EffectType::eax_reverb:
        props_.reverb.density = EffectProps::Reverb::default_density;
        props_.reverb.diffusion = EffectProps::Reverb::default_diffusion;
        props_.reverb.gain = EffectProps::Reverb::default_gain;
        props_.reverb.gain_hf = EffectProps::Reverb::default_gain_hf;
        props_.reverb.gain_lf = EffectProps::Reverb::default_gain_lf;
        props_.reverb.decay_time = EffectProps::Reverb::default_decay_time;
        props_.reverb.decay_hf_ratio = EffectProps::Reverb::default_decay_hf_ratio;
        props_.reverb.decay_lf_ratio = EffectProps::Reverb::default_decay_lf_ratio;
        props_.reverb.reflections_gain = EffectProps::Reverb::default_reflections_gain;
        props_.reverb.reflections_delay = EffectProps::Reverb::default_reflections_delay;
        props_.reverb.reflections_pan[0] = EffectProps::Reverb::default_reflections_pan_xyz;
        props_.reverb.reflections_pan[1] = EffectProps::Reverb::default_reflections_pan_xyz;
        props_.reverb.reflections_pan[2] = EffectProps::Reverb::default_reflections_pan_xyz;
        props_.reverb.late_reverb_gain = EffectProps::Reverb::default_late_reverb_gain;
        props_.reverb.late_reverb_delay = EffectProps::Reverb::default_late_reverb_delay;
        props_.reverb.late_reverb_pan[0] = EffectProps::Reverb::default_late_reverb_pan_xyz;
        props_.reverb.late_reverb_pan[1] = EffectProps::Reverb::default_late_reverb_pan_xyz;
        props_.reverb.late_reverb_pan[2] = EffectProps::Reverb::default_late_reverb_pan_xyz;
        props_.reverb.echo_time = EffectProps::Reverb::default_echo_time;
        props_.reverb.echo_depth = EffectProps::Reverb::default_echo_depth;
        props_.reverb.modulation_time = EffectProps::Reverb::default_modulation_time;
        props_.reverb.modulation_depth = EffectProps::Reverb::default_modulation_depth;
        props_.reverb.air_absorption_gain_hf = EffectProps::Reverb::default_air_absorption_gain_hf;
        props_.reverb.hf_reference = EffectProps::Reverb::default_hf_reference;
        props_.reverb.lf_reference = EffectProps::Reverb::default_lf_reference;
        props_.reverb.room_rolloff_factor = EffectProps::Reverb::default_room_rolloff_factor;
        props_.reverb.decay_hf_limit = EffectProps::Reverb::default_decay_hf_limit;
        break;

    case EffectType::chorus:
        props_.chorus.waveform = EffectProps::Chorus::default_waveform;
        props_.chorus.phase = EffectProps::Chorus::default_phase;
        props_.chorus.rate = EffectProps::Chorus::default_rate;
        props_.chorus.depth = EffectProps::Chorus::default_depth;
        props_.chorus.feedback = EffectProps::Chorus::default_feedback;
        props_.chorus.delay = EffectProps::Chorus::default_delay;
        break;

    case EffectType::compressor:
        props_.compressor.on_off = EffectProps::Compressor::default_on_off;
        break;

    case EffectType::distortion:
        props_.distortion.edge = EffectProps::Distortion::default_edge;
        props_.distortion.gain = EffectProps::Distortion::default_gain;
        props_.distortion.low_pass_cutoff = EffectProps::Distortion::default_low_pass_cutoff;
        props_.distortion.eq_center = EffectProps::Distortion::default_eq_center;
        props_.distortion.eq_bandwidth = EffectProps::Distortion::default_eq_bandwidth;
        break;

    case EffectType::echo:
        props_.echo.delay = EffectProps::Echo::default_delay;
        props_.echo.lr_delay = EffectProps::Echo::default_lr_delay;
        props_.echo.damping = EffectProps::Echo::default_damping;
        props_.echo.feedback = EffectProps::Echo::default_feedback;
        props_.echo.spread = EffectProps::Echo::default_spread;
        break;

    case EffectType::equalizer:
        props_.equalizer.low_cutoff = EffectProps::Equalizer::default_low_cutoff;
        props_.equalizer.low_gain = EffectProps::Equalizer::default_high_gain;
        props_.equalizer.mid1_center = EffectProps::Equalizer::default_mid1_center;
        props_.equalizer.mid1_gain = EffectProps::Equalizer::default_mid1_gain;
        props_.equalizer.mid1_width = EffectProps::Equalizer::default_mid1_width;
        props_.equalizer.mid2_center = EffectProps::Equalizer::default_mid2_center;
        props_.equalizer.mid2_gain = EffectProps::Equalizer::default_mid2_gain;
        props_.equalizer.mid2_width = EffectProps::Equalizer::default_mid2_width;
        props_.equalizer.high_cutoff = EffectProps::Equalizer::default_high_cutoff;
        props_.equalizer.high_gain = EffectProps::Equalizer::default_high_gain;
        break;

    case EffectType::flanger:
        props_.flanger.waveform = EffectProps::Flanger::default_waveform;
        props_.flanger.phase = EffectProps::Flanger::default_phase;
        props_.flanger.rate = EffectProps::Flanger::default_rate;
        props_.flanger.depth = EffectProps::Flanger::default_depth;
        props_.flanger.feedback = EffectProps::Flanger::default_feedback;
        props_.flanger.delay = EffectProps::Flanger::default_delay;
        break;

    case EffectType::ring_modulator:
        props_.modulator.frequency = EffectProps::Modulator::default_frequency;
        props_.modulator.high_pass_cutoff = EffectProps::Modulator::default_high_pass_cutoff;
        props_.modulator.waveform = EffectProps::Modulator::default_waveform;
        break;

    case EffectType::dedicated_dialog:
    case EffectType::dedicated_low_frequency:
        props_.dedicated.gain = EffectProps::Dedicated::default_gain;
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
    const EffectType type)
{
    switch (type)
    {
    case EffectType::null:
        return create_null();

    case EffectType::eax_reverb:
        return create_reverb();

    case EffectType::reverb:
        return create_reverb();

    case EffectType::chorus:
        return create_chorus();

    case EffectType::compressor:
        return create_compressor();

    case EffectType::distortion:
        return create_distortion();

    case EffectType::echo:
        return create_echo();

    case EffectType::equalizer:
        return create_equalizer();

    case EffectType::flanger:
        return create_flanger();

    case EffectType::ring_modulator:
        return create_modulator();

    case EffectType::dedicated_dialog:
    case EffectType::dedicated_low_frequency:
        return create_dedicated();

    default:
        return nullptr;
    }
}

// EffectStateFactory
// ==========================================================================
