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


static IEffect* createByType(
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

void initialize_effect(
    ALCdevice* device,
    ALeffectslot* effect_slot,
    ALeffect* effect)
{
    if(effect->type != effect_slot->effect.type)
    {
        auto state = createByType(effect->type);
        state->out_buffer = &device->dry.buffers;
        state->out_channels = device->dry.num_channels;
        state->update_device(device);

        effect_slot->effect.type = effect->type;
        effect_slot->effect.props = effect->props;

        destroy_effect(effect_slot->effect.state);
        effect_slot->effect.state = state;
    }
    else if (effect)
    {
        effect_slot->effect.props = effect->props;
    }

    // Remove state references from old effect slot property updates.
    auto props = effect_slot->props;

    if (props)
    {
        props->state = nullptr;
    }
}

void init_effect_slot(
    ALeffectslot* slot)
{
    slot->effect.type = AL_EFFECT_NULL;
    slot->effect.state = createByType(AL_EFFECT_NULL);
    slot->update = nullptr;
    slot->props = nullptr;
    slot->params.effect_state = slot->effect.state;
}

void deinit_effect_slot(
    ALeffectslot* slot)
{
    auto props = slot->update;

    if (props)
    {
        delete props;
    }

    props = slot->props;

    if (props)
    {
        delete props;
    }

    destroy_effect(slot->effect.state);
}

void update_effect_slot_props(
    ALeffectslot* slot)
{
    // Get an unused property container, or allocate a new one as needed.
    auto props = slot->props;

    if (!props)
    {
        props = new ALeffectslotProps{};
    }

    // Copy in current property values.
    props->type = slot->effect.type;
    props->props = slot->effect.props;

    // Swap out any stale effect state object there may be in the container, to
    // delete it.
    props->state = slot->effect.state;

    // Set the new container for updating internal parameters.
    slot->update = props;
}

void update_all_effect_slot_props(
    ALCdevice* device)
{
    auto slot = device->effect_slot;
    update_effect_slot_props(slot);
}
