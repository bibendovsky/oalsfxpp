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


ALCdevice* g_device = nullptr;


void AmbiConfig::reset()
{
    for (auto& coeff : coeffs_)
    {
        coeff.fill(0.0F);
    }

    for (auto& map_item : map_)
    {
        map_item.reset();
    }
}

void BFChannelConfig::reset()
{
    scale_ = 0.0F;
    index_ = 0;
}

// Sets the default channel order used by WaveFormatEx.
void set_default_wfx_channel_order(
    ALCdevice* device)
{
    device->channel_names_.fill(InvalidChannel);

    switch (device->channel_format_)
    {
    case DevFmtMono:
        device->channel_names_[0] = FrontCenter;
        break;

    case DevFmtStereo:
        device->channel_names_[0] = FrontLeft;
        device->channel_names_[1] = FrontRight;
        break;

    case DevFmtQuad:
        device->channel_names_[0] = FrontLeft;
        device->channel_names_[1] = FrontRight;
        device->channel_names_[2] = BackLeft;
        device->channel_names_[3] = BackRight;
        break;

    case DevFmtX51:
        device->channel_names_[0] = FrontLeft;
        device->channel_names_[1] = FrontRight;
        device->channel_names_[2] = FrontCenter;
        device->channel_names_[3] = LFE;
        device->channel_names_[4] = SideLeft;
        device->channel_names_[5] = SideRight;
        break;

    case DevFmtX51Rear:
        device->channel_names_[0] = FrontLeft;
        device->channel_names_[1] = FrontRight;
        device->channel_names_[2] = FrontCenter;
        device->channel_names_[3] = LFE;
        device->channel_names_[4] = BackLeft;
        device->channel_names_[5] = BackRight;
        break;

    case DevFmtX61:
        device->channel_names_[0] = FrontLeft;
        device->channel_names_[1] = FrontRight;
        device->channel_names_[2] = FrontCenter;
        device->channel_names_[3] = LFE;
        device->channel_names_[4] = BackCenter;
        device->channel_names_[5] = SideLeft;
        device->channel_names_[6] = SideRight;
        break;

    case DevFmtX71:
        device->channel_names_[0] = FrontLeft;
        device->channel_names_[1] = FrontRight;
        device->channel_names_[2] = FrontCenter;
        device->channel_names_[3] = LFE;
        device->channel_names_[4] = BackLeft;
        device->channel_names_[5] = BackRight;
        device->channel_names_[6] = SideLeft;
        device->channel_names_[7] = SideRight;
        break;
    }
}

// Updates device parameters according to the attribute list (caller is
// responsible for holding the list lock).
static void update_device_params(
    ALCdevice* device)
{
    device->sample_buffers_ = SampleBuffers{};
    device->channel_count_ = 0;

    alu_init_renderer(device);

    device->sample_buffers_.resize(device->channel_count_);

    auto slot = device->effect_slot_;
    auto state = slot->effect_state_.get();

    state->dst_buffers_ = &device->sample_buffers_;
    state->dst_channel_count_ = device->channel_count_;

    state->update_device(device);
    slot->is_props_updated_ = true;

    auto source = device->source_;

    for (int i = 0; i < device->channel_count_; ++i)
    {
        source->direct_.channels_[i].reset();
        source->aux_.channels_[i].reset();
    }
}

// Frees the device structure, and destroys any objects the app failed to
// delete. Called once there's no more references on the device.
static void free_device(
    ALCdevice* device)
{
    delete device->effect_;
    delete device->effect_slot_;

    delete device->source_;

    device->sample_buffers_ = SampleBuffers{};
    device->channel_count_ = 0;

    delete device;
}


/************************************************
 * Standard ALC functions
 ************************************************/

/* alcOpenDevice
 *
 * Opens the named device.
 */
ALCdevice* alcOpenDevice()
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

    device->sample_buffers_ = SampleBuffers{};
    device->channel_count_ = 0;

    // Set output format
    device->channel_format_ = DevFmtChannelsDefault;
    device->frequency_ = default_output_rate;
    device->update_size_ = clamp(1024, 64, 8192);

    device->source_ = new ALsource{};

    device->effect_slot_ = new EffectSlot{};
    alu_init_effect_panning(device->effect_slot_);

    device->effect_ = new Effect{};
    device->effect_->initialize();

    update_device_params(device);

    g_device = device;

    return device;
}

/* alcCloseDevice
 *
 * Closes the given device.
 */
void alcCloseDevice(
    ALCdevice* device)
{
    free_device(g_device);
}
