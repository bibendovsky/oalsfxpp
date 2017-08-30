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

#include "version.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <ctype.h>
#include <signal.h>

#include "alMain.h"
#include "alSource.h"
#include "alSource.h"
#include "alBuffer.h"
#include "alAuxEffectSlot.h"
#include "bformatdec.h"
#include "alu.h"

#include "almalloc.h"


void InitSourceParams(ALsource *Source, ALsizei num_sends);
void DeinitSource(ALsource *source, ALsizei num_sends);


/************************************************
 * Global variables
 ************************************************/

/* Default is always the first in the list */
static ALCchar *alcDefaultAllDevicesSpecifier;

static ALCenum LastNullDeviceError = ALC_NO_ERROR;

/* Thread-local current context */
static ALCcontext* LocalContext;
/* Process-wide current context */
static ALCcontext* GlobalContext = NULL;

/* One-time configuration init control */
static ALCboolean alc_config_once = ALC_FALSE;

/* Default effect that applies to sources that don't have an effect on send 0 */
static ALeffect DefaultEffect;

/* Flag to specify if alcSuspendContext/alcProcessContext should defer/process
 * updates.
 */
static ALCboolean SuspendDefers = ALC_TRUE;


/************************************************
 * Device lists
 ************************************************/
static ALCdevice* DeviceList = NULL;


/************************************************
 * Library initialization
 ************************************************/
static void alc_initconfig(void)
{
    InitEffect(&DefaultEffect);
}
void DO_INITCONFIG()
{
    if (!alc_config_once)
    {
        alc_config_once = ALC_TRUE;
        alc_initconfig();
    }
}


/************************************************
 * Miscellaneous ALC helpers
 ************************************************/

/* SetDefaultWFXChannelOrder
 *
 * Sets the default channel order used by WaveFormatEx.
 */
void SetDefaultWFXChannelOrder(ALCdevice *device)
{
    ALsizei i;

    for(i = 0;i < MAX_OUTPUT_CHANNELS;i++)
        device->real_out.channel_name[i] = InvalidChannel;

    switch(device->fmt_chans)
    {
    case DevFmtMono:
        device->real_out.channel_name[0] = FrontCenter;
        break;
    case DevFmtStereo:
        device->real_out.channel_name[0] = FrontLeft;
        device->real_out.channel_name[1] = FrontRight;
        break;
    case DevFmtQuad:
        device->real_out.channel_name[0] = FrontLeft;
        device->real_out.channel_name[1] = FrontRight;
        device->real_out.channel_name[2] = BackLeft;
        device->real_out.channel_name[3] = BackRight;
        break;
    case DevFmtX51:
        device->real_out.channel_name[0] = FrontLeft;
        device->real_out.channel_name[1] = FrontRight;
        device->real_out.channel_name[2] = FrontCenter;
        device->real_out.channel_name[3] = LFE;
        device->real_out.channel_name[4] = SideLeft;
        device->real_out.channel_name[5] = SideRight;
        break;
    case DevFmtX51Rear:
        device->real_out.channel_name[0] = FrontLeft;
        device->real_out.channel_name[1] = FrontRight;
        device->real_out.channel_name[2] = FrontCenter;
        device->real_out.channel_name[3] = LFE;
        device->real_out.channel_name[4] = BackLeft;
        device->real_out.channel_name[5] = BackRight;
        break;
    case DevFmtX61:
        device->real_out.channel_name[0] = FrontLeft;
        device->real_out.channel_name[1] = FrontRight;
        device->real_out.channel_name[2] = FrontCenter;
        device->real_out.channel_name[3] = LFE;
        device->real_out.channel_name[4] = BackCenter;
        device->real_out.channel_name[5] = SideLeft;
        device->real_out.channel_name[6] = SideRight;
        break;
    case DevFmtX71:
        device->real_out.channel_name[0] = FrontLeft;
        device->real_out.channel_name[1] = FrontRight;
        device->real_out.channel_name[2] = FrontCenter;
        device->real_out.channel_name[3] = LFE;
        device->real_out.channel_name[4] = BackLeft;
        device->real_out.channel_name[5] = BackRight;
        device->real_out.channel_name[6] = SideLeft;
        device->real_out.channel_name[7] = SideRight;
        break;
    }
}

extern inline ALint GetChannelIndex(const enum Channel names[MAX_OUTPUT_CHANNELS], enum Channel chan);


/* UpdateDeviceParams
 *
 * Updates device parameters according to the attribute list (caller is
 * responsible for holding the list lock).
 */
