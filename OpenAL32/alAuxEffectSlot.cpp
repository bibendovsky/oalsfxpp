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
    const ALenum type)
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

static void ALeffectState_IncRef(ALeffectState *state);
static void ALeffectState_DecRef(ALeffectState *state);


ALenum InitializeEffect(ALCdevice *Device, ALeffectslot *EffectSlot, ALeffect *effect)
{
    ALenum newtype = (effect ? effect->type : AL_EFFECT_NULL);
    struct ALeffectslotProps *props;
    IEffect *State;

    if(newtype != EffectSlot->effect.type)
    {
        State = createByType(newtype);
        if(!State) return AL_OUT_OF_MEMORY;

        State->out_buffer = Device->dry.buffer;
        State->out_channels = Device->dry.num_channels;
        if(State->update_device(Device) == AL_FALSE)
        {
            return AL_OUT_OF_MEMORY;
        }

        if(!effect)
        {
            EffectSlot->effect.type = AL_EFFECT_NULL;
            memset(&EffectSlot->effect.props, 0, sizeof(EffectSlot->effect.props));
        }
        else
        {
            EffectSlot->effect.type = effect->type;
            EffectSlot->effect.props = effect->props;
        }

        destroy_effect(EffectSlot->effect.state);
        EffectSlot->effect.state = State;
    }
    else if(effect)
        EffectSlot->effect.props = effect->props;

    /* Remove state references from old effect slot property updates. */
    props = EffectSlot->free_list;
    while(props)
    {
        props->state = NULL;
        props = props->next;
    }

    return AL_NO_ERROR;
}


static void ALeffectState_IncRef(ALeffectState *state)
{
    unsigned int ref;
    ref = ++state->ref;
}

static void ALeffectState_DecRef(ALeffectState *state)
{
    unsigned int ref;
    ref = --state->ref;
    if(ref == 0) DELETE_OBJ(state);
}


void ALeffectState_Construct(ALeffectState *state)
{
    state->ref = 1;

    state->out_buffer = NULL;
    state->out_channels = 0;
}

void ALeffectState_Destruct(ALeffectState *state)
{
}

ALenum InitEffectSlot(ALeffectslot *slot)
{
    slot->effect.type = AL_EFFECT_NULL;

    if(!(slot->effect.state= createByType(AL_EFFECT_NULL)))
        return AL_OUT_OF_MEMORY;

    slot->ref = 0;

    slot->update = NULL;
    slot->free_list = NULL;

    slot->params.effect_state = slot->effect.state;

    return AL_NO_ERROR;
}

void DeinitEffectSlot(ALeffectslot *slot)
{
    struct ALeffectslotProps *props;
    size_t count = 0;

    props = slot->update;
    if(props)
    {
        al_free(props);
    }
    props = slot->free_list;
    while(props)
    {
        struct ALeffectslotProps *next = props->next;
        al_free(props);
        props = next;
        ++count;
    }

    destroy_effect(slot->effect.state);
}

void UpdateEffectSlotProps(ALeffectslot *slot)
{
    struct ALeffectslotProps *props;
    struct ALeffectslotProps *temp_props;
    IEffect *oldstate;

    /* Get an unused property container, or allocate a new one as needed. */
    props = slot->free_list;
    if(!props)
        props = static_cast<ALeffectslotProps*>(al_calloc(16, sizeof(*props)));
    else
    {
        struct ALeffectslotProps *next;
        do {
            next = props->next;
        } while((slot->free_list == props ? (slot->free_list = next, true) : (props = slot->free_list, false)) == 0);
    }

    /* Copy in current property values. */
    props->type = slot->effect.type;
    props->props = slot->effect.props;
    /* Swap out any stale effect state object there may be in the container, to
     * delete it.
     */
    oldstate = props->state;
    props->state = slot->effect.state;

    /* Set the new container for updating internal parameters. */
    temp_props = props;
    props = slot->update;
    slot->update = temp_props;

    if(props)
    {
        /* If there was an unused update container, put it back in the
         * freelist.
         */
        props->next = slot->free_list;
    }
}

void UpdateAllEffectSlotProps(ALCcontext *context)
{
    struct ALeffectslotArray *auxslots;
    ALsizei i;

    auxslots = context->active_aux_slots;
    for(i = 0;i < auxslots->count;i++)
    {
        ALeffectslot *slot = auxslots->slot[i];
        UpdateEffectSlotProps(slot);
    }
}
