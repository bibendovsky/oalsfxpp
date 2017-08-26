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

#include "version.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <ctype.h>
#include <signal.h>

#include "alMain.h"
#include "alSource.h"
#include "alThunk.h"
#include "alSource.h"
#include "alBuffer.h"
#include "alAuxEffectSlot.h"
#include "alError.h"
#include "bformatdec.h"
#include "alu.h"

#include "compat.h"
#include "alstring.h"
#include "almalloc.h"

#include "backends/base.h"


void InitSourceParams(ALsource *Source, ALsizei num_sends);
void DeinitSource(ALsource *source, ALsizei num_sends);


/************************************************
 * Functions, enums, and errors
 ************************************************/
#define DECL(x) { #x, (ALCvoid*)(x) }
static const struct {
    const ALCchar *funcName;
    ALCvoid *address;
} alcFunctions[] = {
    DECL(alcCreateContext),
    DECL(alcMakeContextCurrent),
    DECL(alcProcessContext),
    DECL(alcSuspendContext),
    DECL(alcDestroyContext),
    DECL(alcGetCurrentContext),
    DECL(alcGetContextsDevice),
    DECL(alcOpenDevice),
    DECL(alcCloseDevice),
    DECL(alcGetError),
    DECL(alcIsExtensionPresent),
    DECL(alcGetProcAddress),
    DECL(alcGetEnumValue),
    DECL(alcGetString),
    DECL(alcGetIntegerv),
    DECL(alcCaptureOpenDevice),
    DECL(alcCaptureCloseDevice),
    DECL(alcCaptureStart),
    DECL(alcCaptureStop),
    DECL(alcCaptureSamples),

    DECL(alcSetThreadContext),
    DECL(alcGetThreadContext),

    DECL(alcLoopbackOpenDeviceSOFT),
    DECL(alcIsRenderFormatSupportedSOFT),
    DECL(alcRenderSamplesSOFT),

    DECL(alcIsAmbisonicFormatSupportedSOFT),

    DECL(alcDevicePauseSOFT),
    DECL(alcDeviceResumeSOFT),

    DECL(alcGetStringiSOFT),
    DECL(alcResetDeviceSOFT),

    DECL(alcGetInteger64vSOFT),

    DECL(alEnable),
    DECL(alDisable),
    DECL(alIsEnabled),
    DECL(alGetString),
    DECL(alGetBooleanv),
    DECL(alGetIntegerv),
    DECL(alGetFloatv),
    DECL(alGetDoublev),
    DECL(alGetBoolean),
    DECL(alGetInteger),
    DECL(alGetFloat),
    DECL(alGetDouble),
    DECL(alGetError),
    DECL(alIsExtensionPresent),
    DECL(alGetProcAddress),
    DECL(alGetEnumValue),
    DECL(alGenSources),
    DECL(alDeleteSources),
    DECL(alIsSource),
    DECL(alSourcef),
    DECL(alSource3f),
    DECL(alSourcefv),
    DECL(alSourcei),
    DECL(alSource3i),
    DECL(alSourceiv),
    DECL(alGetSourcef),
    DECL(alGetSource3f),
    DECL(alGetSourcefv),
    DECL(alGetSourcei),
    DECL(alGetSource3i),
    DECL(alGetSourceiv),
    DECL(alSourcePlayv),
    DECL(alSourceStopv),
    DECL(alSourceRewindv),
    DECL(alSourcePausev),
    DECL(alSourcePlay),
    DECL(alSourceStop),
    DECL(alSourceRewind),
    DECL(alSourcePause),
    DECL(alSourceQueueBuffers),
    DECL(alSourceUnqueueBuffers),

    DECL(alGenFilters),
    DECL(alDeleteFilters),
    DECL(alIsFilter),
    DECL(alFilteri),
    DECL(alFilteriv),
    DECL(alFilterf),
    DECL(alFilterfv),
    DECL(alGetFilteri),
    DECL(alGetFilteriv),
    DECL(alGetFilterf),
    DECL(alGetFilterfv),

    DECL(alDeferUpdatesSOFT),
    DECL(alProcessUpdatesSOFT),

    DECL(alSourcedSOFT),
    DECL(alSource3dSOFT),
    DECL(alSourcedvSOFT),
    DECL(alGetSourcedSOFT),
    DECL(alGetSource3dSOFT),
    DECL(alGetSourcedvSOFT),
    DECL(alSourcei64SOFT),
    DECL(alSource3i64SOFT),
    DECL(alSourcei64vSOFT),
    DECL(alGetSourcei64SOFT),
    DECL(alGetSource3i64SOFT),
    DECL(alGetSourcei64vSOFT),

    DECL(alGetStringiSOFT),
};
#undef DECL

