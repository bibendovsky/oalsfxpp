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

        if (source->state == state ? (source->state = AL_STOPPED, true) : (state = source->state, false))
        {
            return AL_STOPPED;
        }

        return state;
    }

    return source->state;
}

void update_source_props(
    ALsource* source,
    ALvoice* voice,
    const int num_sends)
{
    // Get an unused property container, or allocate a new one as needed.
    auto& props = voice->props;

    // Copy in current property values.
    props.direct.gain = source->direct.gain;
    props.direct.gain_hf = source->direct.gain_hf;
    props.direct.hf_reference = source->direct.hf_reference;
    props.direct.gain_lf = source->direct.gain_lf;
    props.direct.lf_reference = source->direct.lf_reference;

    if (num_sends > 0)
    {
        props.send.slot = source->send->slot;
        props.send.gain = source->send->gain;
        props.send.gain_hf = source->send->gain_hf;
        props.send.hf_reference = source->send->hf_reference;
        props.send.gain_lf = source->send->gain_lf;
        props.send.lf_reference = source->send->lf_reference;
    }
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

    source = device->source;
    voice = device->voice;

    switch (get_source_state(source, voice))
    {
        case AL_PLAYING:
            assert(voice != nullptr);
            // A source that's already playing is restarted from the beginning.
            return;

        case AL_PAUSED:
            assert(voice != nullptr);

            // A source that's paused simply resumes.
            voice->playing = true;
            source->state = AL_PLAYING;
            return;

        default:
            break;
    }

    // Make sure this source isn't already active, and if not, look for an
    // unused voice to put it in.
    for (int j = 0; j < device->voice_count; ++j)
    {
        if (!device->voice->source)
        {
            voice = device->voice;
            break;
        }
    }

    voice->playing = false;

    update_source_props(source, voice, device->num_aux_sends);

    voice->num_channels = device->dry.num_channels;

    for (int i = 0; i < voice->num_channels; ++i)
    {
        voice->direct.params[i].reset();
    }

    if (device->num_aux_sends > 0)
    {
        for (int i = 0; i < voice->num_channels; ++i)
        {
            voice->send.params[i].reset();
        }
    }

    voice->source = source;
    voice->playing = true;
    source->state = AL_PLAYING;
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
    auto source = device->source;
    auto voice = device->voice;

    if (voice)
    {
        voice->source = nullptr;
        voice->playing = false;
    }

    if (source->state != AL_INITIAL)
    {
        source->state = AL_STOPPED;
    }
}

void init_source_params(
    ALsource* source,
    const int num_sends)
{
    source->direct.gain = 1.0F;
    source->direct.gain_hf = 1.0F;
    source->direct.hf_reference = lp_frequency_reference;
    source->direct.gain_lf = 1.0F;
    source->direct.lf_reference = hp_frequency_reference;
    source->send = std::make_unique<ALsource::Send>();
    source->send->slot = nullptr;
    source->send->gain = 1.0F;
    source->send->gain_hf = 1.0F;
    source->send->hf_reference = lp_frequency_reference;
    source->send->gain_lf = 1.0F;
    source->send->lf_reference = hp_frequency_reference;
    source->state = AL_INITIAL;
}

void deinit_source(
    ALsource* source,
    const int num_sends)
{
    if (source->send)
    {
        source->send->slot = nullptr;
        source->send = nullptr;
    }
}

void update_all_source_props(
    ALCdevice* device)
{
    int num_sends = device->num_aux_sends;

    for (int pos = 0; pos < device->voice_count; ++pos)
    {
        auto source = device->source;

        if (source)
        {
            auto voice = device->voice;
            update_source_props(source, voice, num_sends);
        }
    }
}
