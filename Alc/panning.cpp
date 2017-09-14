/**
 * OpenAL cross platform audio library
 * Copyright (C) 1999-2010 by authors.
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


#include "alMain.h"


void alu_init_renderer(
    ALCdevice* device)
{
    device->dry_.ambi_.reset();
    device->dry_.coeff_count_ = 0;
    device->set_default_wfx_channel_order();

    const ChannelPanning *channel_map = nullptr;
    auto coeff_count = 0;
    auto count = 0;

    switch (device->channel_format_)
    {
    case ChannelFormat::mono:
        count = count_of(Panning::mono_panning);
        channel_map = Panning::mono_panning;
        coeff_count = 1;
        break;

    case ChannelFormat::stereo:
        count = count_of(Panning::stereo_panning);
        channel_map = Panning::stereo_panning;
        coeff_count = 4;
        break;

    case ChannelFormat::quad:
        count = count_of(Panning::quad_panning);
        channel_map = Panning::quad_panning;
        coeff_count = 4;
        break;

    case ChannelFormat::five_point_one:
        count = count_of(Panning::x5_1_side_panning);
        channel_map = Panning::x5_1_side_panning;
        coeff_count = 9;
        break;

    case ChannelFormat::five_point_one_rear:
        count = count_of(Panning::x5_1_rear_panning);
        channel_map = Panning::x5_1_rear_panning;
        coeff_count = 9;
        break;

    case ChannelFormat::six_point_one:
        count = count_of(Panning::x6_1_panning);
        channel_map = Panning::x6_1_panning;
        coeff_count = 9;
        break;

    case ChannelFormat::seven_point_one:
        count = count_of(Panning::x7_1_panning);
        channel_map = Panning::x7_1_panning;
        coeff_count = 16;
        break;
    }

    Panning::set_channel_map(
        device->channel_names_.data(),
        device->dry_.ambi_.coeffs_.data(),
        channel_map,
        count,
        &device->channel_count_);

    device->dry_.coeff_count_ = coeff_count;

    device->foa_.ambi_.reset();

    for (int i = 0; i < device->channel_count_; ++i)
    {
        device->foa_.ambi_.coeffs_[i][0] = device->dry_.ambi_.coeffs_[i][0];

        for (int j = 1; j < 4; ++j)
        {
            device->foa_.ambi_.coeffs_[i][j] = device->dry_.ambi_.coeffs_[i][j];
        }
    }

    device->foa_.coeff_count_ = 4;
}
