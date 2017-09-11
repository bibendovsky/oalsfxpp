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
        props_.reverb_.density_ = EffectProps::Reverb::default_density;
        props_.reverb_.diffusion_ = EffectProps::Reverb::default_diffusion;
        props_.reverb_.gain_ = EffectProps::Reverb::default_gain;
        props_.reverb_.gain_hf_ = EffectProps::Reverb::default_gain_hf;
        props_.reverb_.gain_lf_ = EffectProps::Reverb::default_gain_lf;
        props_.reverb_.decay_time_ = EffectProps::Reverb::default_decay_time;
        props_.reverb_.decay_hf_ratio_ = EffectProps::Reverb::default_decay_hf_ratio;
        props_.reverb_.decay_lf_ratio_ = EffectProps::Reverb::default_decay_lf_ratio;
        props_.reverb_.reflections_gain_ = EffectProps::Reverb::default_reflections_gain;
        props_.reverb_.reflections_delay_ = EffectProps::Reverb::default_reflections_delay;
        props_.reverb_.reflections_pan_[0] = EffectProps::Reverb::default_reflections_pan_xyz;
        props_.reverb_.reflections_pan_[1] = EffectProps::Reverb::default_reflections_pan_xyz;
        props_.reverb_.reflections_pan_[2] = EffectProps::Reverb::default_reflections_pan_xyz;
        props_.reverb_.late_reverb_gain_ = EffectProps::Reverb::default_late_reverb_gain;
        props_.reverb_.late_reverb_delay_ = EffectProps::Reverb::default_late_reverb_delay;
        props_.reverb_.late_reverb_pan_[0] = EffectProps::Reverb::default_late_reverb_pan_xyz;
        props_.reverb_.late_reverb_pan_[1] = EffectProps::Reverb::default_late_reverb_pan_xyz;
        props_.reverb_.late_reverb_pan_[2] = EffectProps::Reverb::default_late_reverb_pan_xyz;
        props_.reverb_.echo_time_ = EffectProps::Reverb::default_echo_time;
        props_.reverb_.echo_depth_ = EffectProps::Reverb::default_echo_depth;
        props_.reverb_.modulation_time_ = EffectProps::Reverb::default_modulation_time;
        props_.reverb_.modulation_depth_ = EffectProps::Reverb::default_modulation_depth;
        props_.reverb_.air_absorption_gain_hf_ = EffectProps::Reverb::default_air_absorption_gain_hf;
        props_.reverb_.hf_reference_ = EffectProps::Reverb::default_hf_reference;
        props_.reverb_.lf_reference_ = EffectProps::Reverb::default_lf_reference;
        props_.reverb_.room_rolloff_factor_ = EffectProps::Reverb::default_room_rolloff_factor;
        props_.reverb_.decay_hf_limit_ = EffectProps::Reverb::default_decay_hf_limit;
        break;

    case EffectType::chorus:
        props_.chorus_.waveform_ = EffectProps::Chorus::default_waveform;
        props_.chorus_.phase_ = EffectProps::Chorus::default_phase;
        props_.chorus_.rate_ = EffectProps::Chorus::default_rate;
        props_.chorus_.depth_ = EffectProps::Chorus::default_depth;
        props_.chorus_.feedback_ = EffectProps::Chorus::default_feedback;
        props_.chorus_.delay_ = EffectProps::Chorus::default_delay;
        break;

    case EffectType::compressor:
        props_.compressor_.on_off_ = EffectProps::Compressor::default_on_off;
        break;

    case EffectType::distortion:
        props_.distortion_.edge_ = EffectProps::Distortion::default_edge;
        props_.distortion_.gain_ = EffectProps::Distortion::default_gain;
        props_.distortion_.low_pass_cutoff_ = EffectProps::Distortion::default_low_pass_cutoff;
        props_.distortion_.eq_center_ = EffectProps::Distortion::default_eq_center;
        props_.distortion_.eq_bandwidth_ = EffectProps::Distortion::default_eq_bandwidth;
        break;

    case EffectType::echo:
        props_.echo_.delay_ = EffectProps::Echo::default_delay;
        props_.echo_.lr_delay_ = EffectProps::Echo::default_lr_delay;
        props_.echo_.damping_ = EffectProps::Echo::default_damping;
        props_.echo_.feedback_ = EffectProps::Echo::default_feedback;
        props_.echo_.spread_ = EffectProps::Echo::default_spread;
        break;

    case EffectType::equalizer:
        props_.equalizer_.low_cutoff_ = EffectProps::Equalizer::default_low_cutoff;
        props_.equalizer_.low_gain_ = EffectProps::Equalizer::default_high_gain;
        props_.equalizer_.mid1_center_ = EffectProps::Equalizer::default_mid1_center;
        props_.equalizer_.mid1_gain_ = EffectProps::Equalizer::default_mid1_gain;
        props_.equalizer_.mid1_width_ = EffectProps::Equalizer::default_mid1_width;
        props_.equalizer_.mid2_center_ = EffectProps::Equalizer::default_mid2_center;
        props_.equalizer_.mid2_gain_ = EffectProps::Equalizer::default_mid2_gain;
        props_.equalizer_.mid2_width_ = EffectProps::Equalizer::default_mid2_width;
        props_.equalizer_.high_cutoff_ = EffectProps::Equalizer::default_high_cutoff;
        props_.equalizer_.high_gain_ = EffectProps::Equalizer::default_high_gain;
        break;

    case EffectType::flanger:
        props_.flanger_.waveform_ = EffectProps::Flanger::default_waveform;
        props_.flanger_.phase_ = EffectProps::Flanger::default_phase;
        props_.flanger_.rate_ = EffectProps::Flanger::default_rate;
        props_.flanger_.depth_ = EffectProps::Flanger::default_depth;
        props_.flanger_.feedback_ = EffectProps::Flanger::default_feedback;
        props_.flanger_.delay_ = EffectProps::Flanger::default_delay;
        break;

    case EffectType::ring_modulator:
        props_.modulator_.frequency_ = EffectProps::Modulator::default_frequency;
        props_.modulator_.high_pass_cutoff_ = EffectProps::Modulator::default_high_pass_cutoff;
        props_.modulator_.waveform_ = EffectProps::Modulator::default_waveform;
        break;

    case EffectType::dedicated_dialog:
    case EffectType::dedicated_low_frequency:
        props_.dedicated_.gain_ = EffectProps::Dedicated::default_gain;
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
    dst_buffers_{},
    dst_channel_count_{}
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