static ALCenum UpdateDeviceParams(ALCdevice *device, const ALCint *attrList)
{
    const ALsizei old_sends = device->num_aux_sends;
    ALsizei new_sends = device->num_aux_sends;
    ALboolean update_failed;
    ALCcontext *context;
    size_t size;

    al_free(device->dry.buffer);
    device->dry.buffer = NULL;
    device->dry.num_channels = 0;
    device->foa_out.buffer = NULL;
    device->foa_out.num_channels = 0;
    device->real_out.buffer = NULL;
    device->real_out.num_channels = 0;

    aluInitRenderer(device);

    /* Allocate extra channels for any post-filter output. */
    size = (device->dry.num_channels + device->foa_out.num_channels +
            device->real_out.num_channels)*sizeof(device->dry.buffer[0]);

    device->dry.buffer = al_calloc(16, size);
    if(!device->dry.buffer)
    {
        return ALC_INVALID_DEVICE;
    }

    if(device->real_out.num_channels != 0)
        device->real_out.buffer = device->dry.buffer + device->dry.num_channels +
                                 device->foa_out.num_channels;
    else
    {
        device->real_out.buffer = device->dry.buffer;
        device->real_out.num_channels = device->dry.num_channels;
    }

    if(device->foa_out.num_channels != 0)
        device->foa_out.buffer = device->dry.buffer + device->dry.num_channels;
    else
    {
        device->foa_out.buffer = device->dry.buffer;
        device->foa_out.num_channels = device->dry.num_channels;
    }

    device->num_aux_sends = new_sends;

    /* Need to delay returning failure until replacement Send arrays have been
     * allocated with the appropriate size.
     */
    update_failed = AL_FALSE;
    context = device->context;
    if(context)
    {
        ALsizei pos;

        {
            ALeffectslot *slot = device->effect_slot;
            ALeffectState *state = slot->effect.state;

            state->out_buffer = device->dry.buffer;
            state->out_channels = device->dry.num_channels;
            if(V(state,device_update)(device) == AL_FALSE)
                update_failed = AL_TRUE;
            else
                UpdateEffectSlotProps(slot);
        }

        {
            ALsource *source = device->source;

            if(old_sends != device->num_aux_sends)
            {
                ALvoid *sends = al_calloc(16, device->num_aux_sends*sizeof(source->send[0]));
                ALsizei s;

                memcpy(sends, source->send,
                    mini(device->num_aux_sends, old_sends)*sizeof(source->send[0])
                );
                for(s = device->num_aux_sends;s < old_sends;s++)
                {
                    if(source->send[s].slot)
                        source->send[s].slot->ref -= 1;
                    source->send[s].slot = NULL;
                }
                al_free(source->send);
                source->send = sends;
                for(s = old_sends;s < device->num_aux_sends;s++)
                {
                    source->send[s].slot = NULL;
                    source->send[s].gain = 1.0f;
                    source->send[s].gain_hf = 1.0f;
                    source->send[s].hf_reference = LOWPASSFREQREF;
                    source->send[s].gain_lf = 1.0f;
                    source->send[s].lf_reference = HIGHPASSFREQREF;
                }
            }
        }
        AllocateVoices(context, 1, old_sends);
        for(pos = 0;pos < context->voice_count;pos++)
        {
            ALvoice *voice = context->voice;

            if(voice->source == NULL)
                continue;
        }

        UpdateAllSourceProps(context);
    }
    if(update_failed)
        return ALC_INVALID_DEVICE;

    return ALC_NO_ERROR;
}

/* FreeDevice
 *
 * Frees the device structure, and destroys any objects the app failed to
 * delete. Called once there's no more references on the device.
 */
static ALCvoid FreeDevice(ALCdevice *device)
{
    al_free(device->effect);

    DeinitEffectSlot(device->effect_slot);
    al_free(device->effect_slot);

    DeinitSource(device->source, device->num_aux_sends);
    al_free(device->source);

    al_free(device->dry.buffer);
    device->dry.buffer = NULL;
    device->dry.num_channels = 0;
    device->foa_out.buffer = NULL;
    device->foa_out.num_channels = 0;
    device->real_out.buffer = NULL;
    device->real_out.num_channels = 0;

    al_free(device);
}


void ALCdevice_IncRef(ALCdevice *device)
{
    unsigned int ref;
    ref = ++device->ref;
}

void ALCdevice_DecRef(ALCdevice *device)
{
    unsigned int ref;
    ref = --device->ref;
    if(ref == 0) FreeDevice(device);
}

/* VerifyDevice
 *
 * Checks if the device handle is valid, and increments its ref count if so.
 */
static ALCboolean VerifyDevice(ALCdevice **device)
{
    ALCdevice *tmpDevice;

    tmpDevice = DeviceList;

    if(tmpDevice)
    {
        if(tmpDevice == *device)
        {
            ALCdevice_IncRef(tmpDevice);
            return ALC_TRUE;
        }
    }

    *device = NULL;
    return ALC_FALSE;
}


