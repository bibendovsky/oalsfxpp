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


// Returns an updated source state using the matching voice's status (or lack
// thereof).
static int get_source_state(
    ALsource* source,
    ALvoice* voice)
{
    if (!voice)
    {
        int state = AL_PLAYING;

        if (source->state_ == state ? (source->state_ = AL_STOPPED, true) : (state = source->state_, false))
        {
            return AL_STOPPED;
        }

        return state;
    }

    return source->state_;
}

void update_source_props(
    ALsource* source,
    ALvoice* voice)
{
    // Get an unused property container, or allocate a new one as needed.
    auto& props = voice->props_;

    // Copy in current property values.
    props.direct_.gain_ = source->direct_.gain_;
    props.direct_.gain_hf_ = source->direct_.gain_hf_;
    props.direct_.hf_reference_ = source->direct_.hf_reference_;
    props.direct_.gain_lf_ = source->direct_.gain_lf_;
    props.direct_.lf_reference_ = source->direct_.lf_reference_;

    props.send_.effect_slot_ = source->send_->effect_slot_;
    props.send_.gain_ = source->send_->gain_;
    props.send_.gain_hf_ = source->send_->gain_hf_;
    props.send_.hf_reference_ = source->send_->hf_reference_;
    props.send_.gain_lf_ = source->send_->gain_lf_;
    props.send_.lf_reference_ = source->send_->lf_reference_;
}

AL_API void AL_APIENTRY alSourcePlay(
    ALuint source)
{
    alSourcePlayv(1, &source);
}

AL_API void AL_APIENTRY alSourcePlayv(
    int n,
    const ALuint* sources)
{
    assert(n == 1);

    ALsource* source;
    ALvoice* voice;

    auto device = g_device;

    bool start_fading = false;

    source = device->source_;
    voice = device->voice_;

    switch (get_source_state(source, voice))
    {
        case AL_PLAYING:
            assert(voice != nullptr);
            // A source that's already playing is restarted from the beginning.
            return;

        case AL_PAUSED:
            assert(voice != nullptr);

            // A source that's paused simply resumes.
            voice->is_playing_ = true;
            source->state_ = AL_PLAYING;
            return;

        default:
            break;
    }

    // Make sure this source isn't already active, and if not, look for an
    // unused voice to put it in.
    for (int j = 0; j < device->voice_count_; ++j)
    {
        if (!device->voice_->source_)
        {
            voice = device->voice_;
            break;
        }
    }

    voice->is_playing_ = false;

    update_source_props(source, voice);

    voice->channel_count_ = device->channel_count_;

    for (int i = 0; i < voice->channel_count_; ++i)
    {
        voice->direct_.params_[i].reset();
        voice->send_.params_[i].reset();
    }

    voice->source_ = source;
    voice->is_playing_ = true;
    source->state_ = AL_PLAYING;
}

AL_API void AL_APIENTRY alSourceStop(
    ALuint source)
{
    alSourceStopv(1, &source);
}

AL_API void AL_APIENTRY alSourceStopv(
    int n,
    const ALuint* sources)
{
    assert(n == 1);

    auto device = g_device;
    auto source = device->source_;
    auto voice = device->voice_;

    if (voice)
    {
        voice->source_ = nullptr;
        voice->is_playing_ = false;
    }

    if (source->state_ != AL_INITIAL)
    {
        source->state_ = AL_STOPPED;
    }
}

void init_source_params(
    ALsource* source)
{
    source->direct_.gain_ = 1.0F;
    source->direct_.gain_hf_ = 1.0F;
    source->direct_.hf_reference_ = lp_frequency_reference;
    source->direct_.gain_lf_ = 1.0F;
    source->direct_.lf_reference_ = hp_frequency_reference;
    source->send_ = std::make_unique<ALsource::Send>();
    source->send_->effect_slot_ = nullptr;
    source->send_->gain_ = 1.0F;
    source->send_->gain_hf_ = 1.0F;
    source->send_->hf_reference_ = lp_frequency_reference;
    source->send_->gain_lf_ = 1.0F;
    source->send_->lf_reference_ = hp_frequency_reference;
    source->state_ = AL_INITIAL;
}

void deinit_source(
    ALsource* source)
{
    if (source->send_)
    {
        source->send_->effect_slot_ = nullptr;
        source->send_ = nullptr;
    }
}

void update_all_source_props(
    ALCdevice* device)
{
    for (int pos = 0; pos < device->voice_count_; ++pos)
    {
        auto source = device->source_;

        if (source)
        {
            auto voice = device->voice_;
            update_source_props(source, voice);
        }
    }
}
