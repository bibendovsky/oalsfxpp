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

/* One-time configuration init control */
static ALCboolean alc_config_once = ALC_FALSE;

/* Flag to specify if alcSuspendContext/alcProcessContext should defer/process
 * updates.
 */
static ALCboolean SuspendDefers = ALC_TRUE;


ALCdevice* g_device = nullptr;


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

extern inline int GetChannelIndex(const enum Channel names[MAX_OUTPUT_CHANNELS], enum Channel chan);


/* UpdateDeviceParams
 *
 * Updates device parameters according to the attribute list (caller is
 * responsible for holding the list lock).
 */
static void UpdateDeviceParams(
    ALCdevice* device)
{
    const auto old_sends = device->num_aux_sends;
    const auto new_sends = device->num_aux_sends;

    device->dry.buffers = SampleBuffers{};
    device->dry.num_channels = 0;
    device->foa_out.buffers = nullptr;
    device->foa_out.num_channels = 0;
    device->real_out.buffers = nullptr;
    device->real_out.num_channels = 0;

    aluInitRenderer(device);

    device->dry.buffers.resize(device->dry.num_channels);

    device->real_out.buffers = &device->dry.buffers;
    device->real_out.num_channels = device->dry.num_channels;

    device->foa_out.buffers = &device->dry.buffers;
    device->foa_out.num_channels = device->dry.num_channels;

    device->num_aux_sends = new_sends;

    auto slot = device->effect_slot;
    auto state = slot->effect.state;

    state->out_buffer = &device->dry.buffers;
    state->out_channels = device->dry.num_channels;

    state->update_device(device);
    UpdateEffectSlotProps(slot);

    AllocateVoices(device, 1, old_sends);

    for (int pos = 0; pos < device->voice_count; ++pos)
    {
        auto voice = device->voice;

        if (!voice->source)
        {
            continue;
        }
    }

    UpdateAllSourceProps(device);
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

    device->dry.buffers = SampleBuffers{};
    device->dry.num_channels = 0;
    device->foa_out.buffers = nullptr;
    device->foa_out.num_channels = 0;
    device->real_out.buffers = nullptr;
    device->real_out.num_channels = 0;

    delete device;
}

/* FreeContext
 *
 * Cleans up the context, and destroys any remaining objects the app failed to
 * delete. Called once there's no more references on the context.
 */
static void FreeContext(ALCcontext *context)
{
    auto device = g_device;

    for(int i = 0; i < device->voice_count; ++i)
    {
        DeinitVoice(device->voice);
    }

    delete[] reinterpret_cast<char*>(device->voice);
    device->voice = nullptr;
    device->voice_count = 0;
}

/* ReleaseContext
 *
 * Removes the context reference from the given device and removes it from
 * being current on the running thread or globally. Returns true if other
 * contexts still exist on the device.
 */
static void ReleaseContext(ALCdevice *device)
{
    FreeContext(nullptr);
}

void AllocateVoices(ALCdevice* device, int num_voices, int old_sends)
{
    if (device->voice)
    {
        return;
    }

    delete device->voice;
    device->voice = new ALvoice{};
    device->voice_count = 1;
}


/************************************************
 * Standard ALC functions
 ************************************************/

/* alcOpenDevice
 *
 * Opens the named device.
 */
ALC_API ALCdevice* ALC_APIENTRY alcOpenDevice(const ALCchar *deviceName)
{
    ALCdevice *device;

    if (g_device)
    {
        return nullptr;
    }

    device = new ALCdevice{};

    if(!device)
    {
        return NULL;
    }

    device->dry.buffers = SampleBuffers{};
    device->dry.num_channels = 0;
    device->foa_out.buffers = NULL;
    device->foa_out.num_channels = 0;
    device->real_out.buffers = NULL;
    device->real_out.num_channels = 0;

    device->auxiliary_effect_slot_max = 64;
    device->num_aux_sends = DEFAULT_SENDS;

    //Set output format
    device->fmt_chans = DevFmtChannelsDefault;
    device->frequency = DEFAULT_OUTPUT_RATE;
    device->update_size = clamp(1024, 64, 8192);

    if(device->auxiliary_effect_slot_max == 0) device->auxiliary_effect_slot_max = 64;

    device->source = new ALsource{};
    InitSourceParams(device->source, device->num_aux_sends);

    device->effect_slot = new ALeffectslot{};
    InitEffectSlot(device->effect_slot);
    aluInitEffectPanning(device->effect_slot);

    device->effect = new ALeffect{};
    InitEffect(device->effect);

    device->voice = nullptr;
    device->voice_count = 0;

    UpdateDeviceParams(device);
    AllocateVoices(device, 1, device->num_aux_sends);

    g_device = device;

    return device;
}

/* alcCloseDevice
 *
 * Closes the given device.
 */
ALC_API ALCboolean ALC_APIENTRY alcCloseDevice(ALCdevice *device)
{
    ReleaseContext(device);
    return ALC_TRUE;
}