#define DECL(x) { #x, (x) }
static const struct {
    const ALCchar *enumName;
    ALCenum value;
} alcEnumerations[] = {
    DECL(ALC_INVALID),
    DECL(ALC_FALSE),
    DECL(ALC_TRUE),

    DECL(ALC_MAJOR_VERSION),
    DECL(ALC_MINOR_VERSION),
    DECL(ALC_ATTRIBUTES_SIZE),
    DECL(ALC_ALL_ATTRIBUTES),
    DECL(ALC_DEFAULT_DEVICE_SPECIFIER),
    DECL(ALC_DEVICE_SPECIFIER),
    DECL(ALC_ALL_DEVICES_SPECIFIER),
    DECL(ALC_DEFAULT_ALL_DEVICES_SPECIFIER),
    DECL(ALC_EXTENSIONS),
    DECL(ALC_FREQUENCY),
    DECL(ALC_REFRESH),
    DECL(ALC_SYNC),
    DECL(ALC_MONO_SOURCES),
    DECL(ALC_STEREO_SOURCES),
    DECL(ALC_CAPTURE_DEVICE_SPECIFIER),
    DECL(ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER),
    DECL(ALC_CAPTURE_SAMPLES),
    DECL(ALC_CONNECTED),

    DECL(ALC_EFX_MAJOR_VERSION),
    DECL(ALC_EFX_MINOR_VERSION),
    DECL(ALC_MAX_AUXILIARY_SENDS),

    DECL(ALC_FORMAT_CHANNELS_SOFT),
    DECL(ALC_FORMAT_TYPE_SOFT),

    DECL(ALC_MONO_SOFT),
    DECL(ALC_STEREO_SOFT),
    DECL(ALC_QUAD_SOFT),
    DECL(ALC_5POINT1_SOFT),
    DECL(ALC_6POINT1_SOFT),
    DECL(ALC_7POINT1_SOFT),
    DECL(ALC_BFORMAT3D_SOFT),

    DECL(ALC_BYTE_SOFT),
    DECL(ALC_UNSIGNED_BYTE_SOFT),
    DECL(ALC_SHORT_SOFT),
    DECL(ALC_UNSIGNED_SHORT_SOFT),
    DECL(ALC_INT_SOFT),
    DECL(ALC_UNSIGNED_INT_SOFT),
    DECL(ALC_FLOAT_SOFT),

    DECL(ALC_HRTF_SOFT),
    DECL(ALC_DONT_CARE_SOFT),
    DECL(ALC_HRTF_STATUS_SOFT),
    DECL(ALC_HRTF_DISABLED_SOFT),
    DECL(ALC_HRTF_ENABLED_SOFT),
    DECL(ALC_HRTF_DENIED_SOFT),
    DECL(ALC_HRTF_REQUIRED_SOFT),
    DECL(ALC_HRTF_HEADPHONES_DETECTED_SOFT),
    DECL(ALC_HRTF_UNSUPPORTED_FORMAT_SOFT),
    DECL(ALC_NUM_HRTF_SPECIFIERS_SOFT),
    DECL(ALC_HRTF_SPECIFIER_SOFT),
    DECL(ALC_HRTF_ID_SOFT),

    DECL(ALC_AMBISONIC_LAYOUT_SOFT),
    DECL(ALC_AMBISONIC_SCALING_SOFT),
    DECL(ALC_AMBISONIC_ORDER_SOFT),
    DECL(ALC_ACN_SOFT),
    DECL(ALC_FUMA_SOFT),
    DECL(ALC_N3D_SOFT),
    DECL(ALC_SN3D_SOFT),

    DECL(ALC_OUTPUT_LIMITER_SOFT),

    DECL(ALC_NO_ERROR),
    DECL(ALC_INVALID_DEVICE),
    DECL(ALC_INVALID_CONTEXT),
    DECL(ALC_INVALID_ENUM),
    DECL(ALC_INVALID_VALUE),
    DECL(ALC_OUT_OF_MEMORY),


    DECL(AL_INVALID),
    DECL(AL_NONE),
    DECL(AL_FALSE),
    DECL(AL_TRUE),

    DECL(AL_SOURCE_RELATIVE),
    DECL(AL_CONE_INNER_ANGLE),
    DECL(AL_CONE_OUTER_ANGLE),
    DECL(AL_PITCH),
    DECL(AL_POSITION),
    DECL(AL_DIRECTION),
    DECL(AL_VELOCITY),
    DECL(AL_LOOPING),
    DECL(AL_BUFFER),
    DECL(AL_GAIN),
    DECL(AL_MIN_GAIN),
    DECL(AL_MAX_GAIN),
    DECL(AL_ORIENTATION),
    DECL(AL_REFERENCE_DISTANCE),
    DECL(AL_ROLLOFF_FACTOR),
    DECL(AL_CONE_OUTER_GAIN),
    DECL(AL_MAX_DISTANCE),
    DECL(AL_SEC_OFFSET),
    DECL(AL_SAMPLE_OFFSET),
    DECL(AL_BYTE_OFFSET),
    DECL(AL_SOURCE_TYPE),
    DECL(AL_STATIC),
    DECL(AL_STREAMING),
    DECL(AL_UNDETERMINED),
    DECL(AL_METERS_PER_UNIT),
    DECL(AL_LOOP_POINTS_SOFT),
    DECL(AL_DIRECT_CHANNELS_SOFT),

    DECL(AL_DIRECT_FILTER),
    DECL(AL_AUXILIARY_SEND_FILTER),
    DECL(AL_AIR_ABSORPTION_FACTOR),
    DECL(AL_ROOM_ROLLOFF_FACTOR),
    DECL(AL_CONE_OUTER_GAINHF),
    DECL(AL_DIRECT_FILTER_GAINHF_AUTO),
    DECL(AL_AUXILIARY_SEND_FILTER_GAIN_AUTO),
    DECL(AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO),

    DECL(AL_SOURCE_STATE),
    DECL(AL_INITIAL),
    DECL(AL_PLAYING),
    DECL(AL_PAUSED),
    DECL(AL_STOPPED),

    DECL(AL_BUFFERS_QUEUED),
    DECL(AL_BUFFERS_PROCESSED),

    DECL(AL_FORMAT_MONO8),
    DECL(AL_FORMAT_MONO16),
    DECL(AL_FORMAT_MONO_FLOAT32),
    DECL(AL_FORMAT_MONO_DOUBLE_EXT),
    DECL(AL_FORMAT_STEREO8),
    DECL(AL_FORMAT_STEREO16),
    DECL(AL_FORMAT_STEREO_FLOAT32),
    DECL(AL_FORMAT_STEREO_DOUBLE_EXT),
    DECL(AL_FORMAT_MONO_IMA4),
    DECL(AL_FORMAT_STEREO_IMA4),
    DECL(AL_FORMAT_MONO_MSADPCM_SOFT),
    DECL(AL_FORMAT_STEREO_MSADPCM_SOFT),
    DECL(AL_FORMAT_QUAD8_LOKI),
    DECL(AL_FORMAT_QUAD16_LOKI),
    DECL(AL_FORMAT_QUAD8),
    DECL(AL_FORMAT_QUAD16),
    DECL(AL_FORMAT_QUAD32),
    DECL(AL_FORMAT_51CHN8),
    DECL(AL_FORMAT_51CHN16),
    DECL(AL_FORMAT_51CHN32),
    DECL(AL_FORMAT_61CHN8),
    DECL(AL_FORMAT_61CHN16),
    DECL(AL_FORMAT_61CHN32),
    DECL(AL_FORMAT_71CHN8),
    DECL(AL_FORMAT_71CHN16),
    DECL(AL_FORMAT_71CHN32),
    DECL(AL_FORMAT_REAR8),
    DECL(AL_FORMAT_REAR16),
    DECL(AL_FORMAT_REAR32),
    DECL(AL_FORMAT_MONO_MULAW),
    DECL(AL_FORMAT_MONO_MULAW_EXT),
    DECL(AL_FORMAT_STEREO_MULAW),
    DECL(AL_FORMAT_STEREO_MULAW_EXT),
    DECL(AL_FORMAT_QUAD_MULAW),
    DECL(AL_FORMAT_51CHN_MULAW),
    DECL(AL_FORMAT_61CHN_MULAW),
    DECL(AL_FORMAT_71CHN_MULAW),
    DECL(AL_FORMAT_REAR_MULAW),
    DECL(AL_FORMAT_MONO_ALAW_EXT),
    DECL(AL_FORMAT_STEREO_ALAW_EXT),

    DECL(AL_FORMAT_BFORMAT2D_8),
    DECL(AL_FORMAT_BFORMAT2D_16),
    DECL(AL_FORMAT_BFORMAT2D_FLOAT32),
    DECL(AL_FORMAT_BFORMAT2D_MULAW),
    DECL(AL_FORMAT_BFORMAT3D_8),
    DECL(AL_FORMAT_BFORMAT3D_16),
    DECL(AL_FORMAT_BFORMAT3D_FLOAT32),
    DECL(AL_FORMAT_BFORMAT3D_MULAW),

    DECL(AL_MONO8_SOFT),
    DECL(AL_MONO16_SOFT),
    DECL(AL_MONO32F_SOFT),
    DECL(AL_STEREO8_SOFT),
    DECL(AL_STEREO16_SOFT),
    DECL(AL_STEREO32F_SOFT),
    DECL(AL_QUAD8_SOFT),
    DECL(AL_QUAD16_SOFT),
    DECL(AL_QUAD32F_SOFT),
    DECL(AL_REAR8_SOFT),
    DECL(AL_REAR16_SOFT),
    DECL(AL_REAR32F_SOFT),
    DECL(AL_5POINT1_8_SOFT),
    DECL(AL_5POINT1_16_SOFT),
    DECL(AL_5POINT1_32F_SOFT),
    DECL(AL_6POINT1_8_SOFT),
    DECL(AL_6POINT1_16_SOFT),
    DECL(AL_6POINT1_32F_SOFT),
    DECL(AL_7POINT1_8_SOFT),
    DECL(AL_7POINT1_16_SOFT),
    DECL(AL_7POINT1_32F_SOFT),
    DECL(AL_BFORMAT2D_8_SOFT),
    DECL(AL_BFORMAT2D_16_SOFT),
    DECL(AL_BFORMAT2D_32F_SOFT),
    DECL(AL_BFORMAT3D_8_SOFT),
    DECL(AL_BFORMAT3D_16_SOFT),
    DECL(AL_BFORMAT3D_32F_SOFT),

    DECL(AL_MONO_SOFT),
    DECL(AL_STEREO_SOFT),
    DECL(AL_QUAD_SOFT),
    DECL(AL_REAR_SOFT),
    DECL(AL_5POINT1_SOFT),
    DECL(AL_6POINT1_SOFT),
    DECL(AL_7POINT1_SOFT),
    DECL(AL_BFORMAT2D_SOFT),
    DECL(AL_BFORMAT3D_SOFT),

    DECL(AL_BYTE_SOFT),
    DECL(AL_UNSIGNED_BYTE_SOFT),
    DECL(AL_SHORT_SOFT),
    DECL(AL_UNSIGNED_SHORT_SOFT),
    DECL(AL_INT_SOFT),
    DECL(AL_UNSIGNED_INT_SOFT),
    DECL(AL_FLOAT_SOFT),
    DECL(AL_DOUBLE_SOFT),
    DECL(AL_BYTE3_SOFT),
    DECL(AL_UNSIGNED_BYTE3_SOFT),
    DECL(AL_MULAW_SOFT),

    DECL(AL_FREQUENCY),
    DECL(AL_BITS),
    DECL(AL_CHANNELS),
    DECL(AL_SIZE),
    DECL(AL_INTERNAL_FORMAT_SOFT),
    DECL(AL_BYTE_LENGTH_SOFT),
    DECL(AL_SAMPLE_LENGTH_SOFT),
    DECL(AL_SEC_LENGTH_SOFT),
    DECL(AL_UNPACK_BLOCK_ALIGNMENT_SOFT),
    DECL(AL_PACK_BLOCK_ALIGNMENT_SOFT),

    DECL(AL_SOURCE_RADIUS),

    DECL(AL_STEREO_ANGLES),

    DECL(AL_UNUSED),
    DECL(AL_PENDING),
    DECL(AL_PROCESSED),

    DECL(AL_NO_ERROR),
    DECL(AL_INVALID_NAME),
    DECL(AL_INVALID_ENUM),
    DECL(AL_INVALID_VALUE),
    DECL(AL_INVALID_OPERATION),
    DECL(AL_OUT_OF_MEMORY),

    DECL(AL_VENDOR),
    DECL(AL_VERSION),
    DECL(AL_RENDERER),
    DECL(AL_EXTENSIONS),

    DECL(AL_DOPPLER_FACTOR),
    DECL(AL_DOPPLER_VELOCITY),
    DECL(AL_DISTANCE_MODEL),
    DECL(AL_SPEED_OF_SOUND),
    DECL(AL_SOURCE_DISTANCE_MODEL),
    DECL(AL_DEFERRED_UPDATES_SOFT),
    DECL(AL_GAIN_LIMIT_SOFT),

    DECL(AL_INVERSE_DISTANCE),
    DECL(AL_INVERSE_DISTANCE_CLAMPED),
    DECL(AL_LINEAR_DISTANCE),
    DECL(AL_LINEAR_DISTANCE_CLAMPED),
    DECL(AL_EXPONENT_DISTANCE),
    DECL(AL_EXPONENT_DISTANCE_CLAMPED),

    DECL(AL_FILTER_TYPE),
    DECL(AL_FILTER_NULL),
    DECL(AL_FILTER_LOWPASS),
    DECL(AL_FILTER_HIGHPASS),
    DECL(AL_FILTER_BANDPASS),

    DECL(AL_LOWPASS_GAIN),
    DECL(AL_LOWPASS_GAINHF),

    DECL(AL_HIGHPASS_GAIN),
    DECL(AL_HIGHPASS_GAINLF),

    DECL(AL_BANDPASS_GAIN),
    DECL(AL_BANDPASS_GAINHF),
    DECL(AL_BANDPASS_GAINLF),

    DECL(AL_EFFECT_TYPE),
    DECL(AL_EFFECT_NULL),
    DECL(AL_EFFECT_REVERB),
    DECL(AL_EFFECT_EAXREVERB),
    DECL(AL_EFFECT_CHORUS),
    DECL(AL_EFFECT_DISTORTION),
    DECL(AL_EFFECT_ECHO),
    DECL(AL_EFFECT_FLANGER),
#if 0
    DECL(AL_EFFECT_FREQUENCY_SHIFTER),
    DECL(AL_EFFECT_VOCAL_MORPHER),
    DECL(AL_EFFECT_PITCH_SHIFTER),
#endif
    DECL(AL_EFFECT_RING_MODULATOR),
#if 0
    DECL(AL_EFFECT_AUTOWAH),
#endif
    DECL(AL_EFFECT_COMPRESSOR),
    DECL(AL_EFFECT_EQUALIZER),
    DECL(AL_EFFECT_DEDICATED_LOW_FREQUENCY_EFFECT),
    DECL(AL_EFFECT_DEDICATED_DIALOGUE),

    DECL(AL_EFFECTSLOT_EFFECT),
    DECL(AL_EFFECTSLOT_GAIN),
    DECL(AL_EFFECTSLOT_AUXILIARY_SEND_AUTO),
    DECL(AL_EFFECTSLOT_NULL),

    DECL(AL_EAXREVERB_DENSITY),
    DECL(AL_EAXREVERB_DIFFUSION),
    DECL(AL_EAXREVERB_GAIN),
    DECL(AL_EAXREVERB_GAINHF),
    DECL(AL_EAXREVERB_GAINLF),
    DECL(AL_EAXREVERB_DECAY_TIME),
    DECL(AL_EAXREVERB_DECAY_HFRATIO),
    DECL(AL_EAXREVERB_DECAY_LFRATIO),
    DECL(AL_EAXREVERB_REFLECTIONS_GAIN),
    DECL(AL_EAXREVERB_REFLECTIONS_DELAY),
    DECL(AL_EAXREVERB_REFLECTIONS_PAN),
    DECL(AL_EAXREVERB_LATE_REVERB_GAIN),
    DECL(AL_EAXREVERB_LATE_REVERB_DELAY),
    DECL(AL_EAXREVERB_LATE_REVERB_PAN),
    DECL(AL_EAXREVERB_ECHO_TIME),
    DECL(AL_EAXREVERB_ECHO_DEPTH),
    DECL(AL_EAXREVERB_MODULATION_TIME),
    DECL(AL_EAXREVERB_MODULATION_DEPTH),
    DECL(AL_EAXREVERB_AIR_ABSORPTION_GAINHF),
    DECL(AL_EAXREVERB_HFREFERENCE),
    DECL(AL_EAXREVERB_LFREFERENCE),
    DECL(AL_EAXREVERB_ROOM_ROLLOFF_FACTOR),
    DECL(AL_EAXREVERB_DECAY_HFLIMIT),

    DECL(AL_REVERB_DENSITY),
    DECL(AL_REVERB_DIFFUSION),
    DECL(AL_REVERB_GAIN),
    DECL(AL_REVERB_GAINHF),
    DECL(AL_REVERB_DECAY_TIME),
    DECL(AL_REVERB_DECAY_HFRATIO),
    DECL(AL_REVERB_REFLECTIONS_GAIN),
    DECL(AL_REVERB_REFLECTIONS_DELAY),
    DECL(AL_REVERB_LATE_REVERB_GAIN),
    DECL(AL_REVERB_LATE_REVERB_DELAY),
    DECL(AL_REVERB_AIR_ABSORPTION_GAINHF),
    DECL(AL_REVERB_ROOM_ROLLOFF_FACTOR),
    DECL(AL_REVERB_DECAY_HFLIMIT),

    DECL(AL_CHORUS_WAVEFORM),
    DECL(AL_CHORUS_PHASE),
    DECL(AL_CHORUS_RATE),
    DECL(AL_CHORUS_DEPTH),
    DECL(AL_CHORUS_FEEDBACK),
    DECL(AL_CHORUS_DELAY),

    DECL(AL_DISTORTION_EDGE),
    DECL(AL_DISTORTION_GAIN),
    DECL(AL_DISTORTION_LOWPASS_CUTOFF),
    DECL(AL_DISTORTION_EQCENTER),
    DECL(AL_DISTORTION_EQBANDWIDTH),

    DECL(AL_ECHO_DELAY),
    DECL(AL_ECHO_LRDELAY),
    DECL(AL_ECHO_DAMPING),
    DECL(AL_ECHO_FEEDBACK),
    DECL(AL_ECHO_SPREAD),

    DECL(AL_FLANGER_WAVEFORM),
    DECL(AL_FLANGER_PHASE),
    DECL(AL_FLANGER_RATE),
    DECL(AL_FLANGER_DEPTH),
    DECL(AL_FLANGER_FEEDBACK),
    DECL(AL_FLANGER_DELAY),

    DECL(AL_RING_MODULATOR_FREQUENCY),
    DECL(AL_RING_MODULATOR_HIGHPASS_CUTOFF),
    DECL(AL_RING_MODULATOR_WAVEFORM),

    DECL(AL_COMPRESSOR_ONOFF),

    DECL(AL_EQUALIZER_LOW_GAIN),
    DECL(AL_EQUALIZER_LOW_CUTOFF),
    DECL(AL_EQUALIZER_MID1_GAIN),
    DECL(AL_EQUALIZER_MID1_CENTER),
    DECL(AL_EQUALIZER_MID1_WIDTH),
    DECL(AL_EQUALIZER_MID2_GAIN),
    DECL(AL_EQUALIZER_MID2_CENTER),
    DECL(AL_EQUALIZER_MID2_WIDTH),
    DECL(AL_EQUALIZER_HIGH_GAIN),
    DECL(AL_EQUALIZER_HIGH_CUTOFF),

    DECL(AL_DEDICATED_GAIN),

    DECL(AL_NUM_RESAMPLERS_SOFT),
    DECL(AL_DEFAULT_RESAMPLER_SOFT),
    DECL(AL_SOURCE_RESAMPLER_SOFT),
    DECL(AL_RESAMPLER_NAME_SOFT),

    DECL(AL_SOURCE_SPATIALIZE_SOFT),
    DECL(AL_AUTO_SOFT),
};
#undef DECL

static const ALCchar alcNoError[] = "No Error";
static const ALCchar alcErrInvalidDevice[] = "Invalid Device";
static const ALCchar alcErrInvalidContext[] = "Invalid Context";
static const ALCchar alcErrInvalidEnum[] = "Invalid Enum";
static const ALCchar alcErrInvalidValue[] = "Invalid Value";
static const ALCchar alcErrOutOfMemory[] = "Out of Memory";


/************************************************
 * Global variables
 ************************************************/

/* Enumerated device names */
static const ALCchar alcDefaultName[] = "OpenAL Soft\0";

static al_string alcAllDevicesList;

/* Default is always the first in the list */
static ALCchar *alcDefaultAllDevicesSpecifier;

/* Default context extensions */
static const ALchar alExtList[] =
    "AL_EXT_ALAW AL_EXT_BFORMAT AL_EXT_DOUBLE AL_EXT_EXPONENT_DISTANCE "
    "AL_EXT_FLOAT32 AL_EXT_IMA4 AL_EXT_LINEAR_DISTANCE AL_EXT_MCFORMATS "
    "AL_EXT_MULAW AL_EXT_MULAW_BFORMAT AL_EXT_MULAW_MCFORMATS AL_EXT_OFFSET "
    "AL_EXT_source_distance_model AL_EXT_SOURCE_RADIUS AL_EXT_STEREO_ANGLES "
    "AL_LOKI_quadriphonic AL_SOFT_block_alignment AL_SOFT_deferred_updates "
    "AL_SOFT_direct_channels AL_SOFT_gain_clamp_ex AL_SOFT_loop_points "
    "AL_SOFT_MSADPCM AL_SOFT_source_latency AL_SOFT_source_length "
    "AL_SOFT_source_resampler AL_SOFT_source_spatialize";

static ALCenum LastNullDeviceError = ALC_NO_ERROR;

/* Thread-local current context */
static ALCcontext* LocalContext;
/* Process-wide current context */
static ALCcontext* GlobalContext = NULL;

/* Mixing thread piority level */
ALint RTPrioLevel;

