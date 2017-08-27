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

    if(newtype != EffectSlot->Effect.Type)
    {
        ALeffectStateFactory *factory;

        factory = getFactoryByType(newtype);
        if(!factory)
        {
            return AL_INVALID_ENUM;
        }
        State = V0(factory,create)();
        if(!State) return AL_OUT_OF_MEMORY;

        START_MIXER_MODE();
        State->OutBuffer = Device->Dry.Buffer;
        State->OutChannels = Device->Dry.NumChannels;
        if(V(State,deviceUpdate)(Device) == AL_FALSE)
        {
            LEAVE_MIXER_MODE();
            ALeffectState_DecRef(State);
            return AL_OUT_OF_MEMORY;
        }
        END_MIXER_MODE();

        if(!effect)
        {
            EffectSlot->Effect.Type = AL_EFFECT_NULL;
            memset(&EffectSlot->Effect.Props, 0, sizeof(EffectSlot->Effect.Props));
        }
        else
        {
            EffectSlot->Effect.Type = effect->type;
            EffectSlot->Effect.Props = effect->Props;
        }

        ALeffectState_DecRef(EffectSlot->Effect.State);
        EffectSlot->Effect.State = State;
    }
    else if(effect)
        EffectSlot->Effect.Props = effect->Props;

    /* Remove state references from old effect slot property updates. */
    props = EffectSlot->FreeList;
    while(props)
    {
        if(props->State)
            ALeffectState_DecRef(props->State);
        props->State = NULL;
        props = props->next;
    }

    return AL_NO_ERROR;
}


static void ALeffectState_IncRef(ALeffectState *state)
{
    unsigned int ref;
    ref = ++state->Ref;
}

static void ALeffectState_DecRef(ALeffectState *state)
{
    unsigned int ref;
    ref = --state->Ref;
    if(ref == 0) DELETE_OBJ(state);
}


void ALeffectState_Construct(ALeffectState *state)
{
    state->Ref = 1;

    state->OutBuffer = NULL;
    state->OutChannels = 0;
}

void ALeffectState_Destruct(ALeffectState *UNUSED(state))
{
}

ALenum InitEffectSlot(ALeffectslot *slot)
{
    ALeffectStateFactory *factory;

    slot->Effect.Type = AL_EFFECT_NULL;

    factory = getFactoryByType(AL_EFFECT_NULL);
    if(!(slot->Effect.State=V0(factory,create)()))
        return AL_OUT_OF_MEMORY;

    slot->ref = 0;

    slot->Update = NULL;
    slot->FreeList = NULL;

    ALeffectState_IncRef(slot->Effect.State);
    slot->Params.EffectState = slot->Effect.State;

    return AL_NO_ERROR;
}

void DeinitEffectSlot(ALeffectslot *slot)
{
    struct ALeffectslotProps *props;
    size_t count = 0;

    props = slot->Update;
    if(props)
    {
        if(props->State) ALeffectState_DecRef(props->State);
        al_free(props);
    }
    props = slot->FreeList;
    while(props)
    {
        struct ALeffectslotProps *next = props->next;
        if(props->State) ALeffectState_DecRef(props->State);
        al_free(props);
        props = next;
        ++count;
    }

    ALeffectState_DecRef(slot->Effect.State);
    if(slot->Params.EffectState)
        ALeffectState_DecRef(slot->Params.EffectState);
}

void UpdateEffectSlotProps(ALeffectslot *slot)
{
    struct ALeffectslotProps *props;
    struct ALeffectslotProps *temp_props;
    ALeffectState *oldstate;

    /* Get an unused property container, or allocate a new one as needed. */
    props = slot->FreeList;
    if(!props)
        props = al_calloc(16, sizeof(*props));
    else
    {
        struct ALeffectslotProps *next;
        do {
            next = props->next;
        } while((slot->FreeList == props ? (slot->FreeList = next, true) : (props = slot->FreeList, false)) == 0);
    }

    /* Copy in current property values. */
    props->Type = slot->Effect.Type;
    props->Props = slot->Effect.Props;
    /* Swap out any stale effect state object there may be in the container, to
     * delete it.
     */
    ALeffectState_IncRef(slot->Effect.State);
    oldstate = props->State;
    props->State = slot->Effect.State;

    /* Set the new container for updating internal parameters. */
    temp_props = props;
    props = slot->Update;
    slot->Update = temp_props;

    if(props)
    {
        /* If there was an unused update container, put it back in the
         * freelist.
         */
        props->next = slot->FreeList;
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
