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

#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <float.h>

#include "AL/al.h"
#include "AL/alc.h"
#include "alMain.h"
#include "alSource.h"
#include "alBuffer.h"
#include "alAuxEffectSlot.h"
#include "almalloc.h"


void InitSourceParams(ALsource *Source, ALsizei num_sends);
void DeinitSource(ALsource *source, ALsizei num_sends);
void UpdateSourceProps(ALsource *source, ALvoice *voice, ALsizei num_sends);

typedef enum SourceProp {
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
} SourceProp;

static inline ALvoice *GetSourceVoice(const ALsource *source, const ALCcontext *context)
{
    return context->voice;
}

/**
 * Returns if the last known state for the source was playing or paused. Does
 * not sync with the mixer voice.
 */
static inline bool IsPlayingOrPaused(ALsource *source)
{
    ALenum state = source->state;
    return state == AL_PLAYING || state == AL_PAUSED;
}

/**
 * Returns an updated source state using the matching voice's status (or lack
 * thereof).
 */
static inline ALenum GetSourceState(ALsource *source, ALvoice *voice)
{
    if(!voice)
    {
        ALenum state = AL_PLAYING;
        if(source->state == state ? (source->state = AL_STOPPED, true) : (state = source->state, false))
            return AL_STOPPED;
        return state; 
    }
    return source->state;
}

/**
 * Returns if the source should specify an update, given the context's
 * deferring state and the source's last known state.
 */
static inline bool SourceShouldUpdate(ALsource *source, ALCcontext *context)
{
    return IsPlayingOrPaused(source);
}

#define DO_UPDATEPROPS() do {                                                 \
    ALvoice *voice;                                                           \
    if(SourceShouldUpdate(Source, Context) &&                                 \
       (voice=GetSourceVoice(Source, Context)) != NULL)                       \
        UpdateSourceProps(Source, voice, device->NumAuxSends);                \
} while(0)

#undef CHECKVAL


AL_API ALvoid AL_APIENTRY alSourcePlay(ALuint source)
{
    alSourcePlayv(1, &source);
}
AL_API ALvoid AL_APIENTRY alSourcePlayv(ALsizei n, const ALuint *sources)
{
    ALCcontext *context;
    ALCdevice *device;
    ALsource *source;
    ALvoice *voice;
    ALsizei i, j;

    context = GetContextRef();
    if(!context) return;

    if(!(n == 1))
        goto done;

    device = context->Device;

    for(i = 0;i < n;i++)
    {
        bool start_fading = false;
        ALsizei s;

        source = context->Device->source;

        voice = GetSourceVoice(source, context);
        switch(GetSourceState(source, voice))
        {
            case AL_PLAYING:
                assert(voice != NULL);
                /* A source that's already playing is restarted from the beginning. */
                voice->position = 0;
                voice->position_fraction = 0;
                goto finish_play;

            case AL_PAUSED:
                assert(voice != NULL);
                /* A source that's paused simply resumes. */
                voice->Playing = true;
                source->state = AL_PLAYING;
                goto finish_play;

            default:
                break;
        }

        /* Make sure this source isn't already active, and if not, look for an
         * unused voice to put it in.
         */
        for(j = 0;j < context->VoiceCount;j++)
        {
            if(context->voice->Source == NULL)
            {
                voice = context->voice;
                break;
            }
        }
        voice->Playing = false;

        UpdateSourceProps(source, voice, device->NumAuxSends);

        voice->position = 0;
        voice->position_fraction = 0;
        voice->NumChannels = device->Dry.NumChannels;

        /* Clear the stepping value so the mixer knows not to mix this until
         * the update gets applied.
         */
        voice->Step = 0;

        voice->Flags = start_fading ? VOICE_IS_FADING : 0;
        memset(voice->Direct.Params, 0, sizeof(voice->Direct.Params[0])*voice->NumChannels);
        for(s = 0;s < device->NumAuxSends;s++)
            memset(voice->Send[s].Params, 0, sizeof(voice->Send[s].Params[0])*voice->NumChannels);

        voice->Source = source;
        voice->Playing = true;
        source->state = AL_PLAYING;
    finish_play:
        ;
    }

done:
    ALCcontext_DecRef(context);
}


