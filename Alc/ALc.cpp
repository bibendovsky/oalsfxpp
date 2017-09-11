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


void init_source_params(ALsource* source, const int num_sends);
void deinit_source(ALsource* source, const int num_sends);


ALCdevice* g_device = nullptr;


void AmbiConfig::reset()
{
    for (auto& coeff : coeffs)
    {
        coeff.fill(0.0F);
    }

    for (auto& map_item : map)
    {
        map_item.reset();
    }
}

void BFChannelConfig::reset()
{
    scale = 0.0F;
    index = 0;
}

// Sets the default channel order used by WaveFormatEx.
void set_default_wfx_channel_order(
    ALCdevice* device)
{
    device->channel_names.fill(InvalidChannel);

    switch (device->fmt_chans)
    {
    case DevFmtMono:
        device->channel_names[0] = FrontCenter;
        break;

    case DevFmtStereo:
        device->channel_names[0] = FrontLeft;
        device->channel_names[1] = FrontRight;
        break;

    case DevFmtQuad:
        device->channel_names[0] = FrontLeft;
        device->channel_names[1] = FrontRight;
        device->channel_names[2] = BackLeft;
        device->channel_names[3] = BackRight;
        break;

    case DevFmtX51:
        device->channel_names[0] = FrontLeft;
        device->channel_names[1] = FrontRight;
        device->channel_names[2] = FrontCenter;
        device->channel_names[3] = LFE;
        device->channel_names[4] = SideLeft;
        device->channel_names[5] = SideRight;
        break;

    case DevFmtX51Rear:
        device->channel_names[0] = FrontLeft;
        device->channel_names[1] = FrontRight;
        device->channel_names[2] = FrontCenter;
        device->channel_names[3] = LFE;
        device->channel_names[4] = BackLeft;
        device->channel_names[5] = BackRight;
        break;

    case DevFmtX61:
        device->channel_names[0] = FrontLeft;
        device->channel_names[1] = FrontRight;
        device->channel_names[2] = FrontCenter;
        device->channel_names[3] = LFE;
        device->channel_names[4] = BackCenter;
        device->channel_names[5] = SideLeft;
        device->channel_names[6] = SideRight;
        break;

    case DevFmtX71:
        device->channel_names[0] = FrontLeft;
        device->channel_names[1] = FrontRight;
        device->channel_names[2] = FrontCenter;
        device->channel_names[3] = LFE;
        device->channel_names[4] = BackLeft;
        device->channel_names[5] = BackRight;
        device->channel_names[6] = SideLeft;
        device->channel_names[7] = SideRight;
        break;
    }
}

// Updates device parameters according to the attribute list (caller is
// responsible for holding the list lock).
static void update_device_params(
    ALCdevice* device)
{
    device->sample_buffers = SampleBuffers{};
    device->channel_count = 0;

    alu_init_renderer(device);

    device->sample_buffers.resize(device->channel_count);

    auto slot = device->effect_slot;
    auto state = slot->effect_state_.get();

    state->out_buffer = &device->sample_buffers;
    state->out_channels = device->channel_count;

    state->update_device(device);
    slot->is_props_updated_ = true;

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
    delete device->effect_slot;

    deinit_source(device->source, device->num_aux_sends);
    delete device->source;

    device->sample_buffers = SampleBuffers{};
    device->channel_count = 0;

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

    device->sample_buffers = SampleBuffers{};
    device->channel_count = 0;

    device->num_aux_sends = default_sends;

    // Set output format
    device->fmt_chans = DevFmtChannelsDefault;
    device->frequency = default_output_rate;
    device->update_size = clamp(1024, 64, 8192);

    device->source = new ALsource{};
    init_source_params(device->source, device->num_aux_sends);

    device->effect_slot = new EffectSlot{};
    alu_init_effect_panning(device->effect_slot);

    device->effect = new Effect{};
    device->effect->initialize();

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
    free_device(g_device);

    return ALC_TRUE;
}
