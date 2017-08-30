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

#include "AL/al.h"
#include "AL/alc.h"
#include "alMain.h"
#include "alAuxEffectSlot.h"
#include "alSource.h"

#include "almalloc.h"


static inline ALeffectStateFactory *getFactoryByType(ALenum type)
{
    ALeffectStateFactory* (*get_factory)() = NULL;

    switch (type)
    {
    case AL_EFFECT_NULL:
        get_factory = ALnullStateFactory_getFactory;
        break;

    case AL_EFFECT_EAXREVERB:
        get_factory = ALreverbStateFactory_getFactory;
        break;

    case AL_EFFECT_REVERB:
        get_factory = ALreverbStateFactory_getFactory;
        break;

    case AL_EFFECT_CHORUS:
        get_factory = ALchorusStateFactory_getFactory;
        break;

    case AL_EFFECT_COMPRESSOR:
        get_factory = ALcompressorStateFactory_getFactory;
        break;

    case AL_EFFECT_DISTORTION:
        get_factory = ALdistortionStateFactory_getFactory;
        break;

    case AL_EFFECT_ECHO:
        get_factory = ALechoStateFactory_getFactory;
        break;

    case AL_EFFECT_EQUALIZER:
        get_factory = ALequalizerStateFactory_getFactory;
        break;

    case AL_EFFECT_FLANGER:
        get_factory = ALflangerStateFactory_getFactory;
        break;

    case AL_EFFECT_RING_MODULATOR:
        get_factory = ALmodulatorStateFactory_getFactory;
        break;

    case AL_EFFECT_DEDICATED_DIALOGUE:
        get_factory = ALdedicatedStateFactory_getFactory;
        break;

    case AL_EFFECT_DEDICATED_LOW_FREQUENCY_EFFECT:
        get_factory = ALdedicatedStateFactory_getFactory;
        break;

    default:
        return NULL;
    }

    return get_factory();
}

static void ALeffectState_IncRef(ALeffectState *state);
static void ALeffectState_DecRef(ALeffectState *state);


ALenum InitializeEffect(ALCdevice *Device, ALeffectslot *EffectSlot, ALeffect *effect)
{
    ALenum newtype = (effect ? effect->type : AL_EFFECT_NULL);
    struct ALeffectslotProps *props;
    ALeffectState *State;

    if(newtype != EffectSlot->effect.type)
    {
        ALeffectStateFactory *factory;

        factory = getFactoryByType(newtype);
        if(!factory)
        {
            return AL_INVALID_ENUM;
        }
        State = V0(factory,create)();
        if(!State) return AL_OUT_OF_MEMORY;

        State->out_buffer = Device->Dry.Buffer;
        State->out_channels = Device->Dry.NumChannels;
        if(V(State,device_update)(Device) == AL_FALSE)
        {
            ALeffectState_DecRef(State);
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

        ALeffectState_DecRef(EffectSlot->effect.state);
        EffectSlot->effect.state = State;
    }
    else if(effect)
        EffectSlot->effect.props = effect->props;

    /* Remove state references from old effect slot property updates. */
    props = EffectSlot->free_list;
    while(props)
    {
        if(props->state)
            ALeffectState_DecRef(props->state);
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

void ALeffectState_Destruct(ALeffectState *UNUSED(state))
{
}

ALenum InitEffectSlot(ALeffectslot *slot)
{
    ALeffectStateFactory *factory;

    slot->effect.type = AL_EFFECT_NULL;

    factory = getFactoryByType(AL_EFFECT_NULL);
    if(!(slot->effect.state=V0(factory,create)()))
        return AL_OUT_OF_MEMORY;

    slot->ref = 0;

    slot->update = NULL;
    slot->free_list = NULL;

    ALeffectState_IncRef(slot->effect.state);
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
        if(props->state) ALeffectState_DecRef(props->state);
        al_free(props);
    }
    props = slot->free_list;
    while(props)
    {
        struct ALeffectslotProps *next = props->next;
        if(props->state) ALeffectState_DecRef(props->state);
        al_free(props);
        props = next;
        ++count;
    }

    ALeffectState_DecRef(slot->effect.state);
    if(slot->params.effect_state)
        ALeffectState_DecRef(slot->params.effect_state);
}

void UpdateEffectSlotProps(ALeffectslot *slot)
{
    struct ALeffectslotProps *props;
    struct ALeffectslotProps *temp_props;
    ALeffectState *oldstate;

    /* Get an unused property container, or allocate a new one as needed. */
    props = slot->free_list;
    if(!props)
        props = al_calloc(16, sizeof(*props));
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
    ALeffectState_IncRef(slot->effect.state);
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

    if(oldstate)
        ALeffectState_DecRef(oldstate);
}

void UpdateAllEffectSlotProps(ALCcontext *context)
{
    struct ALeffectslotArray *auxslots;
    ALsizei i;

    auxslots = context->ActiveAuxSlots;
    for(i = 0;i < auxslots->count;i++)
    {
        ALeffectslot *slot = auxslots->slot[i];
        UpdateEffectSlotProps(slot);
    }
}