FILE *LogFile;
#ifdef _DEBUG
enum LogLevel LogLevel = LogWarning;
#else
enum LogLevel LogLevel = LogError;
#endif

/* Flag to trap ALC device errors */
static ALCboolean TrapALCError = ALC_FALSE;

/* One-time configuration init control */
static ALCboolean alc_config_once = ALC_FALSE;

/* Default effect that applies to sources that don't have an effect on send 0 */
static ALeffect DefaultEffect;

/* Flag to specify if alcSuspendContext/alcProcessContext should defer/process
 * updates.
 */
static ALCboolean SuspendDefers = ALC_TRUE;


/************************************************
 * ALC information
 ************************************************/
static const ALCchar alcNoDeviceExtList[] =
    "ALC_ENUMERATE_ALL_EXT ALC_ENUMERATION_EXT ALC_EXT_CAPTURE "
    "ALC_EXT_thread_local_context ALC_SOFT_loopback";
static const ALCchar alcExtensionList[] =
    "ALC_ENUMERATE_ALL_EXT ALC_ENUMERATION_EXT ALC_EXT_CAPTURE "
    "ALC_EXT_DEDICATED ALC_EXT_disconnect ALC_EXT_EFX "
    "ALC_EXT_thread_local_context ALC_SOFTX_device_clock ALC_SOFT_HRTF "
    "ALC_SOFT_loopback ALC_SOFT_output_limiter ALC_SOFT_pause_device";
static const ALCint alcMajorVersion = 1;
static const ALCint alcMinorVersion = 1;

static const ALCint alcEFXMajorVersion = 1;
static const ALCint alcEFXMinorVersion = 0;


/************************************************
 * Device lists
 ************************************************/
static ALCdevice* DeviceList = NULL;


/************************************************
 * Library initialization
 ************************************************/
#if defined(_WIN32)
static void alc_init(void);
static void alc_deinit(void);
static void alc_deinit_safe(void);

#ifndef AL_LIBTYPE_STATIC
BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD reason, LPVOID lpReserved)
{
    switch(reason)
    {
        case DLL_PROCESS_ATTACH:
            /* Pin the DLL so we won't get unloaded until the process terminates */
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                               (WCHAR*)hModule, &hModule);
            alc_init();
            break;

        case DLL_THREAD_DETACH:
            break;

        case DLL_PROCESS_DETACH:
            if(!lpReserved)
                alc_deinit();
            else
                alc_deinit_safe();
            break;
    }
    return TRUE;
}
#elif defined(_MSC_VER)
#pragma section(".CRT$XCU",read)
static void alc_constructor(void);
static void alc_destructor(void);
__declspec(allocate(".CRT$XCU")) void (__cdecl* alc_constructor_)(void) = alc_constructor;

static void alc_constructor(void)
{
    atexit(alc_destructor);
    alc_init();
}

static void alc_destructor(void)
{
    alc_deinit();
}
#elif defined(HAVE_GCC_DESTRUCTOR)
static void alc_init(void) __attribute__((constructor));
static void alc_deinit(void) __attribute__((destructor));
#else
#error "No static initialization available on this platform!"
#endif

#elif defined(HAVE_GCC_DESTRUCTOR)

static void alc_init(void) __attribute__((constructor));
static void alc_deinit(void) __attribute__((destructor));

#else
#error "No global initialization available on this platform!"
#endif

static void ReleaseThreadCtx(void *ptr);
static void alc_init(void)
{
    const char *str;

    LogFile = stderr;

    AL_STRING_INIT(alcAllDevicesList);

    str = getenv("__ALSOFT_HALF_ANGLE_CONES");
    if(str && (strcasecmp(str, "true") == 0 || strtol(str, NULL, 0) == 1))
        ConeScale *= 0.5f;

    str = getenv("__ALSOFT_REVERSE_Z");
    if(str && (strcasecmp(str, "true") == 0 || strtol(str, NULL, 0) == 1))
        ZScale *= -1.0f;

    ThunkInit();
}

static void alc_initconfig(void)
{
    const char *devs, *str;
    ALuint capfilter;
    int i;

    str = getenv("ALSOFT_LOGLEVEL");
    if(str)
    {
        long lvl = strtol(str, NULL, 0);
        if(lvl >= NoLog && lvl <= LogRef)
            LogLevel = lvl;
    }

    str = getenv("ALSOFT_LOGFILE");
    if(str && str[0])
    {
        FILE *logfile = al_fopen(str, "wt");
        if(logfile) LogFile = logfile;
        else ERR("Failed to open log file '%s'\n", str);
    }

    TRACE("Initializing library v%s-%s %s\n", ALSOFT_VERSION,
          ALSOFT_GIT_COMMIT_HASH, ALSOFT_GIT_BRANCH);

    str = getenv("__ALSOFT_SUSPEND_CONTEXT");
    if(str && *str)
    {
        if(strcasecmp(str, "ignore") == 0)
        {
            SuspendDefers = ALC_FALSE;
            TRACE("Selected context suspend behavior, \"ignore\"\n");
        }
        else
            ERR("Unhandled context suspend behavior setting: \"%s\"\n", str);
    }

    capfilter = 0;
#if defined(HAVE_SSE4_1)
    capfilter |= CPU_CAP_SSE | CPU_CAP_SSE2 | CPU_CAP_SSE3 | CPU_CAP_SSE4_1;
#elif defined(HAVE_SSE3)
    capfilter |= CPU_CAP_SSE | CPU_CAP_SSE2 | CPU_CAP_SSE3;
#elif defined(HAVE_SSE2)
    capfilter |= CPU_CAP_SSE | CPU_CAP_SSE2;
#elif defined(HAVE_SSE)
    capfilter |= CPU_CAP_SSE;
#endif
#ifdef HAVE_NEON
    capfilter |= CPU_CAP_NEON;
#endif
    FillCPUCaps(capfilter);

#ifdef _WIN32
    RTPrioLevel = 1;
#else
    RTPrioLevel = 0;
#endif

    aluInitMixer();

    str = getenv("ALSOFT_TRAP_ERROR");
    if(str && (strcasecmp(str, "true") == 0 || strtol(str, NULL, 0) == 1))
    {
        TrapALError  = AL_TRUE;
        TrapALCError = AL_TRUE;
    }
    else
    {
        str = getenv("ALSOFT_TRAP_AL_ERROR");
        if(str && (strcasecmp(str, "true") == 0 || strtol(str, NULL, 0) == 1))
            TrapALError = AL_TRUE;

        str = getenv("ALSOFT_TRAP_ALC_ERROR");
        if(str && (strcasecmp(str, "true") == 0 || strtol(str, NULL, 0) == 1))
            TrapALCError = ALC_TRUE;
    }

    if((devs=getenv("ALSOFT_DRIVERS")) && devs[0])
    {
        size_t len;
        const char *next = devs;
        int endlist, delitem;

        i = 0;
        do {
            devs = next;
            while(isspace(devs[0]))
                devs++;
            next = strchr(devs, ',');

            delitem = (devs[0] == '-');
            if(devs[0] == '-') devs++;

            if(!devs[0] || devs[0] == ',')
            {
                endlist = 0;
                continue;
            }
            endlist = 1;

            len = (next ? ((size_t)(next-devs)) : strlen(devs));
            while(len > 0 && isspace(devs[len-1]))
                len--;
        } while(next++);
    }

    InitEffectFactoryMap();

    InitEffect(&DefaultEffect);
}
void DO_INITCONFIG()
{
    if (!alc_config_once)
    {
        alc_config_once = ALC_TRUE;
        alc_initconfig();
    }
}

#ifdef __ANDROID__
#include <jni.h>

static JavaVM *gJavaVM;
static pthread_key_t gJVMThreadKey;

static void CleanupJNIEnv(void* UNUSED(ptr))
{
    JCALL0(gJavaVM,DetachCurrentThread)();
}

void *Android_GetJNIEnv(void)
{
    if(!gJavaVM)
    {
        WARN("gJavaVM is NULL!\n");
        return NULL;
    }

    /* http://developer.android.com/guide/practices/jni.html
     *
     * All threads are Linux threads, scheduled by the kernel. They're usually
     * started from managed code (using Thread.start), but they can also be
     * created elsewhere and then attached to the JavaVM. For example, a thread
     * started with pthread_create can be attached with the JNI
     * AttachCurrentThread or AttachCurrentThreadAsDaemon functions. Until a
     * thread is attached, it has no JNIEnv, and cannot make JNI calls.
     * Attaching a natively-created thread causes a java.lang.Thread object to
     * be constructed and added to the "main" ThreadGroup, making it visible to
     * the debugger. Calling AttachCurrentThread on an already-attached thread
     * is a no-op.
     */
    JNIEnv *env = pthread_getspecific(gJVMThreadKey);
    if(!env)
    {
        int status = JCALL(gJavaVM,AttachCurrentThread)(&env, NULL);
        if(status < 0)
        {
            ERR("Failed to attach current thread\n");
            return NULL;
        }
        pthread_setspecific(gJVMThreadKey, env);
    }
    return env;
}

/* Automatically called by JNI. */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *jvm, void* UNUSED(reserved))
{
    void *env;
    int err;

    gJavaVM = jvm;
    if(JCALL(gJavaVM,GetEnv)(&env, JNI_VERSION_1_4) != JNI_OK)
    {
        ERR("Failed to get JNIEnv with JNI_VERSION_1_4\n");
        return JNI_ERR;
    }

    /* Create gJVMThreadKey so we can keep track of the JNIEnv assigned to each
     * thread. The JNIEnv *must* be detached before the thread is destroyed.
     */
    if((err=pthread_key_create(&gJVMThreadKey, CleanupJNIEnv)) != 0)
        ERR("pthread_key_create failed: %d\n", err);
    pthread_setspecific(gJVMThreadKey, env);
    return JNI_VERSION_1_4;
}
#endif


/************************************************
 * Library deinitialization
 ************************************************/
static void alc_cleanup(void)
{
    ALCdevice *dev;

    AL_STRING_DEINIT(alcAllDevicesList);

    free(alcDefaultAllDevicesSpecifier);
    alcDefaultAllDevicesSpecifier = NULL;

    dev = DeviceList;
    DeviceList = NULL;
    if(dev != NULL)
    {
        ERR("Device not closed\n");
    }

    DeinitEffectFactoryMap();
}

static void alc_deinit_safe(void)
{
    alc_cleanup();

    ThunkExit();

    if(LogFile != stderr)
        fclose(LogFile);
    LogFile = NULL;
}

static void alc_deinit(void)
{
    alc_cleanup();
    alc_deinit_safe();
}


/************************************************
 * Device enumeration
 ************************************************/
static void ProbeDevices(al_string *list, struct BackendInfo *backendinfo, enum DevProbe type)
{
    DO_INITCONFIG();

    alstr_clear(list);
}

static void AppendDevice(const ALCchar *name, al_string *devnames)
{
    size_t len = strlen(name);
    if(len > 0)
        alstr_append_range(devnames, name, name+len+1);
}
void AppendAllDevicesList(const ALCchar *name)
{ AppendDevice(name, &alcAllDevicesList); }


/************************************************
 * Device format information
 ************************************************/
const ALCchar *DevFmtTypeString(enum DevFmtType type)
{
    switch(type)
    {
    case DevFmtByte: return "Signed Byte";
    case DevFmtUByte: return "Unsigned Byte";
    case DevFmtShort: return "Signed Short";
    case DevFmtUShort: return "Unsigned Short";
    case DevFmtInt: return "Signed Int";
    case DevFmtUInt: return "Unsigned Int";
    case DevFmtFloat: return "Float";
    }
    return "(unknown type)";
}
const ALCchar *DevFmtChannelsString(enum DevFmtChannels chans)
{
    switch(chans)
    {
    case DevFmtMono: return "Mono";
    case DevFmtStereo: return "Stereo";
    case DevFmtQuad: return "Quadraphonic";
    case DevFmtX51: return "5.1 Surround";
    case DevFmtX51Rear: return "5.1 Surround (Rear)";
    case DevFmtX61: return "6.1 Surround";
    case DevFmtX71: return "7.1 Surround";
    case DevFmtAmbi3D: return "Ambisonic 3D";
    }
    return "(unknown channels)";
}

extern inline ALsizei FrameSizeFromDevFmt(enum DevFmtChannels chans, enum DevFmtType type, ALsizei ambiorder);
ALsizei BytesFromDevFmt(enum DevFmtType type)
{
    switch(type)
    {
    case DevFmtByte: return sizeof(ALbyte);
    case DevFmtUByte: return sizeof(ALubyte);
    case DevFmtShort: return sizeof(ALshort);
    case DevFmtUShort: return sizeof(ALushort);
    case DevFmtInt: return sizeof(ALint);
    case DevFmtUInt: return sizeof(ALuint);
    case DevFmtFloat: return sizeof(ALfloat);
    }
    return 0;
}
ALsizei ChannelsFromDevFmt(enum DevFmtChannels chans, ALsizei ambiorder)
{
    switch(chans)
    {
    case DevFmtMono: return 1;
    case DevFmtStereo: return 2;
    case DevFmtQuad: return 4;
    case DevFmtX51: return 6;
    case DevFmtX51Rear: return 6;
    case DevFmtX61: return 7;
    case DevFmtX71: return 8;
    case DevFmtAmbi3D: return (ambiorder >= 3) ? 16 :
                              (ambiorder == 2) ? 9 :
                              (ambiorder == 1) ? 4 : 1;
    }
    return 0;
}

