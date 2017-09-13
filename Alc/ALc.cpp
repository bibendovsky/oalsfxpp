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
#include "alu.h"


// Sets the default channel order used by WaveFormatEx.
void ALCdevice_struct::set_default_wfx_channel_order()
{
    channel_names_.fill(ChannelId::invalid);

    switch (channel_format_)
    {
    case ChannelFormat::mono:
        channel_names_[0] = ChannelId::front_center;
        break;

    case ChannelFormat::stereo:
        channel_names_[0] = ChannelId::front_left;
        channel_names_[1] = ChannelId::front_right;
        break;

    case ChannelFormat::quad:
        channel_names_[0] = ChannelId::front_left;
        channel_names_[1] = ChannelId::front_right;
        channel_names_[2] = ChannelId::back_left;
        channel_names_[3] = ChannelId::back_right;
        break;

    case ChannelFormat::five_point_one:
        channel_names_[0] = ChannelId::front_left;
        channel_names_[1] = ChannelId::front_right;
        channel_names_[2] = ChannelId::front_center;
        channel_names_[3] = ChannelId::lfe;
        channel_names_[4] = ChannelId::side_left;
        channel_names_[5] = ChannelId::side_right;
        break;

    case ChannelFormat::five_point_one_rear:
        channel_names_[0] = ChannelId::front_left;
        channel_names_[1] = ChannelId::front_right;
        channel_names_[2] = ChannelId::front_center;
        channel_names_[3] = ChannelId::lfe;
        channel_names_[4] = ChannelId::back_left;
        channel_names_[5] = ChannelId::back_right;
        break;

    case ChannelFormat::six_point_one:
        channel_names_[0] = ChannelId::front_left;
        channel_names_[1] = ChannelId::front_right;
        channel_names_[2] = ChannelId::front_center;
        channel_names_[3] = ChannelId::lfe;
        channel_names_[4] = ChannelId::back_center;
        channel_names_[5] = ChannelId::side_left;
        channel_names_[6] = ChannelId::side_right;
        break;

    case ChannelFormat::seven_point_one:
        channel_names_[0] = ChannelId::front_left;
        channel_names_[1] = ChannelId::front_right;
        channel_names_[2] = ChannelId::front_center;
        channel_names_[3] = ChannelId::lfe;
        channel_names_[4] = ChannelId::back_left;
        channel_names_[5] = ChannelId::back_right;
        channel_names_[6] = ChannelId::side_left;
        channel_names_[7] = ChannelId::side_right;
        break;
    }
}

ALCdevice_struct::ALCdevice_struct()
    :
    frequency_{},
    update_size_{},
    channel_format_{},
    channel_count_{},
    channel_names_{},
    sample_buffers_{},
    resampled_data_{},
    filtered_data_{},
    dry_{},
    foa_{},
    source_samples_{}
{
}

void ALCdevice_struct::initialize(
    const int channel_count,
    const int sampling_rate)
{
    channel_count_ = channel_count;

    // Set output format
    channel_format_ = ChannelFormat::mono;
    frequency_ = sampling_rate;
    update_size_ = 1024;

    alu_init_renderer(this);

    sample_buffers_.clear();
    sample_buffers_.resize(channel_count_);
}

void ALCdevice_struct::uninitialize()
{
}
