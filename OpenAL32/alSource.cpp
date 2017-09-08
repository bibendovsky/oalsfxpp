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


void init_source_params(ALsource* source, int num_sends);
void deinit_source(ALsource *source, int num_sends);
void update_source_props(ALsource* source, ALvoice* voice, int num_sends);

enum SourceProp
{
    srcPitch = AL_PITCH,
    srcGain = AL_GAIN,
    srcMinGain = AL_MIN_GAIN,
    srcMaxGain = AL_MAX_GAIN,
    srcMaxDistance = AL_MAX_DISTANCE,
    srcRolloffFactor = AL_ROLLOFF_FACTOR,
    srcDopplerFactor = AL_DOPPLER_FACTOR,
    srcConeOuterGain = AL_CONE_OUTER_GAIN,
    srcSecOffset = AL_SEC_OFFSET,
    srcSampleOffset = AL_SAMPLE_OFFSET,
    srcByteOffset = AL_BYTE_OFFSET,
    srcConeInnerAngle = AL_CONE_INNER_ANGLE,
    srcConeOuterAngle = AL_CONE_OUTER_ANGLE,
    srcRefDistance = AL_REFERENCE_DISTANCE,

    srcPosition = AL_POSITION,
    srcVelocity = AL_VELOCITY,
    srcDirection = AL_DIRECTION,

    srcSourceRelative = AL_SOURCE_RELATIVE,
    srcLooping = AL_LOOPING,
    srcBuffer = AL_BUFFER,
    srcSourceState = AL_SOURCE_STATE,
    srcBuffersQueued = AL_BUFFERS_QUEUED,
    srcBuffersProcessed = AL_BUFFERS_PROCESSED,
    srcSourceType = AL_SOURCE_TYPE,

    /* ALC_EXT_EFX */
    srcConeOuterGainHF = AL_CONE_OUTER_GAINHF,
    srcAirAbsorptionFactor = AL_AIR_ABSORPTION_FACTOR,
    srcRoomRolloffFactor =  AL_ROOM_ROLLOFF_FACTOR,
    srcDirectFilterGainHFAuto = AL_DIRECT_FILTER_GAINHF_AUTO,
    srcAuxSendFilterGainAuto = AL_AUXILIARY_SEND_FILTER_GAIN_AUTO,
    srcAuxSendFilterGainHFAuto = AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO,
    srcDirectFilter = AL_DIRECT_FILTER,
    srcAuxSendFilter = AL_AUXILIARY_SEND_FILTER,

    /* AL_SOFT_direct_channels */
    srcDirectChannelsSOFT = AL_DIRECT_CHANNELS_SOFT,

    /* AL_EXT_source_distance_model */
    srcDistanceModel = AL_DISTANCE_MODEL,

    srcByteLengthSOFT = AL_BYTE_LENGTH_SOFT,
    srcSampleLengthSOFT = AL_SAMPLE_LENGTH_SOFT,
    srcSecLengthSOFT = AL_SEC_LENGTH_SOFT,

    /* AL_SOFT_source_latency */
    srcSampleOffsetLatencySOFT = AL_SAMPLE_OFFSET_LATENCY_SOFT,
    srcSecOffsetLatencySOFT = AL_SEC_OFFSET_LATENCY_SOFT,

    /* AL_EXT_STEREO_ANGLES */
    srcAngles = AL_STEREO_ANGLES,

    /* AL_EXT_SOURCE_RADIUS */
    srcRadius = AL_SOURCE_RADIUS,

    /* AL_EXT_BFORMAT */
    srcOrientation = AL_ORIENTATION,

    /* AL_SOFT_source_resampler */
    srcResampler = AL_SOURCE_RESAMPLER_SOFT,

    /* AL_SOFT_source_spatialize */
    srcSpatialize = AL_SOURCE_SPATIALIZE_SOFT,
}; // SourceProp