static ALboolean DecomposeDevFormat(ALenum format, enum DevFmtChannels *chans,
                                    enum DevFmtType *type)
{
    static const struct {
        ALenum format;
        enum DevFmtChannels channels;
        enum DevFmtType type;
    } list[] = {
        { AL_FORMAT_MONO8,        DevFmtMono, DevFmtUByte },
        { AL_FORMAT_MONO16,       DevFmtMono, DevFmtShort },
        { AL_FORMAT_MONO_FLOAT32, DevFmtMono, DevFmtFloat },

        { AL_FORMAT_STEREO8,        DevFmtStereo, DevFmtUByte },
        { AL_FORMAT_STEREO16,       DevFmtStereo, DevFmtShort },
        { AL_FORMAT_STEREO_FLOAT32, DevFmtStereo, DevFmtFloat },

        { AL_FORMAT_QUAD8,  DevFmtQuad, DevFmtUByte },
        { AL_FORMAT_QUAD16, DevFmtQuad, DevFmtShort },
        { AL_FORMAT_QUAD32, DevFmtQuad, DevFmtFloat },

        { AL_FORMAT_51CHN8,  DevFmtX51, DevFmtUByte },
        { AL_FORMAT_51CHN16, DevFmtX51, DevFmtShort },
        { AL_FORMAT_51CHN32, DevFmtX51, DevFmtFloat },

        { AL_FORMAT_61CHN8,  DevFmtX61, DevFmtUByte },
        { AL_FORMAT_61CHN16, DevFmtX61, DevFmtShort },
        { AL_FORMAT_61CHN32, DevFmtX61, DevFmtFloat },

        { AL_FORMAT_71CHN8,  DevFmtX71, DevFmtUByte },
        { AL_FORMAT_71CHN16, DevFmtX71, DevFmtShort },
        { AL_FORMAT_71CHN32, DevFmtX71, DevFmtFloat },
    };
    ALuint i;

    for(i = 0;i < COUNTOF(list);i++)
    {
        if(list[i].format == format)
        {
            *chans = list[i].channels;
            *type  = list[i].type;
            return AL_TRUE;
        }
    }

    return AL_FALSE;
}

static ALCboolean IsValidALCType(ALCenum type)
{
    switch(type)
    {
        case ALC_BYTE_SOFT:
        case ALC_UNSIGNED_BYTE_SOFT:
        case ALC_SHORT_SOFT:
        case ALC_UNSIGNED_SHORT_SOFT:
        case ALC_INT_SOFT:
        case ALC_UNSIGNED_INT_SOFT:
        case ALC_FLOAT_SOFT:
            return ALC_TRUE;
    }
    return ALC_FALSE;
}

static ALCboolean IsValidALCChannels(ALCenum channels)
{
    switch(channels)
    {
        case ALC_MONO_SOFT:
        case ALC_STEREO_SOFT:
        case ALC_QUAD_SOFT:
        case ALC_5POINT1_SOFT:
        case ALC_6POINT1_SOFT:
        case ALC_7POINT1_SOFT:
        case ALC_BFORMAT3D_SOFT:
            return ALC_TRUE;
    }
    return ALC_FALSE;
}

static ALCboolean IsValidAmbiLayout(ALCenum layout)
{
    switch(layout)
    {
        case ALC_ACN_SOFT:
        case ALC_FUMA_SOFT:
            return ALC_TRUE;
    }
    return ALC_FALSE;
}

static ALCboolean IsValidAmbiScaling(ALCenum scaling)
{
    switch(scaling)
    {
        case ALC_N3D_SOFT:
        case ALC_SN3D_SOFT:
        case ALC_FUMA_SOFT:
            return ALC_TRUE;
    }
    return ALC_FALSE;
}

/************************************************
 * Miscellaneous ALC helpers
 ************************************************/

/* SetDefaultWFXChannelOrder
 *
 * Sets the default channel order used by WaveFormatEx.
 */
void SetDefaultWFXChannelOrder(ALCdevice *device)
{
    ALsizei i;

    for(i = 0;i < MAX_OUTPUT_CHANNELS;i++)
        device->RealOut.ChannelName[i] = InvalidChannel;

    switch(device->FmtChans)
    {
    case DevFmtMono:
        device->RealOut.ChannelName[0] = FrontCenter;
        break;
    case DevFmtStereo:
        device->RealOut.ChannelName[0] = FrontLeft;
        device->RealOut.ChannelName[1] = FrontRight;
        break;
    case DevFmtQuad:
        device->RealOut.ChannelName[0] = FrontLeft;
        device->RealOut.ChannelName[1] = FrontRight;
        device->RealOut.ChannelName[2] = BackLeft;
        device->RealOut.ChannelName[3] = BackRight;
        break;
    case DevFmtX51:
        device->RealOut.ChannelName[0] = FrontLeft;
        device->RealOut.ChannelName[1] = FrontRight;
        device->RealOut.ChannelName[2] = FrontCenter;
        device->RealOut.ChannelName[3] = LFE;
        device->RealOut.ChannelName[4] = SideLeft;
        device->RealOut.ChannelName[5] = SideRight;
        break;
    case DevFmtX51Rear:
        device->RealOut.ChannelName[0] = FrontLeft;
        device->RealOut.ChannelName[1] = FrontRight;
        device->RealOut.ChannelName[2] = FrontCenter;
        device->RealOut.ChannelName[3] = LFE;
        device->RealOut.ChannelName[4] = BackLeft;
        device->RealOut.ChannelName[5] = BackRight;
        break;
    case DevFmtX61:
        device->RealOut.ChannelName[0] = FrontLeft;
        device->RealOut.ChannelName[1] = FrontRight;
        device->RealOut.ChannelName[2] = FrontCenter;
        device->RealOut.ChannelName[3] = LFE;
        device->RealOut.ChannelName[4] = BackCenter;
        device->RealOut.ChannelName[5] = SideLeft;
        device->RealOut.ChannelName[6] = SideRight;
        break;
    case DevFmtX71:
        device->RealOut.ChannelName[0] = FrontLeft;
        device->RealOut.ChannelName[1] = FrontRight;
        device->RealOut.ChannelName[2] = FrontCenter;
        device->RealOut.ChannelName[3] = LFE;
        device->RealOut.ChannelName[4] = BackLeft;
        device->RealOut.ChannelName[5] = BackRight;
        device->RealOut.ChannelName[6] = SideLeft;
        device->RealOut.ChannelName[7] = SideRight;
        break;
    }
}

/* SetDefaultChannelOrder
 *
 * Sets the default channel order used by most non-WaveFormatEx-based APIs.
 */
void SetDefaultChannelOrder(ALCdevice *device)
{
    ALsizei i;

    for(i = 0;i < MAX_OUTPUT_CHANNELS;i++)
        device->RealOut.ChannelName[i] = InvalidChannel;

    switch(device->FmtChans)
    {
    case DevFmtX51Rear:
        device->RealOut.ChannelName[0] = FrontLeft;
        device->RealOut.ChannelName[1] = FrontRight;
        device->RealOut.ChannelName[2] = BackLeft;
        device->RealOut.ChannelName[3] = BackRight;
        device->RealOut.ChannelName[4] = FrontCenter;
        device->RealOut.ChannelName[5] = LFE;
        return;
    case DevFmtX71:
        device->RealOut.ChannelName[0] = FrontLeft;
        device->RealOut.ChannelName[1] = FrontRight;
        device->RealOut.ChannelName[2] = BackLeft;
        device->RealOut.ChannelName[3] = BackRight;
        device->RealOut.ChannelName[4] = FrontCenter;
        device->RealOut.ChannelName[5] = LFE;
        device->RealOut.ChannelName[6] = SideLeft;
        device->RealOut.ChannelName[7] = SideRight;
        return;

    /* Same as WFX order */
    case DevFmtMono:
    case DevFmtStereo:
    case DevFmtQuad:
    case DevFmtX51:
    case DevFmtX61:
    case DevFmtAmbi3D:
        SetDefaultWFXChannelOrder(device);
        break;
    }
}

extern inline ALint GetChannelIndex(const enum Channel names[MAX_OUTPUT_CHANNELS], enum Channel chan);


/* ALCcontext_DeferUpdates
 *
 * Defers/suspends updates for the given context's listener and sources. This
 * does *NOT* stop mixing, but rather prevents certain property changes from
 * taking effect.
 */
void ALCcontext_DeferUpdates(ALCcontext *context)
{
}

/* ALCcontext_ProcessUpdates
 *
 * Resumes update processing after being deferred.
 */
void ALCcontext_ProcessUpdates(ALCcontext *context)
{
    /* Tell the mixer to stop applying updates, then wait for any active
        * updating to finish, before providing updates.
        */
    context->HoldUpdates = AL_TRUE;

    UpdateAllEffectSlotProps(context);
    UpdateAllSourceProps(context);

    /* Now with all updates declared, let the mixer continue applying them
        * so they all happen at once.
        */
    context->HoldUpdates = AL_FALSE;
}


/* alcSetError
 *
 * Stores the latest ALC device error
 */
static void alcSetError(ALCdevice *device, ALCenum errorCode)
{
    WARN("Error generated on device %p, code 0x%04x\n", device, errorCode);
    if(TrapALCError)
    {
#ifdef _WIN32
        /* DebugBreak() will cause an exception if there is no debugger */
        if(IsDebuggerPresent())
            DebugBreak();
#elif defined(SIGTRAP)
        raise(SIGTRAP);
#endif
    }

    if(device)
        device->LastError = errorCode;
    else
        LastNullDeviceError = errorCode;
}


struct Compressor *CreateDeviceLimiter(const ALCdevice *device)
{
    return CompressorInit(0.0f, 0.0f, AL_FALSE, AL_TRUE, 0.0f, 0.0f, 0.5f, 2.0f,
                          0.0f, -3.0f, 3.0f, device->Frequency);
}

/* UpdateClockBase
 *
 * Updates the device's base clock time with however many samples have been
 * done. This is used so frequency changes on the device don't cause the time
 * to jump forward or back. Must not be called while the device is running/
 * mixing.
 */
static inline void UpdateClockBase(ALCdevice *device)
{
    device->MixCount += 1;
    device->ClockBase += device->SamplesDone * DEVICE_CLOCK_RES / device->Frequency;
    device->SamplesDone = 0;
    device->MixCount += 1;
}

/* UpdateDeviceParams
 *
 * Updates device parameters according to the attribute list (caller is
 * responsible for holding the list lock).
 */
