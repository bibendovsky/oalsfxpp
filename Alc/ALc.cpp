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
#include "alSource.h"


void InitSourceParams(ALsource *Source, int num_sends);
void DeinitSource(ALsource *source, int num_sends);


/************************************************
 * Global variables
 ************************************************/

/* Default is always the first in the list */
static ALCchar *alcDefaultAllDevicesSpecifier;

static ALCenum LastNullDeviceError = ALC_NO_ERROR;

/* Process-wide current context */
static ALCcontext* GlobalContext = NULL;

/* One-time configuration init control */
static ALCboolean alc_config_once = ALC_FALSE;

/* Flag to specify if alcSuspendContext/alcProcessContext should defer/process
 * updates.
 */
static ALCboolean SuspendDefers = ALC_TRUE;


/************************************************
 * Device lists
 ************************************************/
static ALCdevice* DeviceList = NULL;


/************************************************
 * Miscellaneous ALC helpers
 ************************************************/

/* SetDefaultWFXChannelOrder
 *
 * Sets the default channel order used by WaveFormatEx.
 */
void SetDefaultWFXChannelOrder(ALCdevice *device)
{
    int i;

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
static ALCenum UpdateDeviceParams(
    ALCdevice* device,
    const ALCint* attrList)
{
    const auto old_sends = device->num_aux_sends;
    const auto new_sends = device->num_aux_sends;

    device->dry.buffer = SampleBuffers{};
    device->dry.num_channels = 0;
    device->foa_out.buffer = nullptr;
    device->foa_out.num_channels = 0;
    device->real_out.buffer = nullptr;
    device->real_out.num_channels = 0;

    aluInitRenderer(device);

    device->dry.buffer.resize(device->dry.num_channels);

    device->real_out.buffer = &device->dry.buffer;
    device->real_out.num_channels = device->dry.num_channels;

    device->foa_out.buffer = &device->dry.buffer;
    device->foa_out.num_channels = device->dry.num_channels;

    device->num_aux_sends = new_sends;

    /* Need to delay returning failure until replacement Send arrays have been
     * allocated with the appropriate size.
     */
    auto update_failed = ALboolean{AL_FALSE};
    auto context = device->context;

    if (context)
    {
        auto slot = device->effect_slot;
        auto state = slot->effect.state;

        state->out_buffer = &device->dry.buffer;
        state->out_channels = device->dry.num_channels;

        if (state->update_device(device) == AL_FALSE)
        {
            update_failed = AL_TRUE;
        }
        else
        {
            UpdateEffectSlotProps(slot);
        }

        AllocateVoices(context, 1, old_sends);

        for (int pos = 0; pos < context->voice_count; ++pos)
        {
            auto voice = context->voice;

            if (!voice->source)
            {
                continue;
            }
        }

        UpdateAllSourceProps(context);
    }

    if (update_failed)
    {
        return ALC_INVALID_DEVICE;
    }

    return ALC_NO_ERROR;
}

/* FreeDevice
 *
 * Frees the device structure, and destroys any objects the app failed to
 * delete. Called once there's no more references on the device.
 */
static ALCvoid FreeDevice(ALCdevice *device)
{
    delete device->effect;

    DeinitEffectSlot(device->effect_slot);
    delete device->effect_slot;

    DeinitSource(device->source, device->num_aux_sends);
    delete device->source;

    device->dry.buffer = SampleBuffers{};
    device->dry.num_channels = 0;
    device->foa_out.buffer = nullptr;
    device->foa_out.num_channels = 0;
    device->real_out.buffer = nullptr;
    device->real_out.num_channels = 0;

    delete device;
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
            return ALC_TRUE;
        }
    }

    *device = NULL;
    return ALC_FALSE;
}

/* FreeContext
 *
 * Cleans up the context, and destroys any remaining objects the app failed to
 * delete. Called once there's no more references on the context.
 */
static void FreeContext(ALCcontext *context)
{
    size_t count;
    int i;

    for(i = 0;i < context->voice_count;i++)
        DeinitVoice(context->voice);
    delete[] reinterpret_cast<char*>(context->voice);
    context->voice = nullptr;
    context->voice_count = 0;

    count = 0;

    //Invalidate context
    memset(context, 0, sizeof(ALCcontext));

    delete context;
}

/* ReleaseContext
 *
 * Removes the context reference from the given device and removes it from
 * being current on the running thread or globally. Returns true if other
 * contexts still exist on the device.
 */
static void ReleaseContext(ALCdevice *device)
{
    FreeContext(device->context);
    device->context = nullptr;
    GlobalContext = nullptr;
}

/* GetContextRef
 *
 * Returns the currently active context for this thread, and adds a reference
 * without locking it.
 */
ALCcontext* GetContextRef()
{
    return GlobalContext;
}

void AllocateVoices(ALCcontext *context, int num_voices, int old_sends)
{
    if (context->voice)
    {
        return;
    }

    delete context->voice;
    context->voice = new ALvoice{};
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
    ALCenum err;

    /* Explicitly hold the list lock while taking the BackendLock in case the
     * device is asynchronously destropyed, to ensure this new context is
     * properly cleaned up after being made.
     */
    if(!VerifyDevice(&device))
    {
        return NULL;
    }

    if (device->context)
    {
        return NULL;
    }

    auto ALContext = new ALCcontext{};
    if(!ALContext)
    {
        return NULL;
    }

    ALContext->voice = NULL;
    ALContext->voice_count = 0;
    ALContext->device = device;

    if((err=UpdateDeviceParams(device, attrList)) != ALC_NO_ERROR)
    {
        delete ALContext;
        ALContext = NULL;

        if(err == ALC_INVALID_DEVICE)
        {
            aluHandleDisconnect(device);
        }
        return NULL;
    }
    AllocateVoices(ALContext, 1, device->num_aux_sends);

    device->context = ALContext;
    GlobalContext = ALContext;

    return ALContext;
}

/* alcDestroyContext
 *
 * Remove a context from its device
 */
ALC_API ALCvoid ALC_APIENTRY alcDestroyContext(ALCcontext *context)
{
    auto Device = context->device;
    if(Device)
    {
        ReleaseContext(Device);
    }
}

/* alcOpenDevice
 *
 * Opens the named device.
 */
ALC_API ALCdevice* ALC_APIENTRY alcOpenDevice(const ALCchar *deviceName)
{
    ALCdevice *device;

    if (DeviceList)
    {
        return NULL;
    }

    device = new ALCdevice{};

    if(!device)
    {
        return NULL;
    }

    device->dry.buffer = SampleBuffers{};
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
    device->update_size = clampi(1024, 64, 8192);

    if(device->auxiliary_effect_slot_max == 0) device->auxiliary_effect_slot_max = 64;

    device->source = new ALsource{};
    InitSourceParams(device->source, device->num_aux_sends);

    device->effect_slot = new ALeffectslot{};
    InitEffectSlot(device->effect_slot);
    aluInitEffectPanning(device->effect_slot);

    device->effect = new ALeffect{};
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
        ReleaseContext(device);
    }

    return ALC_TRUE;
}