AL_API ALvoid AL_APIENTRY alSourceStop(ALuint source)
{
    alSourceStopv(1, &source);
}
AL_API ALvoid AL_APIENTRY alSourceStopv(ALsizei n, const ALuint *sources)
{
    ALCcontext *context;
    ALCdevice *device;
    ALsource *source;
    ALvoice *voice;
    ALsizei i;

    context = GetContextRef();
    if(!context) return;

    if(!(n == 1))
        goto done;

    device = context->Device;
    for(i = 0;i < n;i++)
    {
        source = context->Device->source;
        if((voice=GetSourceVoice(source, context)) != NULL)
        {
            voice->Source = NULL;
            voice->Playing = false;
        }
        if(source->state != AL_INITIAL)
            source->state = AL_STOPPED;
    }

done:
    ALCcontext_DecRef(context);
}

void InitSourceParams(ALsource *Source, ALsizei num_sends)
{
    ALsizei i;

    Source->Direct.Gain = 1.0f;
    Source->Direct.GainHF = 1.0f;
    Source->Direct.HFReference = LOWPASSFREQREF;
    Source->Direct.GainLF = 1.0f;
    Source->Direct.LFReference = HIGHPASSFREQREF;
    Source->Send = al_calloc(16, num_sends*sizeof(Source->Send[0]));
    for(i = 0;i < num_sends;i++)
    {
        Source->Send[i].Slot = NULL;
        Source->Send[i].Gain = 1.0f;
        Source->Send[i].GainHF = 1.0f;
        Source->Send[i].HFReference = LOWPASSFREQREF;
        Source->Send[i].GainLF = 1.0f;
        Source->Send[i].LFReference = HIGHPASSFREQREF;
    }

    Source->state = AL_INITIAL;
}

void DeinitSource(ALsource *source, ALsizei num_sends)
{
    ALsizei i;

    if(source->Send)
    {
        for(i = 0;i < num_sends;i++)
        {
            if(source->Send[i].Slot)
                source->Send[i].Slot->ref -= 1;
            source->Send[i].Slot = NULL;
        }
        al_free(source->Send);
        source->Send = NULL;
    }
}

void UpdateSourceProps(ALsource *source, ALvoice *voice, ALsizei num_sends)
{
    struct ALvoiceProps *props;
    struct ALvoiceProps *temp_props;
    ALsizei i;

    /* Get an unused property container, or allocate a new one as needed. */
    props = voice->FreeList;
    if(!props)
        props = al_calloc(16, FAM_SIZE(struct ALvoiceProps, Send, num_sends));
    else
    {
        struct ALvoiceProps *next;
        do {
            next = props->next;
        } while((voice->FreeList == props ? (voice->FreeList = next, true) : (props = voice->FreeList, false)) == 0);
    }

    /* Copy in current property values. */
    props->Direct.Gain = source->Direct.Gain;
    props->Direct.GainHF = source->Direct.GainHF;
    props->Direct.HFReference = source->Direct.HFReference;
    props->Direct.GainLF = source->Direct.GainLF;
    props->Direct.LFReference = source->Direct.LFReference;

    for(i = 0;i < num_sends;i++)
    {
        props->Send[i].Slot = source->Send[i].Slot;
        props->Send[i].Gain = source->Send[i].Gain;
        props->Send[i].GainHF = source->Send[i].GainHF;
        props->Send[i].HFReference = source->Send[i].HFReference;
        props->Send[i].GainLF = source->Send[i].GainLF;
        props->Send[i].LFReference = source->Send[i].LFReference;
    }

    /* Set the new container for updating internal parameters. */
    temp_props = props;
    props = voice->Update;
    voice->Update = temp_props;;
    if(props)
    {
        /* If there was an unused update container, put it back in the
         * freelist.
         */
        props->next = voice->FreeList;
    }
}

void UpdateAllSourceProps(ALCcontext *context)
{
    ALsizei num_sends = context->Device->NumAuxSends;
    ALsizei pos;

    for(pos = 0;pos < context->VoiceCount;pos++)
    {
        ALvoice *voice = context->voice;
        ALsource *source = voice->Source;
        if(source)
            UpdateSourceProps(source, voice, num_sends);
    }
}