/* InitContext
 *
 * Initializes context fields
 */
static ALvoid InitContext(ALCcontext *Context)
{
    struct ALeffectslotArray *auxslots;

    auxslots = al_calloc(DEF_ALIGN, FAM_SIZE(struct ALeffectslotArray, slot, 1));
    auxslots->count = 1;
    auxslots->slot[0] = Context->device->effect_slot;

    Context->active_aux_slots = auxslots;
}


/* FreeContext
 *
 * Cleans up the context, and destroys any remaining objects the app failed to
 * delete. Called once there's no more references on the context.
 */
static void FreeContext(ALCcontext *context)
{
    struct ALeffectslotArray *auxslots;
    size_t count;
    ALsizei i;

    auxslots = context->active_aux_slots;
    context->active_aux_slots = NULL;
    al_free(auxslots);

    for(i = 0;i < context->voice_count;i++)
        DeinitVoice(context->voice);
    al_free(context->voice);
    context->voice = NULL;
    context->voice_count = 0;

    count = 0;

    ALCdevice_DecRef(context->device);
    context->device = NULL;

    //Invalidate context
    memset(context, 0, sizeof(ALCcontext));
    al_free(context);
}

/* ReleaseContext
 *
 * Removes the context reference from the given device and removes it from
 * being current on the running thread or globally. Returns true if other
 * contexts still exist on the device.
 */
static bool ReleaseContext(ALCcontext *context, ALCdevice *device)
{
    ALCcontext *origctx, *newhead;
    bool ret = true;

    origctx = context;
    if(GlobalContext == origctx ? (GlobalContext = NULL, true) : (origctx = GlobalContext, false))
        ALCcontext_DecRef(context);

    origctx = context;
    newhead = NULL;
    if(device->context == origctx ? (device->context = newhead, true) : (origctx = device->context, false))
    {
        ret = !!newhead;
    }

    ALCcontext_DecRef(context);
    return ret;
}

void ALCcontext_IncRef(ALCcontext *context)
{
    unsigned int ref = ++context->ref;
}

void ALCcontext_DecRef(ALCcontext *context)
{
    unsigned int ref = --context->ref;
    if(ref == 0) FreeContext(context);
}

/* VerifyContext
 *
 * Checks that the given context is valid, and increments its reference count.
 */
static ALCboolean VerifyContext(ALCcontext **context)
{
    ALCdevice *dev;

    dev = DeviceList;
    if(dev)
    {
        ALCcontext *ctx = dev->context;
        if(ctx)
        {
            if(ctx == *context)
            {
                ALCcontext_IncRef(ctx);
                return ALC_TRUE;
            }
        }
    }

    *context = NULL;
    return ALC_FALSE;
}


/* GetContextRef
 *
 * Returns the currently active context for this thread, and adds a reference
 * without locking it.
 */
ALCcontext *GetContextRef(void)
{
    ALCcontext *context;

    context = LocalContext;
    if(context)
        ALCcontext_IncRef(context);
    else
    {
        context = GlobalContext;
        if(context)
            ALCcontext_IncRef(context);
    }

    return context;
}


void AllocateVoices(ALCcontext *context, ALsizei num_voices, ALsizei old_sends)
{
    ALCdevice *device = context->device;
    ALsizei num_sends = device->num_aux_sends;
    struct ALvoiceProps *props;
    size_t sizeof_props;
    size_t sizeof_voice;
    ALvoice *voice;
    ALsizei v = 0;
    size_t size;

    if (context->voice)
    {
        return;
    }

    /* Allocate the voice pointers, voices, and the voices' stored source
     * property set (including the dynamically-sized Send[] array) in one
     * chunk.
     */
    sizeof_voice = RoundUp(FAM_SIZE(ALvoice, send, num_sends), 16);
    sizeof_props = RoundUp(FAM_SIZE(struct ALvoiceProps, send, num_sends), 16);
    size = sizeof_voice + sizeof_props;

    voice = al_calloc(16, RoundUp(size*1, 16));
    /* The voice and property objects are stored interleaved since they're
     * paired together.
     */
    props = (struct ALvoiceProps*)((char*)voice + sizeof_voice);

    /* Finish setting the voices' property set pointers and references. */
    voice->props = props;

    al_free(context->voice);
    context->voice = voice;
    context->voice_count = 1;
}


/************************************************
 * Standard ALC functions
 ************************************************/

/* alcCreateContext
 *
 * Create and attach a context to the given device.
 */
