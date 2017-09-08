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


void init_source_params(ALsource* source, int num_sends);
void deinit_source(ALsource* source, int num_sends);


/************************************************
 * Global variables
 ************************************************/

ALCdevice* g_device = nullptr;


/************************************************
 * Miscellaneous ALC helpers
 ************************************************/

// Sets the default channel order used by WaveFormatEx.
void set_default_wfx_channel_order(
    ALCdevice* device)
{
    device->real_out.channel_name.fill(InvalidChannel);

    switch (device->fmt_chans)
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

// Updates device parameters according to the attribute list (caller is
// responsible for holding the list lock).
static void update_device_params(
    ALCdevice* device)
{
    device->dry.buffers = SampleBuffers{};
    device->dry.num_channels = 0;
    device->foa_out.buffers = nullptr;
    device->foa_out.num_channels = 0;
    device->real_out.buffers = nullptr;
    device->real_out.num_channels = 0;

    alu_init_renderer(device);

    device->dry.buffers.resize(device->dry.num_channels);

    device->real_out.buffers = &device->dry.buffers;
    device->real_out.num_channels = device->dry.num_channels;

    device->foa_out.buffers = &device->dry.buffers;
    device->foa_out.num_channels = device->dry.num_channels;

    auto slot = device->effect_slot;
    auto state = slot->effect.state;

    state->out_buffer = &device->dry.buffers;
    state->out_channels = device->dry.num_channels;

    state->update_device(device);
    update_effect_slot_props(slot);

    allocate_voices(device);

    for (int pos = 0; pos < device->voice_count; ++pos)
    {
        const auto voice = device->voice;

        if (!voice->source)
        {
            continue;
        }
    }

    update_all_source_props(device);
}

// Frees the device structure, and destroys any objects the app failed to
// delete. Called once there's no more references on the device.
static void free_device(
    ALCdevice* device)
{
    delete device->effect;

    deinit_effect_slot(device->effect_slot);
    delete device->effect_slot;

    deinit_source(device->source, device->num_aux_sends);
    delete device->source;

    device->dry.buffers = SampleBuffers{};
    device->dry.num_channels = 0;
    device->foa_out.buffers = nullptr;
    device->foa_out.num_channels = 0;
    device->real_out.buffers = nullptr;
    device->real_out.num_channels = 0;

    delete device;
}

// Cleans up the context, and destroys any remaining objects the app failed to
// delete. Called once there's no more references on the context.
static void free_context()
{
    auto device = g_device;

    for(int i = 0; i < device->voice_count; ++i)
    {
        deinit_voice(device->voice);
    }

    delete[] reinterpret_cast<char*>(device->voice);
    device->voice = nullptr;
    device->voice_count = 0;
}

// Removes the context reference from the given device and removes it from
// being current on the running thread or globally. Returns true if other
// contexts still exist on the device.
static void release_context()
{
    free_context();
}

void allocate_voices(
    ALCdevice* device)
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
ALC_API ALCdevice* ALC_APIENTRY alcOpenDevice(
    const ALCchar* device_name)
{
    if (g_device)
    {
        return nullptr;
    }

    auto device = new ALCdevice{};

    if(!device)
    {
        return nullptr;
    }

    device->dry.buffers = SampleBuffers{};
    device->dry.num_channels = 0;
    device->foa_out.buffers = nullptr;
    device->foa_out.num_channels = 0;
    device->real_out.buffers = nullptr;
    device->real_out.num_channels = 0;

    device->auxiliary_effect_slot_max = 64;
    device->num_aux_sends = default_sends;

    // Set output format
    device->fmt_chans = DevFmtChannelsDefault;
    device->frequency = default_output_rate;
    device->update_size = clamp(1024, 64, 8192);

    if (device->auxiliary_effect_slot_max == 0)
    {
        device->auxiliary_effect_slot_max = 64;
    }

    device->source = new ALsource{};
    init_source_params(device->source, device->num_aux_sends);

    device->effect_slot = new ALeffectslot{};
    init_effect_slot(device->effect_slot);
    alu_init_effect_panning(device->effect_slot);

    device->effect = new ALeffect{};
    init_effect(device->effect);

    device->voice = nullptr;
    device->voice_count = 0;

    update_device_params(device);
    allocate_voices(device);

    g_device = device;

    return device;
}

/* alcCloseDevice
 *
 * Closes the given device.
 */
ALC_API ALCboolean ALC_APIENTRY alcCloseDevice(ALCdevice *device)
{
    release_context();
    return ALC_TRUE;
}
