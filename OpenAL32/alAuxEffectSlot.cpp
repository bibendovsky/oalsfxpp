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
#include "alAuxEffectSlot.h"
#include "alSource.h"


IEffect* create_chorus_effect();
IEffect* create_compressor_effect();
IEffect* create_dedicated_effect();
IEffect* create_distortion_effect();
IEffect* create_echo_effect();
IEffect* create_equalizer_effect();
IEffect* create_flanger_effect();
IEffect* create_modulator_effect();
IEffect* create_null_effect();
IEffect* create_reverb_effect();


EffectSlot::Effect::Effect()
    :
    type{AL_EFFECT_NULL},
    props{},
    state{}
{
}

EffectSlot::EffectSlot()
    :
    effect{},
    is_props_updated{},
    channel_count{},
    channel_map{},
    wet_buffer{SampleBuffers::size_type{max_effect_channels}}
{
    initialize();
}

EffectSlot::~EffectSlot()
{
    uninitialize();
}

void EffectSlot::initialize()
{
    uninitialize();

    effect.type = AL_EFFECT_NULL;
    effect.state = create_effect_state_by_type(AL_EFFECT_NULL);
    is_props_updated = true;
}

void EffectSlot::uninitialize()
{
    destroy_effect(effect.state);
}

void EffectSlot::initialize_effect(
    ALCdevice* device)
{
    if (effect.type != device->effect->type)
    {
        auto state = create_effect_state_by_type(device->effect->type);
        state->out_buffer = &device->dry.buffers;
        state->out_channels = device->dry.num_channels;
        state->update_device(device);

        effect.type = device->effect->type;
        effect.props = device->effect->props;

        destroy_effect(effect.state);
        effect.state = state;
    }
    else
    {
        effect.props = device->effect->props;
    }

    is_props_updated = true;
}

IEffect* EffectSlot::create_effect_state_by_type(
    const int type)
{
    switch (type)
    {
    case AL_EFFECT_NULL:
        return create_null_effect();

    case AL_EFFECT_EAXREVERB:
        return create_reverb_effect();

    case AL_EFFECT_REVERB:
        return create_reverb_effect();

    case AL_EFFECT_CHORUS:
        return create_chorus_effect();

    case AL_EFFECT_COMPRESSOR:
        return create_compressor_effect();

    case AL_EFFECT_DISTORTION:
        return create_distortion_effect();

    case AL_EFFECT_ECHO:
        return create_echo_effect();

    case AL_EFFECT_EQUALIZER:
        return create_equalizer_effect();

    case AL_EFFECT_FLANGER:
        return create_flanger_effect();

    case AL_EFFECT_RING_MODULATOR:
        return create_modulator_effect();

    case AL_EFFECT_DEDICATED_DIALOGUE:
        return create_dedicated_effect();

    case AL_EFFECT_DEDICATED_LOW_FREQUENCY_EFFECT:
        return create_dedicated_effect();

    default:
        return nullptr;
    }
}