ALC_API ALCcontext* ALC_APIENTRY alcCreateContext(ALCdevice *device, const ALCint *attrList)
{
    ALCcontext *ALContext;
    ALCenum err;

    /* Explicitly hold the list lock while taking the BackendLock in case the
     * device is asynchronously destropyed, to ensure this new context is
     * properly cleaned up after being made.
     */
    if(!VerifyDevice(&device))
    {
        if(device) ALCdevice_DecRef(device);
        return NULL;
    }

    if (device->context)
    {
        return NULL;
    }

    if(DefaultEffect.type != AL_EFFECT_NULL)
        ALContext = al_calloc(16, sizeof(ALCcontext)+sizeof(ALeffectslot));
    else
        ALContext = al_calloc(16, sizeof(ALCcontext));
    if(!ALContext)
    {
        ALCdevice_DecRef(device);
        return NULL;
    }

    ALContext->ref = 1;

    ALContext->voice = NULL;
    ALContext->voice_count = 0;
    ALContext->active_aux_slots = NULL;
    ALContext->device = device;

    if((err=UpdateDeviceParams(device, attrList)) != ALC_NO_ERROR)
    {
        al_free(ALContext);
        ALContext = NULL;

        if(err == ALC_INVALID_DEVICE)
        {
            aluHandleDisconnect(device);
        }
        ALCdevice_DecRef(device);
        return NULL;
    }
    AllocateVoices(ALContext, 1, device->num_aux_sends);

    ALCdevice_IncRef(ALContext->device);
    InitContext(ALContext);

    device->context = ALContext;

    ALCdevice_DecRef(device);

    return ALContext;
}

/* alcDestroyContext
 *
 * Remove a context from its device
 */
ALC_API ALCvoid ALC_APIENTRY alcDestroyContext(ALCcontext *context)
{
    ALCdevice *Device;

    if(!VerifyContext(&context))
    {
        return;
    }

    Device = context->device;
    if(Device)
    {
        ReleaseContext(context, Device);
    }

    ALCcontext_DecRef(context);
}

/* alcMakeContextCurrent
 *
 * Makes the given context the active process-wide context, and removes the
 * thread-local context for the calling thread.
 */
ALC_API ALCboolean ALC_APIENTRY alcMakeContextCurrent(ALCcontext *context)
{
    ALCcontext* temp_context;

    /* context must be valid or NULL */
    if(context && !VerifyContext(&context))
    {
        return ALC_FALSE;
    }
    /* context's reference count is already incremented */
    temp_context = context;
    context = GlobalContext;
    GlobalContext = temp_context;
    if(context) ALCcontext_DecRef(context);

    if((context=LocalContext) != NULL)
    {
        LocalContext = NULL;
        ALCcontext_DecRef(context);
    }

    return ALC_TRUE;
}

/* alcOpenDevice
 *
 * Opens the named device.
 */
ALC_API ALCdevice* ALC_APIENTRY alcOpenDevice(const ALCchar *deviceName)
{
    ALCdevice *device;

    DO_INITCONFIG();

    if (DeviceList)
    {
        return NULL;
    }

    device = al_calloc(16, sizeof(ALCdevice));
    if(!device)
    {
        return NULL;
    }

    device->ref = 1;

    device->dry.buffer = NULL;
    device->dry.num_channels = 0;
    device->foa_out.buffer = NULL;
    device->foa_out.num_channels = 0;
    device->real_out.buffer = NULL;
    device->real_out.num_channels = 0;

    device->context = NULL;

    device->auxiliary_effect_slot_max = 64;
    device->num_aux_sends = DEFAULT_SENDS;

    //Set output format
    device->fmt_chans = DevFmtChannelsDefault;
    device->frequency = DEFAULT_OUTPUT_RATE;
    device->update_size = clampu(1024, 64, 8192);

    if(device->auxiliary_effect_slot_max == 0) device->auxiliary_effect_slot_max = 64;

    device->source = al_calloc(16, sizeof(ALsource));
    InitSourceParams(device->source, device->num_aux_sends);

    device->effect_slot = al_calloc(16, sizeof(ALeffectslot));
    InitEffectSlot(device->effect_slot);
    aluInitEffectPanning(device->effect_slot);

    device->effect = al_calloc(16, sizeof(ALeffect));
    InitEffect(device->effect);

    DeviceList = device;

    return device;
}

/* alcCloseDevice
 *
 * Closes the given device.
 */
ALC_API ALCboolean ALC_APIENTRY alcCloseDevice(ALCdevice *device)
{
    ALCdevice *iter;
    ALCcontext *ctx;

    iter = DeviceList;
    if(!iter)
    {
        return ALC_FALSE;
    }

    ctx = device->context;
    if(ctx != NULL)
    {
        ReleaseContext(ctx, device);
    }

    ALCdevice_DecRef(device);

    return ALC_TRUE;
}
