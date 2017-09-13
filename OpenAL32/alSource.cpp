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
#include "AL/al.h"
#include "alSource.h"


void ALsource::Send::Channel::reset()
{
    low_pass_.reset();
    high_pass_.reset();

    current_gains_.fill(0.0F);
    target_gains_.fill(0.0F);
}

void init_source_params(
    ALsource* source)
{
    source->direct_.gain_ = 1.0F;
    source->direct_.gain_hf_ = 1.0F;
    source->direct_.hf_reference_ = FilterState::lp_frequency_reference;
    source->direct_.gain_lf_ = 1.0F;
    source->direct_.lf_reference_ = FilterState::hp_frequency_reference;
    source->aux_.gain_ = 1.0F;
    source->aux_.gain_hf_ = 1.0F;
    source->aux_.hf_reference_ = FilterState::lp_frequency_reference;
    source->aux_.gain_lf_ = 1.0F;
    source->aux_.lf_reference_ = FilterState::hp_frequency_reference;
}