static ALCenum UpdateDeviceParams(ALCdevice *device, const ALCint *attrList)
{
    ALCenum gainLimiter = device->Limiter ? ALC_TRUE : ALC_FALSE;
    const ALsizei old_sends = device->NumAuxSends;
    ALsizei new_sends = device->NumAuxSends;
    enum DevFmtChannels oldChans;
    enum DevFmtType oldType;
    ALboolean update_failed;
    ALCcontext *context;
    ALCuint oldFreq;
    size_t size;
    ALCsizei i;

    // Check for attributes
    if(device->Type == Loopback)
    {
        ALCsizei numMono, numStereo, numSends;
        ALCenum alayout = AL_NONE;
        ALCenum ascale = AL_NONE;
        ALCenum schans = AL_NONE;
        ALCenum stype = AL_NONE;
        ALCsizei attrIdx = 0;
        ALCsizei aorder = 0;
        ALCuint freq = 0;

        if(!attrList)
        {
            WARN("Missing attributes for loopback device\n");
            return ALC_INVALID_VALUE;
        }

        numMono = device->NumMonoSources;
        numStereo = device->NumStereoSources;
        numSends = old_sends;

#define TRACE_ATTR(a, v) TRACE("Loopback %s = %d\n", #a, v)
        while(attrList[attrIdx])
        {
            switch(attrList[attrIdx])
            {
                case ALC_FORMAT_CHANNELS_SOFT:
                    schans = attrList[attrIdx + 1];
                    TRACE_ATTR(ALC_FORMAT_CHANNELS_SOFT, schans);
                    if(!IsValidALCChannels(schans))
                        return ALC_INVALID_VALUE;
                    break;

                case ALC_FORMAT_TYPE_SOFT:
                    stype = attrList[attrIdx + 1];
                    TRACE_ATTR(ALC_FORMAT_TYPE_SOFT, stype);
                    if(!IsValidALCType(stype))
                        return ALC_INVALID_VALUE;
                    break;

                case ALC_FREQUENCY:
                    freq = attrList[attrIdx + 1];
                    TRACE_ATTR(ALC_FREQUENCY, freq);
                    if(freq < MIN_OUTPUT_RATE)
                        return ALC_INVALID_VALUE;
                    break;

                case ALC_AMBISONIC_LAYOUT_SOFT:
                    alayout = attrList[attrIdx + 1];
                    TRACE_ATTR(ALC_AMBISONIC_LAYOUT_SOFT, alayout);
                    if(!IsValidAmbiLayout(alayout))
                        return ALC_INVALID_VALUE;
                    break;

                case ALC_AMBISONIC_SCALING_SOFT:
                    ascale = attrList[attrIdx + 1];
                    TRACE_ATTR(ALC_AMBISONIC_SCALING_SOFT, ascale);
                    if(!IsValidAmbiScaling(ascale))
                        return ALC_INVALID_VALUE;
                    break;

                case ALC_AMBISONIC_ORDER_SOFT:
                    aorder = attrList[attrIdx + 1];
                    TRACE_ATTR(ALC_AMBISONIC_ORDER_SOFT, aorder);
                    if(aorder < 1 || aorder > MAX_AMBI_ORDER)
                        return ALC_INVALID_VALUE;
                    break;

                case ALC_MONO_SOURCES:
                    numMono = attrList[attrIdx + 1];
                    TRACE_ATTR(ALC_MONO_SOURCES, numMono);
                    numMono = maxi(numMono, 0);
                    break;

                case ALC_STEREO_SOURCES:
                    numStereo = attrList[attrIdx + 1];
                    TRACE_ATTR(ALC_STEREO_SOURCES, numStereo);
                    numStereo = maxi(numStereo, 0);
                    break;

                case ALC_MAX_AUXILIARY_SENDS:
                    numSends = attrList[attrIdx + 1];
                    TRACE_ATTR(ALC_MAX_AUXILIARY_SENDS, numSends);
                    numSends = clampi(numSends, 0, MAX_SENDS);
                    break;

                case ALC_OUTPUT_LIMITER_SOFT:
                    gainLimiter = attrList[attrIdx + 1];
                    TRACE_ATTR(ALC_OUTPUT_LIMITER_SOFT, gainLimiter);
                    break;

                default:
                    TRACE("Loopback 0x%04X = %d (0x%x)\n", attrList[attrIdx],
                          attrList[attrIdx + 1], attrList[attrIdx + 1]);
                    break;
            }

            attrIdx += 2;
        }
#undef TRACE_ATTR

        if(!schans || !stype || !freq)
        {
            WARN("Missing format for loopback device\n");
            return ALC_INVALID_VALUE;
        }
        if(schans == ALC_BFORMAT3D_SOFT && (!alayout || !ascale || !aorder))
        {
            WARN("Missing ambisonic info for loopback device\n");
            return ALC_INVALID_VALUE;
        }

        device->Flags &= ~DEVICE_RUNNING;

        UpdateClockBase(device);

        device->Frequency = freq;
        device->FmtChans = schans;
        device->FmtType = stype;

        if(numMono > INT_MAX-numStereo)
            numMono = INT_MAX-numStereo;
        numMono += numStereo;
        numMono = maxi(numMono, 256);
        numStereo = mini(numStereo, numMono);
        numMono -= numStereo;
        device->SourcesMax = numMono + numStereo;

        device->NumMonoSources = numMono;
        device->NumStereoSources = numStereo;

        new_sends = numSends;
    }
    else if(attrList && attrList[0])
    {
        ALCsizei numMono, numStereo, numSends;
        ALCsizei attrIdx = 0;
        ALCuint freq;

        /* If a context is already running on the device, stop playback so the
         * device attributes can be updated. */
        device->Flags &= ~DEVICE_RUNNING;

        UpdateClockBase(device);

        freq = device->Frequency;
        numMono = device->NumMonoSources;
        numStereo = device->NumStereoSources;
        numSends = old_sends;

#define TRACE_ATTR(a, v) TRACE("%s = %d\n", #a, v)
        while(attrList[attrIdx])
        {
            switch(attrList[attrIdx])
            {
                case ALC_FREQUENCY:
                    freq = attrList[attrIdx + 1];
                    TRACE_ATTR(ALC_FREQUENCY, freq);
                    device->Flags |= DEVICE_FREQUENCY_REQUEST;
                    break;

                case ALC_MONO_SOURCES:
                    numMono = attrList[attrIdx + 1];
                    TRACE_ATTR(ALC_MONO_SOURCES, numMono);
                    numMono = maxi(numMono, 0);
                    break;

                case ALC_STEREO_SOURCES:
                    numStereo = attrList[attrIdx + 1];
                    TRACE_ATTR(ALC_STEREO_SOURCES, numStereo);
                    numStereo = maxi(numStereo, 0);
                    break;

                case ALC_MAX_AUXILIARY_SENDS:
                    numSends = attrList[attrIdx + 1];
                    TRACE_ATTR(ALC_MAX_AUXILIARY_SENDS, numSends);
                    numSends = clampi(numSends, 0, MAX_SENDS);
                    break;

                case ALC_OUTPUT_LIMITER_SOFT:
                    gainLimiter = attrList[attrIdx + 1];
                    TRACE_ATTR(ALC_OUTPUT_LIMITER_SOFT, gainLimiter);
                    break;

                default:
                    TRACE("0x%04X = %d (0x%x)\n", attrList[attrIdx],
                          attrList[attrIdx + 1], attrList[attrIdx + 1]);
                    break;
            }

            attrIdx += 2;
        }
#undef TRACE_ATTR

        freq = maxu(freq, MIN_OUTPUT_RATE);

        device->UpdateSize = (ALuint64)device->UpdateSize * freq /
                             device->Frequency;
        /* SSE and Neon do best with the update size being a multiple of 4 */
        if((CPUCapFlags&(CPU_CAP_SSE|CPU_CAP_NEON)) != 0)
            device->UpdateSize = (device->UpdateSize+3)&~3;

        device->Frequency = freq;

        if(numMono > INT_MAX-numStereo)
            numMono = INT_MAX-numStereo;
        numMono += numStereo;
        numMono = maxi(numMono, 256);
        numStereo = mini(numStereo, numMono);
        numMono -= numStereo;
        device->SourcesMax = numMono + numStereo;

        device->NumMonoSources = numMono;
        device->NumStereoSources = numStereo;

        new_sends = numSends;
    }

    if((device->Flags&DEVICE_RUNNING))
        return ALC_NO_ERROR;

    al_free(device->ChannelDelay[0].Buffer);
    for(i = 0;i < MAX_OUTPUT_CHANNELS;i++)
    {
        device->ChannelDelay[i].Length = 0;
        device->ChannelDelay[i].Buffer = NULL;
    }

    al_free(device->Dry.Buffer);
    device->Dry.Buffer = NULL;
    device->Dry.NumChannels = 0;
    device->FOAOut.Buffer = NULL;
    device->FOAOut.NumChannels = 0;
    device->RealOut.Buffer = NULL;
    device->RealOut.NumChannels = 0;

    UpdateClockBase(device);

    oldFreq  = device->Frequency;
    oldChans = device->FmtChans;
    oldType  = device->FmtType;

    TRACE("Pre-reset: %s%s, %s%s, %s%uhz, %u update size x%d\n",
        (device->Flags&DEVICE_CHANNELS_REQUEST)?"*":"", DevFmtChannelsString(device->FmtChans),
        (device->Flags&DEVICE_SAMPLE_TYPE_REQUEST)?"*":"", DevFmtTypeString(device->FmtType),
        (device->Flags&DEVICE_FREQUENCY_REQUEST)?"*":"", device->Frequency,
        device->UpdateSize, device->NumUpdates
    );

    if(device->FmtChans != oldChans && (device->Flags&DEVICE_CHANNELS_REQUEST))
    {
        ERR("Failed to set %s, got %s instead\n", DevFmtChannelsString(oldChans),
            DevFmtChannelsString(device->FmtChans));
        device->Flags &= ~DEVICE_CHANNELS_REQUEST;
    }
    if(device->FmtType != oldType && (device->Flags&DEVICE_SAMPLE_TYPE_REQUEST))
    {
        ERR("Failed to set %s, got %s instead\n", DevFmtTypeString(oldType),
            DevFmtTypeString(device->FmtType));
        device->Flags &= ~DEVICE_SAMPLE_TYPE_REQUEST;
    }
    if(device->Frequency != oldFreq && (device->Flags&DEVICE_FREQUENCY_REQUEST))
    {
        ERR("Failed to set %uhz, got %uhz instead\n", oldFreq, device->Frequency);
        device->Flags &= ~DEVICE_FREQUENCY_REQUEST;
    }

    if((device->UpdateSize&3) != 0)
    {
        if((CPUCapFlags&CPU_CAP_SSE))
            WARN("SSE performs best with multiple of 4 update sizes (%u)\n", device->UpdateSize);
        if((CPUCapFlags&CPU_CAP_NEON))
            WARN("NEON performs best with multiple of 4 update sizes (%u)\n", device->UpdateSize);
    }

    TRACE("Post-reset: %s, %s, %uhz, %u update size x%d\n",
        DevFmtChannelsString(device->FmtChans), DevFmtTypeString(device->FmtType),
        device->Frequency, device->UpdateSize, device->NumUpdates
    );

    aluInitRenderer(device);
    TRACE("Channel config, Dry: %d, FOA: %d, Real: %d\n", device->Dry.NumChannels,
          device->FOAOut.NumChannels, device->RealOut.NumChannels);

    /* Allocate extra channels for any post-filter output. */
    size = (device->Dry.NumChannels + device->FOAOut.NumChannels +
            device->RealOut.NumChannels)*sizeof(device->Dry.Buffer[0]);

    TRACE("Allocating "SZFMT" channels, "SZFMT" bytes\n", size/sizeof(device->Dry.Buffer[0]), size);
    device->Dry.Buffer = al_calloc(16, size);
    if(!device->Dry.Buffer)
    {
        ERR("Failed to allocate "SZFMT" bytes for mix buffer\n", size);
        return ALC_INVALID_DEVICE;
    }

    if(device->RealOut.NumChannels != 0)
        device->RealOut.Buffer = device->Dry.Buffer + device->Dry.NumChannels +
                                 device->FOAOut.NumChannels;
    else
    {
        device->RealOut.Buffer = device->Dry.Buffer;
        device->RealOut.NumChannels = device->Dry.NumChannels;
    }

    if(device->FOAOut.NumChannels != 0)
        device->FOAOut.Buffer = device->Dry.Buffer + device->Dry.NumChannels;
    else
    {
        device->FOAOut.Buffer = device->Dry.Buffer;
        device->FOAOut.NumChannels = device->Dry.NumChannels;
    }

    device->NumAuxSends = new_sends;
    TRACE("Max sources: %d (%d + %d), effect slots: %d, sends: %d\n",
          device->SourcesMax, device->NumMonoSources, device->NumStereoSources,
          device->AuxiliaryEffectSlotMax, device->NumAuxSends);

    TRACE("Dithering disabled\n");

    /* Valid values for gainLimiter are ALC_DONT_CARE_SOFT, ALC_TRUE, and
     * ALC_FALSE. We default to on, so ALC_DONT_CARE_SOFT is the same as
     * ALC_TRUE.
     */
    if(gainLimiter != ALC_FALSE)
    {
        if(!device->Limiter || device->Frequency != GetCompressorSampleRate(device->Limiter))
        {
            al_free(device->Limiter);
            device->Limiter = CreateDeviceLimiter(device);
        }
    }
    else
    {
        al_free(device->Limiter);
        device->Limiter = NULL;
    }
    TRACE("Output limiter %s\n", device->Limiter ? "enabled" : "disabled");

    /* Need to delay returning failure until replacement Send arrays have been
     * allocated with the appropriate size.
     */
    update_failed = AL_FALSE;
    START_MIXER_MODE();
    context = device->ContextList;
    if(context)
    {
        ALsizei pos;

        if(context->DefaultSlot)
        {
            ALeffectslot *slot = context->DefaultSlot;
            ALeffectState *state = slot->Effect.State;

            state->OutBuffer = device->Dry.Buffer;
            state->OutChannels = device->Dry.NumChannels;
            if(V(state,deviceUpdate)(device) == AL_FALSE)
                update_failed = AL_TRUE;
            else
                UpdateEffectSlotProps(slot);
        }

        for(pos = 0;pos < context->EffectSlotMap.size;pos++)
        {
            ALeffectslot *slot = context->EffectSlotMap.values[pos];
            ALeffectState *state = slot->Effect.State;

            state->OutBuffer = device->Dry.Buffer;
            state->OutChannels = device->Dry.NumChannels;
            if(V(state,deviceUpdate)(device) == AL_FALSE)
                update_failed = AL_TRUE;
            else
                UpdateEffectSlotProps(slot);
        }

        RelimitUIntMapNoLock(&context->SourceMap, device->SourcesMax);
        for(pos = 0;pos < context->SourceMap.size;pos++)
        {
            ALsource *source = context->SourceMap.values[pos];

            if(old_sends != device->NumAuxSends)
            {
                ALvoid *sends = al_calloc(16, device->NumAuxSends*sizeof(source->Send[0]));
                ALsizei s;

                memcpy(sends, source->Send,
                    mini(device->NumAuxSends, old_sends)*sizeof(source->Send[0])
                );
                for(s = device->NumAuxSends;s < old_sends;s++)
                {
                    if(source->Send[s].Slot)
                        source->Send[s].Slot->ref -= 1;
                    source->Send[s].Slot = NULL;
                }
                al_free(source->Send);
                source->Send = sends;
                for(s = old_sends;s < device->NumAuxSends;s++)
                {
                    source->Send[s].Slot = NULL;
                    source->Send[s].Gain = 1.0f;
                    source->Send[s].GainHF = 1.0f;
                    source->Send[s].HFReference = LOWPASSFREQREF;
                    source->Send[s].GainLF = 1.0f;
                    source->Send[s].LFReference = HIGHPASSFREQREF;
                }
            }

            source->PropsClean = 0;
        }
        AllocateVoices(context, context->MaxVoices, old_sends);
        for(pos = 0;pos < context->VoiceCount;pos++)
        {
            ALvoice *voice = context->Voices[pos];
            struct ALvoiceProps *props;

            /* Clear any pre-existing voice property structs, in case the
             * number of auxiliary sends changed. Active sources will have
             * updates respecified in UpdateAllSourceProps.
             */
            props = voice->Update;
            voice->Update = NULL;
            al_free(props);

            props = voice->FreeList;
            voice->FreeList = NULL;
            while(props)
            {
                struct ALvoiceProps *next = props->next;
                al_free(props);
                props = next;
            }

            if(voice->Source == NULL)
                continue;
        }

        UpdateAllSourceProps(context);
    }
    END_MIXER_MODE();
    if(update_failed)
        return ALC_INVALID_DEVICE;

    if(!(device->Flags&DEVICE_PAUSED))
    {
        device->Flags |= DEVICE_RUNNING;
    }

    return ALC_NO_ERROR;
}

