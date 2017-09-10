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
    effect.state = EffectStateFactory::create_by_type(AL_EFFECT_NULL);
    is_props_updated = true;
}

void EffectSlot::uninitialize()
{
    EffectState::destroy(effect.state);
}

void EffectSlot::initialize_effect(
    ALCdevice* device)
{
    if (effect.type != device->effect->type_)
    {
        auto state = EffectStateFactory::create_by_type(device->effect->type_);
        state->out_buffer = &device->dry.buffers;
        state->out_channels = device->dry.num_channels;
        state->update_device(device);

        effect.type = device->effect->type_;
        effect.props = device->effect->props_;

        EffectState::destroy(effect.state);
        effect.state = state;
    }
    else
    {
        effect.props = device->effect->props_;
    }

    is_props_updated = true;
}