/**
 * Returns an updated source state using the matching voice's status (or lack
 * thereof).
 */
static inline int get_source_state(ALsource* source, ALvoice* voice)
{
    if(!voice)
    {
        int state = AL_PLAYING;
        if(source->state == state ? (source->state = AL_STOPPED, true) : (state = source->state, false))
            return AL_STOPPED;
        return state; 
    }
    return source->state;
}

AL_API void AL_APIENTRY alSourcePlay(ALuint source)
{
    alSourcePlayv(1, &source);
}
AL_API void AL_APIENTRY alSourcePlayv(int n, const ALuint *sources)
{
    ALCdevice *device;
    ALsource *source;
    ALvoice *voice;
    int i, j;

    device = g_device;

    if(!(n == 1))
        return;

    for(i = 0;i < n;i++)
    {
        bool start_fading = false;

        source = device->source;
        voice = device->voice;

        switch(get_source_state(source, voice))
        {
            case AL_PLAYING:
                assert(voice != NULL);
                /* A source that's already playing is restarted from the beginning. */
                return;

            case AL_PAUSED:
                assert(voice != NULL);
                /* A source that's paused simply resumes. */
                voice->playing = true;
                source->state = AL_PLAYING;
                return;

            default:
                break;
        }

        /* Make sure this source isn't already active, and if not, look for an
         * unused voice to put it in.
         */
        for(j = 0;j < device->voice_count;j++)
        {
            if(device->voice->source == NULL)
            {
                voice = device->voice;
                break;
            }
        }
        voice->playing = false;

        update_source_props(source, voice, device->num_aux_sends);

        voice->num_channels = device->dry.num_channels;

        memset(voice->direct.params, 0, sizeof(voice->direct.params[0])*voice->num_channels);

        if(device->num_aux_sends > 0)
        {
            memset(voice->send.params, 0, sizeof(SendParams)*voice->num_channels);
        }

        voice->source = source;
        voice->playing = true;
        source->state = AL_PLAYING;
    }
}


AL_API void AL_APIENTRY alSourceStop(ALuint source)
{
    alSourceStopv(1, &source);
}
AL_API void AL_APIENTRY alSourceStopv(int n, const ALuint *sources)
{
    ALCdevice *device;
    ALsource *source;
    ALvoice *voice;
    int i;

    if(!(n == 1))
        return;

    device = g_device;

    for(i = 0;i < n;i++)
    {
        source = device->source;
        if((voice=device->voice) != NULL)
        {
            voice->source = NULL;
            voice->playing = false;
        }
        if(source->state != AL_INITIAL)
            source->state = AL_STOPPED;
    }
}

void init_source_params(ALsource* source, int num_sends)
{
    source->direct.gain = 1.0f;
    source->direct.gain_hf = 1.0f;
    source->direct.hf_reference = lp_frequency_reference;
    source->direct.gain_lf = 1.0f;
    source->direct.lf_reference = hp_frequency_reference;
    source->send = std::make_unique<ALsource::Send>();
    source->send->slot = nullptr;
    source->send->gain = 1.0f;
    source->send->gain_hf = 1.0f;
    source->send->hf_reference = lp_frequency_reference;
    source->send->gain_lf = 1.0f;
    source->send->lf_reference = hp_frequency_reference;
    source->state = AL_INITIAL;
}

void deinit_source(ALsource* source, int num_sends)
{
    if (source->send)
    {
        source->send->slot = nullptr;
        source->send = nullptr;
    }
}

void update_source_props(ALsource* source, ALvoice* voice, int num_sends)
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

void update_all_source_props(ALCdevice* device)
{
    int num_sends = device->num_aux_sends;
    int pos;

    for(pos = 0;pos < device->voice_count;pos++)
    {
        ALvoice *voice = device->voice;
        ALsource *source = voice->source;
        if(source)
            update_source_props(source, voice, num_sends);
    }
}