/* FreeDevice
 *
 * Frees the device structure, and destroys any objects the app failed to
 * delete. Called once there's no more references on the device.
 */
static ALCvoid FreeDevice(ALCdevice *device)
{
    ALsizei i;

    TRACE("%p\n", device);

    if(device->FilterMap.size > 0)
    {
        WARN("(%p) Deleting %d Filter%s\n", device, device->FilterMap.size,
             (device->FilterMap.size==1)?"":"s");
        ReleaseALFilters(device);
    }
    ResetUIntMap(&device->FilterMap);

    al_free(device->effect);

    DeinitEffectSlot(device->effect_slot);
    al_free(device->effect_slot);

    DeinitSource(device->source, device->NumAuxSends);
    al_free(device->source);

    al_free(device->Limiter);
    device->Limiter = NULL;

    al_free(device->ChannelDelay[0].Buffer);
    for(i = 0;i < MAX_OUTPUT_CHANNELS;i++)
    {
        device->ChannelDelay[i].Gain   = 1.0f;
        device->ChannelDelay[i].Length = 0;
        device->ChannelDelay[i].Buffer = NULL;
    }

    AL_STRING_DEINIT(device->DeviceName);

    al_free(device->Dry.Buffer);
    device->Dry.Buffer = NULL;
    device->Dry.NumChannels = 0;
    device->FOAOut.Buffer = NULL;
    device->FOAOut.NumChannels = 0;
    device->RealOut.Buffer = NULL;
    device->RealOut.NumChannels = 0;

    al_free(device);
}


void ALCdevice_IncRef(ALCdevice *device)
{
    unsigned int ref;
    ref = ++device->ref;
    TRACEREF("%p increasing refcount to %u\n", device, ref);
}

void ALCdevice_DecRef(ALCdevice *device)
{
    unsigned int ref;
    ref = --device->ref;
    TRACEREF("%p decreasing refcount to %u\n", device, ref);
    if(ref == 0) FreeDevice(device);
}

/* VerifyDevice
 *
 * Checks if the device handle is valid, and increments its ref count if so.
 */
static ALCboolean VerifyDevice(ALCdevice **device)
{
    ALCdevice *tmpDevice;

    tmpDevice = DeviceList;

    if(tmpDevice)
    {
        if(tmpDevice == *device)
        {
            ALCdevice_IncRef(tmpDevice);
            return ALC_TRUE;
        }
    }

    *device = NULL;
    return ALC_FALSE;
}


/* InitContext
 *
 * Initializes context fields
 */
static ALvoid InitContext(ALCcontext *Context)
{
    struct ALeffectslotArray *auxslots;

    //Validate Context
    Context->UpdateCount = 0;
    Context->HoldUpdates = AL_FALSE;
    Context->GainBoost = 1.0f;
    Context->LastError = AL_NO_ERROR;
    InitUIntMap(&Context->SourceMap, Context->Device->SourcesMax);
    InitUIntMap(&Context->EffectSlotMap, Context->Device->AuxiliaryEffectSlotMax);

    auxslots = al_calloc(DEF_ALIGN, FAM_SIZE(struct ALeffectslotArray, slot, 1));
    auxslots->count = 1;
    auxslots->slot[0] = Context->Device->effect_slot;

    Context->ActiveAuxSlots = auxslots;

    Context->ExtensionList = alExtList;
}


/* FreeContext
 *
 * Cleans up the context, and destroys any remaining objects the app failed to
 * delete. Called once there's no more references on the context.
 */
static void FreeContext(ALCcontext *context)
{
    struct ALeffectslotArray *auxslots;
    size_t count;
    ALsizei i;

    TRACE("%p\n", context);

    if(context->DefaultSlot)
    {
        DeinitEffectSlot(context->DefaultSlot);
        context->DefaultSlot = NULL;
    }

    auxslots = context->ActiveAuxSlots;
    context->ActiveAuxSlots = NULL;
    al_free(auxslots);

    if(context->SourceMap.size > 0)
    {
        WARN("(%p) Deleting %d Source%s\n", context, context->SourceMap.size,
             (context->SourceMap.size==1)?"":"s");
        ReleaseALSources(context);
    }
    ResetUIntMap(&context->SourceMap);

    if(context->EffectSlotMap.size > 0)
    {
        WARN("(%p) Deleting %d AuxiliaryEffectSlot%s\n", context, context->EffectSlotMap.size,
             (context->EffectSlotMap.size==1)?"":"s");
        ReleaseALAuxiliaryEffectSlots(context);
    }
    ResetUIntMap(&context->EffectSlotMap);

    for(i = 0;i < context->VoiceCount;i++)
        DeinitVoice(context->Voices[i]);
    al_free(context->Voices);
    context->Voices = NULL;
    context->VoiceCount = 0;
    context->MaxVoices = 0;

    count = 0;

    ALCdevice_DecRef(context->Device);
    context->Device = NULL;

    //Invalidate context
    memset(context, 0, sizeof(ALCcontext));
    al_free(context);
}

/* ReleaseContext
 *
 * Removes the context reference from the given device and removes it from
 * being current on the running thread or globally. Returns true if other
 * contexts still exist on the device.
 */
static bool ReleaseContext(ALCcontext *context, ALCdevice *device)
{
    ALCcontext *origctx, *newhead;
    bool ret = true;

    origctx = context;
    if(GlobalContext == origctx ? (GlobalContext = NULL, true) : (origctx = GlobalContext, false))
        ALCcontext_DecRef(context);

    origctx = context;
    newhead = NULL;
    if(device->ContextList == origctx ? (device->ContextList = newhead, true) : (origctx = device->ContextList, false))
    {
        ret = !!newhead;
    }

    ALCcontext_DecRef(context);
    return ret;
}

void ALCcontext_IncRef(ALCcontext *context)
{
    unsigned int ref = ++context->ref;
    TRACEREF("%p increasing refcount to %u\n", context, ref);
}

void ALCcontext_DecRef(ALCcontext *context)
{
    unsigned int ref = --context->ref;
    TRACEREF("%p decreasing refcount to %u\n", context, ref);
    if(ref == 0) FreeContext(context);
}

static void ReleaseThreadCtx(void *ptr)
{
    ALCcontext *context = ptr;
    unsigned int ref = --context->ref;
    TRACEREF("%p decreasing refcount to %u\n", context, ref);
    ERR("Context %p current for thread being destroyed, possible leak!\n", context);
}

/* VerifyContext
 *
 * Checks that the given context is valid, and increments its reference count.
 */
static ALCboolean VerifyContext(ALCcontext **context)
{
    ALCdevice *dev;

    dev = DeviceList;
    if(dev)
    {
        ALCcontext *ctx = dev->ContextList;
        if(ctx)
        {
            if(ctx == *context)
            {
                ALCcontext_IncRef(ctx);
                return ALC_TRUE;
            }
        }
    }

    *context = NULL;
    return ALC_FALSE;
}


/* GetContextRef
 *
 * Returns the currently active context for this thread, and adds a reference
 * without locking it.
 */
ALCcontext *GetContextRef(void)
{
    ALCcontext *context;

    context = LocalContext;
    if(context)
        ALCcontext_IncRef(context);
    else
    {
        context = GlobalContext;
        if(context)
            ALCcontext_IncRef(context);
    }

    return context;
}


void AllocateVoices(ALCcontext *context, ALsizei num_voices, ALsizei old_sends)
{
    ALCdevice *device = context->Device;
    ALsizei num_sends = device->NumAuxSends;
    struct ALvoiceProps *props;
    size_t sizeof_props;
    size_t sizeof_voice;
    ALvoice **voices;
    ALvoice *voice;
    ALsizei v = 0;
    size_t size;

    if(num_voices == context->MaxVoices && num_sends == old_sends)
        return;

    /* Allocate the voice pointers, voices, and the voices' stored source
     * property set (including the dynamically-sized Send[] array) in one
     * chunk.
     */
    sizeof_voice = RoundUp(FAM_SIZE(ALvoice, Send, num_sends), 16);
    sizeof_props = RoundUp(FAM_SIZE(struct ALvoiceProps, Send, num_sends), 16);
    size = sizeof(ALvoice*) + sizeof_voice + sizeof_props;

    voices = al_calloc(16, RoundUp(size*num_voices, 16));
    /* The voice and property objects are stored interleaved since they're
     * paired together.
     */
    voice = (ALvoice*)((char*)voices + RoundUp(num_voices*sizeof(ALvoice*), 16));
    props = (struct ALvoiceProps*)((char*)voice + sizeof_voice);

    if(context->Voices)
    {
        const ALsizei v_count = mini(context->VoiceCount, num_voices);
        const ALsizei s_count = mini(old_sends, num_sends);

        for(;v < v_count;v++)
        {
            ALvoice *old_voice = context->Voices[v];
            ALsizei i;

            /* Copy the old voice data and source property set to the new
             * storage.
             */
            *voice = *old_voice;
            for(i = 0;i < s_count;i++)
                voice->Send[i] = old_voice->Send[i];
            *props = *(old_voice->Props);
            for(i = 0;i < s_count;i++)
                props->Send[i] = old_voice->Props->Send[i];

            /* Set this voice's property set pointer and voice reference. */
            voice->Props = props;
            voices[v] = voice;

            /* Increment pointers to the next storage space. */
            voice = (ALvoice*)((char*)props + sizeof_props);
            props = (struct ALvoiceProps*)((char*)voice + sizeof_voice);
        }
        /* Deinit any left over voices that weren't copied over to the new
         * array. NOTE: If this does anything, v equals num_voices and
         * num_voices is less than VoiceCount, so the following loop won't do
         * anything.
         */
        for(;v < context->VoiceCount;v++)
            DeinitVoice(context->Voices[v]);
    }
    /* Finish setting the voices' property set pointers and references. */
    for(;v < num_voices;v++)
    {
        voice->Update = NULL;
        voice->FreeList = NULL;

        voice->Props = props;
        voices[v] = voice;

        voice = (ALvoice*)((char*)props + sizeof_props);
        props = (struct ALvoiceProps*)((char*)voice + sizeof_voice);
    }

    al_free(context->Voices);
    context->Voices = voices;
    context->MaxVoices = num_voices;
    context->VoiceCount = mini(context->VoiceCount, num_voices);
}


/************************************************
 * Standard ALC functions
 ************************************************/

/* alcGetError
 *
 * Return last ALC generated error code for the given device
*/
ALC_API ALCenum ALC_APIENTRY alcGetError(ALCdevice *device)
{
    ALCenum errorCode;

    if(VerifyDevice(&device))
    {
        errorCode = device->LastError;
        device->LastError = ALC_NO_ERROR;
        ALCdevice_DecRef(device);
    }
    else
    {
        errorCode = LastNullDeviceError;
        LastNullDeviceError = ALC_NO_ERROR;
    }

    return errorCode;
}


/* alcSuspendContext
 *
 * Suspends updates for the given context
 */
ALC_API ALCvoid ALC_APIENTRY alcSuspendContext(ALCcontext *context)
{
    if(!SuspendDefers)
        return;

    if(!VerifyContext(&context))
        alcSetError(NULL, ALC_INVALID_CONTEXT);
    else
    {
        ALCcontext_DeferUpdates(context);
        ALCcontext_DecRef(context);
    }
}

/* alcProcessContext
 *
 * Resumes processing updates for the given context
 */
ALC_API ALCvoid ALC_APIENTRY alcProcessContext(ALCcontext *context)
{
    if(!SuspendDefers)
        return;

    if(!VerifyContext(&context))
        alcSetError(NULL, ALC_INVALID_CONTEXT);
    else
    {
        ALCcontext_ProcessUpdates(context);
        ALCcontext_DecRef(context);
    }
}


/* alcGetString
 *
 * Returns information about the device, and error strings
 */
ALC_API const ALCchar* ALC_APIENTRY alcGetString(ALCdevice *Device, ALCenum param)
{
    const ALCchar *value = NULL;

    switch(param)
    {
    case ALC_NO_ERROR:
        value = alcNoError;
        break;

    case ALC_INVALID_ENUM:
        value = alcErrInvalidEnum;
        break;

    case ALC_INVALID_VALUE:
        value = alcErrInvalidValue;
        break;

    case ALC_INVALID_DEVICE:
        value = alcErrInvalidDevice;
        break;

    case ALC_INVALID_CONTEXT:
        value = alcErrInvalidContext;
        break;

    case ALC_OUT_OF_MEMORY:
        value = alcErrOutOfMemory;
        break;

    case ALC_DEVICE_SPECIFIER:
        value = alcDefaultName;
        break;

    case ALC_EXTENSIONS:
        if(!VerifyDevice(&Device))
            value = alcNoDeviceExtList;
        else
        {
            value = alcExtensionList;
            ALCdevice_DecRef(Device);
        }
        break;

    default:
        VerifyDevice(&Device);
        alcSetError(Device, ALC_INVALID_ENUM);
        if(Device) ALCdevice_DecRef(Device);
        break;
    }

    return value;
}


static inline ALCsizei NumAttrsForDevice(ALCdevice *device)
{
    if(device->Type == Loopback && device->FmtChans == DevFmtAmbi3D)
        return 25;
    return 19;
}

static ALCsizei GetIntegerv(ALCdevice *device, ALCenum param, ALCsizei size, ALCint *values)
{
    ALCsizei i;

    if(size <= 0 || values == NULL)
    {
        alcSetError(device, ALC_INVALID_VALUE);
        return 0;
    }

    if(!device)
    {
        switch(param)
        {
            case ALC_MAJOR_VERSION:
                values[0] = alcMajorVersion;
                return 1;
            case ALC_MINOR_VERSION:
                values[0] = alcMinorVersion;
                return 1;

            case ALC_ATTRIBUTES_SIZE:
            case ALC_ALL_ATTRIBUTES:
            case ALC_FREQUENCY:
            case ALC_REFRESH:
            case ALC_SYNC:
            case ALC_MONO_SOURCES:
            case ALC_STEREO_SOURCES:
            case ALC_CAPTURE_SAMPLES:
            case ALC_FORMAT_CHANNELS_SOFT:
            case ALC_FORMAT_TYPE_SOFT:
            case ALC_AMBISONIC_LAYOUT_SOFT:
            case ALC_AMBISONIC_SCALING_SOFT:
            case ALC_AMBISONIC_ORDER_SOFT:
                alcSetError(NULL, ALC_INVALID_DEVICE);
                return 0;

            default:
                alcSetError(NULL, ALC_INVALID_ENUM);
                return 0;
        }
        return 0;
    }

    /* render device */
    switch(param)
    {
        case ALC_MAJOR_VERSION:
            values[0] = alcMajorVersion;
            return 1;

        case ALC_MINOR_VERSION:
            values[0] = alcMinorVersion;
            return 1;

        case ALC_EFX_MAJOR_VERSION:
            values[0] = alcEFXMajorVersion;
            return 1;

        case ALC_EFX_MINOR_VERSION:
            values[0] = alcEFXMinorVersion;
            return 1;

        case ALC_ATTRIBUTES_SIZE:
            values[0] = NumAttrsForDevice(device);
            return 1;

        case ALC_ALL_ATTRIBUTES:
            if(size < NumAttrsForDevice(device))
            {
                alcSetError(device, ALC_INVALID_VALUE);
                return 0;
            }

            i = 0;
            values[i++] = ALC_FREQUENCY;
            values[i++] = device->Frequency;

            if(device->Type != Loopback)
            {
                values[i++] = ALC_REFRESH;
                values[i++] = device->Frequency / device->UpdateSize;

                values[i++] = ALC_SYNC;
                values[i++] = ALC_FALSE;
            }
            else
            {
                values[i++] = ALC_FORMAT_CHANNELS_SOFT;
                values[i++] = device->FmtChans;

                values[i++] = ALC_FORMAT_TYPE_SOFT;
                values[i++] = device->FmtType;
            }

            values[i++] = ALC_MONO_SOURCES;
            values[i++] = device->NumMonoSources;

            values[i++] = ALC_STEREO_SOURCES;
            values[i++] = device->NumStereoSources;

            values[i++] = ALC_MAX_AUXILIARY_SENDS;
            values[i++] = device->NumAuxSends;

            values[i++] = ALC_OUTPUT_LIMITER_SOFT;
            values[i++] = device->Limiter ? ALC_TRUE : ALC_FALSE;

            values[i++] = 0;
            return i;

        case ALC_FREQUENCY:
            values[0] = device->Frequency;
            return 1;

        case ALC_REFRESH:
            if(device->Type == Loopback)
            {
                alcSetError(device, ALC_INVALID_DEVICE);
                return 0;
            }
            values[0] = device->Frequency / device->UpdateSize;
            return 1;

        case ALC_SYNC:
            if(device->Type == Loopback)
            {
                alcSetError(device, ALC_INVALID_DEVICE);
                return 0;
            }
            values[0] = ALC_FALSE;
            return 1;

        case ALC_FORMAT_CHANNELS_SOFT:
            if(device->Type != Loopback)
            {
                alcSetError(device, ALC_INVALID_DEVICE);
                return 0;
            }
            values[0] = device->FmtChans;
            return 1;

        case ALC_FORMAT_TYPE_SOFT:
            if(device->Type != Loopback)
            {
                alcSetError(device, ALC_INVALID_DEVICE);
                return 0;
            }
            values[0] = device->FmtType;
            return 1;

        case ALC_MONO_SOURCES:
            values[0] = device->NumMonoSources;
            return 1;

        case ALC_STEREO_SOURCES:
            values[0] = device->NumStereoSources;
            return 1;

        case ALC_MAX_AUXILIARY_SENDS:
            values[0] = device->NumAuxSends;
            return 1;

        case ALC_CONNECTED:
            values[0] = device->Connected;
            return 1;

        case ALC_OUTPUT_LIMITER_SOFT:
            values[0] = device->Limiter ? ALC_TRUE : ALC_FALSE;
            return 1;

        default:
            alcSetError(device, ALC_INVALID_ENUM);
            return 0;
    }
    return 0;
}

/* alcGetIntegerv
 *
 * Returns information about the device and the version of OpenAL
 */
ALC_API void ALC_APIENTRY alcGetIntegerv(ALCdevice *device, ALCenum param, ALCsizei size, ALCint *values)
{
    VerifyDevice(&device);
    if(size <= 0 || values == NULL)
        alcSetError(device, ALC_INVALID_VALUE);
    else
        GetIntegerv(device, param, size, values);
    if(device) ALCdevice_DecRef(device);
}

ALC_API void ALC_APIENTRY alcGetInteger64vSOFT(ALCdevice *device, ALCenum pname, ALCsizei size, ALCint64SOFT *values)
{
    ALCint *ivals;
    ALsizei i;

    VerifyDevice(&device);
    if(size <= 0 || values == NULL)
        alcSetError(device, ALC_INVALID_VALUE);
    else if(!device)
    {
        ivals = malloc(size * sizeof(ALCint));
        size = GetIntegerv(device, pname, size, ivals);
        for(i = 0;i < size;i++)
            values[i] = ivals[i];
        free(ivals);
    }
    else /* render device */
    {
        ALuint64 basecount;
        ALuint samplecount;

        switch(pname)
        {
            case ALC_ATTRIBUTES_SIZE:
                *values = NumAttrsForDevice(device)+4;
                break;

            case ALC_ALL_ATTRIBUTES:
                if(size < NumAttrsForDevice(device)+4)
                    alcSetError(device, ALC_INVALID_VALUE);
                else
                {
                    i = 0;
                    values[i++] = ALC_FREQUENCY;
                    values[i++] = device->Frequency;

                    if(device->Type != Loopback)
                    {
                        values[i++] = ALC_REFRESH;
                        values[i++] = device->Frequency / device->UpdateSize;

                        values[i++] = ALC_SYNC;
                        values[i++] = ALC_FALSE;
                    }
                    else
                    {
                        values[i++] = ALC_FORMAT_CHANNELS_SOFT;
                        values[i++] = device->FmtChans;

                        values[i++] = ALC_FORMAT_TYPE_SOFT;
                        values[i++] = device->FmtType;
                    }

                    values[i++] = ALC_MONO_SOURCES;
                    values[i++] = device->NumMonoSources;

                    values[i++] = ALC_STEREO_SOURCES;
                    values[i++] = device->NumStereoSources;

                    values[i++] = ALC_MAX_AUXILIARY_SENDS;
                    values[i++] = device->NumAuxSends;

                    values[i++] = ALC_OUTPUT_LIMITER_SOFT;
                    values[i++] = device->Limiter ? ALC_TRUE : ALC_FALSE;

                    values[i++] = 0;
                }
                break;

            case ALC_DEVICE_CLOCK_SOFT:
                basecount = device->ClockBase;
                samplecount = device->SamplesDone;
                *values = basecount + (samplecount*DEVICE_CLOCK_RES/device->Frequency);
                break;

            default:
                ivals = malloc(size * sizeof(ALCint));
                size = GetIntegerv(device, pname, size, ivals);
                for(i = 0;i < size;i++)
                    values[i] = ivals[i];
                free(ivals);
                break;
        }
    }
    if(device)
        ALCdevice_DecRef(device);
}


/* alcIsExtensionPresent
 *
 * Determines if there is support for a particular extension
 */
ALC_API ALCboolean ALC_APIENTRY alcIsExtensionPresent(ALCdevice *device, const ALCchar *extName)
{
    ALCboolean bResult = ALC_FALSE;

    VerifyDevice(&device);

    if(!extName)
        alcSetError(device, ALC_INVALID_VALUE);
    else
    {
        size_t len = strlen(extName);
        const char *ptr = (device ? alcExtensionList : alcNoDeviceExtList);
        while(ptr && *ptr)
        {
            if(strncasecmp(ptr, extName, len) == 0 &&
               (ptr[len] == '\0' || isspace(ptr[len])))
            {
                bResult = ALC_TRUE;
                break;
            }
            if((ptr=strchr(ptr, ' ')) != NULL)
            {
                do {
                    ++ptr;
                } while(isspace(*ptr));
            }
        }
    }
    if(device)
        ALCdevice_DecRef(device);
    return bResult;
}


/* alcGetProcAddress
 *
 * Retrieves the function address for a particular extension function
 */
ALC_API ALCvoid* ALC_APIENTRY alcGetProcAddress(ALCdevice *device, const ALCchar *funcName)
{
    ALCvoid *ptr = NULL;

    if(!funcName)
    {
        VerifyDevice(&device);
        alcSetError(device, ALC_INVALID_VALUE);
        if(device) ALCdevice_DecRef(device);
    }
    else
    {
        size_t i = 0;
        for(i = 0;i < COUNTOF(alcFunctions);i++)
        {
            if(strcmp(alcFunctions[i].funcName, funcName) == 0)
            {
                ptr = alcFunctions[i].address;
                break;
            }
        }
    }

    return ptr;
}


/* alcGetEnumValue
 *
 * Get the value for a particular ALC enumeration name
 */
ALC_API ALCenum ALC_APIENTRY alcGetEnumValue(ALCdevice *device, const ALCchar *enumName)
{
    ALCenum val = 0;

    if(!enumName)
    {
        VerifyDevice(&device);
        alcSetError(device, ALC_INVALID_VALUE);
        if(device) ALCdevice_DecRef(device);
    }
    else
    {
        size_t i = 0;
        for(i = 0;i < COUNTOF(alcEnumerations);i++)
        {
            if(strcmp(alcEnumerations[i].enumName, enumName) == 0)
            {
                val = alcEnumerations[i].value;
                break;
            }
        }
    }

    return val;
}


/* alcCreateContext
 *
 * Create and attach a context to the given device.
 */
ALC_API ALCcontext* ALC_APIENTRY alcCreateContext(ALCdevice *device, const ALCint *attrList)
{
    ALCcontext *ALContext;
    ALCenum err;

    /* Explicitly hold the list lock while taking the BackendLock in case the
     * device is asynchronously destropyed, to ensure this new context is
     * properly cleaned up after being made.
     */
    if(!VerifyDevice(&device) || !device->Connected)
    {
        alcSetError(device, ALC_INVALID_DEVICE);
        if(device) ALCdevice_DecRef(device);
        return NULL;
    }

    if (device->ContextList)
    {
        alcSetError(device, ALC_INVALID_DEVICE);
        return NULL;
    }

    device->LastError = ALC_NO_ERROR;

    if(device->Type == Playback && DefaultEffect.type != AL_EFFECT_NULL)
        ALContext = al_calloc(16, sizeof(ALCcontext)+sizeof(ALeffectslot));
    else
        ALContext = al_calloc(16, sizeof(ALCcontext));
    if(!ALContext)
    {
        alcSetError(device, ALC_OUT_OF_MEMORY);
        ALCdevice_DecRef(device);
        return NULL;
    }

    ALContext->ref = 1;
    ALContext->DefaultSlot = NULL;

    ALContext->Voices = NULL;
    ALContext->VoiceCount = 0;
    ALContext->MaxVoices = 0;
    ALContext->ActiveAuxSlots = NULL;
    ALContext->Device = device;

    if((err=UpdateDeviceParams(device, attrList)) != ALC_NO_ERROR)
    {
        al_free(ALContext);
        ALContext = NULL;

        alcSetError(device, err);
        if(err == ALC_INVALID_DEVICE)
        {
            aluHandleDisconnect(device);
        }
        ALCdevice_DecRef(device);
        return NULL;
    }
    AllocateVoices(ALContext, 1, device->NumAuxSends);

    if(DefaultEffect.type != AL_EFFECT_NULL && device->Type == Playback)
    {
        ALContext->DefaultSlot = (ALeffectslot*)(ALContext->_listener_mem);
        if(InitEffectSlot(ALContext->DefaultSlot) == AL_NO_ERROR)
            aluInitEffectPanning(ALContext->DefaultSlot);
        else
        {
            ALContext->DefaultSlot = NULL;
            ERR("Failed to initialize the default effect slot\n");
        }
    }

    ALCdevice_IncRef(ALContext->Device);
    InitContext(ALContext);

    device->ContextList = ALContext;

    if(ALContext->DefaultSlot)
    {
        if(InitializeEffect(device, ALContext->DefaultSlot, &DefaultEffect) == AL_NO_ERROR)
            UpdateEffectSlotProps(ALContext->DefaultSlot);
        else
            ERR("Failed to initialize the default effect\n");
    }

    ALCdevice_DecRef(device);

    TRACE("Created context %p\n", ALContext);
    return ALContext;
}

/* alcDestroyContext
 *
 * Remove a context from its device
 */
ALC_API ALCvoid ALC_APIENTRY alcDestroyContext(ALCcontext *context)
{
    ALCdevice *Device;

    if(!VerifyContext(&context))
    {
        alcSetError(NULL, ALC_INVALID_CONTEXT);
        return;
    }

    Device = context->Device;
    if(Device)
    {
        if(!ReleaseContext(context, Device))
        {
            Device->Flags &= ~DEVICE_RUNNING;
        }
    }

    ALCcontext_DecRef(context);
}


/* alcGetCurrentContext
 *
 * Returns the currently active context on the calling thread
 */
ALC_API ALCcontext* ALC_APIENTRY alcGetCurrentContext(void)
{
    ALCcontext *Context = LocalContext;
    if(!Context) Context = GlobalContext;
    return Context;
}

/* alcGetThreadContext
 *
 * Returns the currently active thread-local context
 */
ALC_API ALCcontext* ALC_APIENTRY alcGetThreadContext(void)
{
    return LocalContext;
}


/* alcMakeContextCurrent
 *
 * Makes the given context the active process-wide context, and removes the
 * thread-local context for the calling thread.
 */
ALC_API ALCboolean ALC_APIENTRY alcMakeContextCurrent(ALCcontext *context)
{
    ALCcontext* temp_context;

    /* context must be valid or NULL */
    if(context && !VerifyContext(&context))
    {
        alcSetError(NULL, ALC_INVALID_CONTEXT);
        return ALC_FALSE;
    }
    /* context's reference count is already incremented */
    temp_context = context;
    context = GlobalContext;
    GlobalContext = temp_context;
    if(context) ALCcontext_DecRef(context);

    if((context=LocalContext) != NULL)
    {
        LocalContext = NULL;
        ALCcontext_DecRef(context);
    }

    return ALC_TRUE;
}

/* alcSetThreadContext
 *
 * Makes the given context the active context for the current thread
 */
ALC_API ALCboolean ALC_APIENTRY alcSetThreadContext(ALCcontext *context)
{
    ALCcontext *old;

    /* context must be valid or NULL */
    if(context && !VerifyContext(&context))
    {
        alcSetError(NULL, ALC_INVALID_CONTEXT);
        return ALC_FALSE;
    }
    /* context's reference count is already incremented */
    old = LocalContext;
    LocalContext = context;
    if(old) ALCcontext_DecRef(old);

    return ALC_TRUE;
}


/* alcGetContextsDevice
 *
 * Returns the device that a particular context is attached to
 */
ALC_API ALCdevice* ALC_APIENTRY alcGetContextsDevice(ALCcontext *Context)
{
    ALCdevice *Device;

    if(!VerifyContext(&Context))
    {
        alcSetError(NULL, ALC_INVALID_CONTEXT);
        return NULL;
    }
    Device = Context->Device;
    ALCcontext_DecRef(Context);

    return Device;
}


/* alcOpenDevice
 *
 * Opens the named device.
 */
ALC_API ALCdevice* ALC_APIENTRY alcOpenDevice(const ALCchar *deviceName)
{
    ALCdevice *device;
    ALCsizei i;

    DO_INITCONFIG();

    if (DeviceList)
    {
        alcSetError(NULL, ALC_OUT_OF_MEMORY);
        return NULL;
    }

    if(deviceName && (!deviceName[0] || strcasecmp(deviceName, alcDefaultName) == 0 || strcasecmp(deviceName, "openal-soft") == 0
#ifdef _WIN32
        /* Some old Windows apps hardcode these expecting OpenAL to use a
         * specific audio API, even when they're not enumerated. Creative's
         * router effectively ignores them too.
         */
        || strcasecmp(deviceName, "DirectSound3D") == 0 || strcasecmp(deviceName, "DirectSound") == 0
        || strcasecmp(deviceName, "MMSYSTEM") == 0
#endif
    ))
        deviceName = NULL;

    device = al_calloc(16, sizeof(ALCdevice));
    if(!device)
    {
        alcSetError(NULL, ALC_OUT_OF_MEMORY);
        return NULL;
    }

    //Validate device
    device->ref = 1;
    device->Connected = ALC_TRUE;
    device->Type = Playback;
    device->LastError = ALC_NO_ERROR;

    device->Flags = 0;
    device->Render_Mode = NormalRender;
    AL_STRING_INIT(device->DeviceName);
    device->Dry.Buffer = NULL;
    device->Dry.NumChannels = 0;
    device->FOAOut.Buffer = NULL;
    device->FOAOut.NumChannels = 0;
    device->RealOut.Buffer = NULL;
    device->RealOut.NumChannels = 0;
    device->Limiter = NULL;

    device->ContextList = NULL;

    device->ClockBase = 0;
    device->SamplesDone = 0;

    device->SourcesMax = 256;
    device->AuxiliaryEffectSlotMax = 64;
    device->NumAuxSends = DEFAULT_SENDS;

    InitUIntMap(&device->FilterMap, INT_MAX);

    for(i = 0;i < MAX_OUTPUT_CHANNELS;i++)
    {
        device->ChannelDelay[i].Gain   = 1.0f;
        device->ChannelDelay[i].Length = 0;
        device->ChannelDelay[i].Buffer = NULL;
    }

    //Set output format
    device->FmtChans = DevFmtChannelsDefault;
    device->FmtType = DevFmtTypeDefault;
    device->Frequency = DEFAULT_OUTPUT_RATE;
    device->IsHeadphones = AL_FALSE;
    device->NumUpdates = 3;
    device->UpdateSize = 1024;

    device->NumUpdates = clampu(device->NumUpdates, 2, 16);
    device->UpdateSize = clampu(device->UpdateSize, 64, 8192);

    if((CPUCapFlags&(CPU_CAP_SSE|CPU_CAP_NEON)) != 0)
        device->UpdateSize = (device->UpdateSize+3)&~3;

    if(device->SourcesMax == 0) device->SourcesMax = 256;

    if(device->AuxiliaryEffectSlotMax == 0) device->AuxiliaryEffectSlotMax = 64;

    device->NumStereoSources = 1;
    device->NumMonoSources = device->SourcesMax - device->NumStereoSources;
    device->Limiter = CreateDeviceLimiter(device);

    device->source = al_calloc(16, sizeof(ALsource));
    InitSourceParams(device->source, device->NumAuxSends);

    device->effect_slot = al_calloc(16, sizeof(ALeffectslot));
    InitEffectSlot(device->effect_slot);
    aluInitEffectPanning(device->effect_slot);

    device->effect = al_calloc(16, sizeof(ALeffect));
    InitEffect(device->effect);

    DeviceList = device;

    TRACE("Created device %p, \"%s\"\n", device, alstr_get_cstr(device->DeviceName));
    return device;
}

/* alcCloseDevice
 *
 * Closes the given device.
 */
ALC_API ALCboolean ALC_APIENTRY alcCloseDevice(ALCdevice *device)
{
    ALCdevice *iter;
    ALCcontext *ctx;

    iter = DeviceList;
    if(!iter)
    {
        alcSetError(iter, ALC_INVALID_DEVICE);
        return ALC_FALSE;
    }

    ctx = device->ContextList;
    if(ctx != NULL)
    {
        WARN("Releasing context %p\n", ctx);
        ReleaseContext(ctx, device);
    }
    device->Flags &= ~DEVICE_RUNNING;

    ALCdevice_DecRef(device);

    return ALC_TRUE;
}


/************************************************
 * ALC capture functions
 ************************************************/
ALC_API ALCdevice* ALC_APIENTRY alcCaptureOpenDevice(const ALCchar *deviceName, ALCuint frequency, ALCenum format, ALCsizei samples)
{
    return NULL;
}

ALC_API ALCboolean ALC_APIENTRY alcCaptureCloseDevice(ALCdevice *device)
{
    return ALC_FALSE;
}

ALC_API void ALC_APIENTRY alcCaptureStart(ALCdevice *device)
{
    alcSetError(device, ALC_INVALID_DEVICE);
}

ALC_API void ALC_APIENTRY alcCaptureStop(ALCdevice *device)
{
    alcSetError(device, ALC_INVALID_DEVICE);
}

ALC_API void ALC_APIENTRY alcCaptureSamples(ALCdevice *device, ALCvoid *buffer, ALCsizei samples)
{
    alcSetError(device, ALC_INVALID_DEVICE);
}


/************************************************
 * ALC loopback functions
 ************************************************/

/* alcLoopbackOpenDeviceSOFT
 *
 * Open a loopback device, for manual rendering.
 */
ALC_API ALCdevice* ALC_APIENTRY alcLoopbackOpenDeviceSOFT(const ALCchar *deviceName)
{
    return NULL;
}

/* alcIsRenderFormatSupportedSOFT
 *
 * Determines if the loopback device supports the given format for rendering.
 */
ALC_API ALCboolean ALC_APIENTRY alcIsRenderFormatSupportedSOFT(ALCdevice *device, ALCsizei freq, ALCenum channels, ALCenum type)
{
    ALCboolean ret = ALC_FALSE;

    if(!VerifyDevice(&device) || device->Type != Loopback)
        alcSetError(device, ALC_INVALID_DEVICE);
    else if(freq <= 0)
        alcSetError(device, ALC_INVALID_VALUE);
    else
    {
        if(IsValidALCType(type) && IsValidALCChannels(channels) && freq >= MIN_OUTPUT_RATE)
            ret = ALC_TRUE;
    }
    if(device) ALCdevice_DecRef(device);

    return ret;
}

/* alcRenderSamplesSOFT
 *
 * Renders some samples into a buffer, using the format last set by the
 * attributes given to alcCreateContext.
 */
FORCE_ALIGN ALC_API void ALC_APIENTRY alcRenderSamplesSOFT(ALCdevice *device, ALCvoid *buffer, ALCsizei samples)
{
    if(!VerifyDevice(&device) || device->Type != Loopback)
        alcSetError(device, ALC_INVALID_DEVICE);
    else if(samples < 0 || (samples > 0 && buffer == NULL))
        alcSetError(device, ALC_INVALID_VALUE);
    else
    {
        aluMixData(device, buffer, samples, NULL);
    }
    if(device) ALCdevice_DecRef(device);
}


/************************************************
 * ALC loopback2 functions
 ************************************************/

ALC_API ALCboolean ALC_APIENTRY alcIsAmbisonicFormatSupportedSOFT(ALCdevice *device, ALCenum layout, ALCenum scaling, ALsizei order)
{
    ALCboolean ret = ALC_FALSE;

    if(!VerifyDevice(&device) || device->Type != Loopback)
        alcSetError(device, ALC_INVALID_DEVICE);
    else if(order <= 0)
        alcSetError(device, ALC_INVALID_VALUE);
    else
    {
        if(IsValidAmbiLayout(layout) && IsValidAmbiScaling(scaling) && order <= MAX_AMBI_ORDER)
            ret = ALC_TRUE;
    }
    if(device) ALCdevice_DecRef(device);

    return ret;
}

/************************************************
 * ALC DSP pause/resume functions
 ************************************************/

/* alcDevicePauseSOFT
 *
 * Pause the DSP to stop audio processing.
 */
ALC_API void ALC_APIENTRY alcDevicePauseSOFT(ALCdevice *device)
{
    if(!VerifyDevice(&device) || device->Type != Playback)
        alcSetError(device, ALC_INVALID_DEVICE);
    else
    {
        device->Flags &= ~DEVICE_RUNNING;
        device->Flags |= DEVICE_PAUSED;
    }
    if(device) ALCdevice_DecRef(device);
}

/* alcDeviceResumeSOFT
 *
 * Resume the DSP to restart audio processing.
 */
ALC_API void ALC_APIENTRY alcDeviceResumeSOFT(ALCdevice *device)
{
    if(!VerifyDevice(&device) || device->Type != Playback)
        alcSetError(device, ALC_INVALID_DEVICE);
    else
    {
        if((device->Flags&DEVICE_PAUSED))
        {
            device->Flags &= ~DEVICE_PAUSED;
            if(device->ContextList != NULL)
            {
                device->Flags |= DEVICE_RUNNING;
            }
        }
    }
    if(device) ALCdevice_DecRef(device);
}


/************************************************
 * ALC HRTF functions
 ************************************************/

/* alcGetStringiSOFT
 *
 * Gets a string parameter at the given index.
 */
ALC_API const ALCchar* ALC_APIENTRY alcGetStringiSOFT(ALCdevice *device, ALCenum paramName, ALCsizei index)
{
    const ALCchar *str = NULL;

    if(!VerifyDevice(&device))
        alcSetError(device, ALC_INVALID_DEVICE);
    else switch(paramName)
    {
        default:
            alcSetError(device, ALC_INVALID_ENUM);
            break;
    }
    if(device) ALCdevice_DecRef(device);

    return str;
}

/* alcResetDeviceSOFT
 *
 * Resets the given device output, using the specified attribute list.
 */
ALC_API ALCboolean ALC_APIENTRY alcResetDeviceSOFT(ALCdevice *device, const ALCint *attribs)
{
    ALCenum err;

    if(!VerifyDevice(&device) || !device->Connected)
    {
        alcSetError(device, ALC_INVALID_DEVICE);
        if(device) ALCdevice_DecRef(device);
        return ALC_FALSE;
    }

    err = UpdateDeviceParams(device, attribs);

    if(err != ALC_NO_ERROR)
    {
        alcSetError(device, err);
        if(err == ALC_INVALID_DEVICE)
        {
            aluHandleDisconnect(device);
        }
        ALCdevice_DecRef(device);
        return ALC_FALSE;
    }
    ALCdevice_DecRef(device);

    return ALC_TRUE;
}
