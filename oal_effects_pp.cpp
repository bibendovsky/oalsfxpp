#define NOMINMAX


#include <malloc.h>
#include <stdio.h>
#include <stdint.h>
#include "config.h"
#include "AL\al.h"
#include "AL\alc.h"
#include "AL\alext.h"
#include "AL\efx.h"
#include "AL\efx-presets.h"
#include "alMain.h"
#include "alEffect.h"
#include "alAuxEffectSlot.h"
#include "alSource.h"


void InitEffectParams(
    ALeffect* effect,
    ALenum type);

void UpdateSourceProps(
    ALsource* source,
    ALvoice* voice,
    ALsizei num_sends);

void aluMixData(
    ALCdevice* device,
    ALvoid* OutBuffer,
    ALsizei NumSamples,
    const ALfloat* src_samples);


int main()
{
    const char* src_file_name = "f:\\temp\\rev\\in.raw";
    const char* dst_file_name = "f:\\temp\\rev\\out.raw";
    const int sample_count = 1024;
    const int channel_count = 1;
    const ALCint context_attribs[] = {
        ALC_MAX_AUXILIARY_SENDS,
        1,

        0,
    };

    FILE* src_stream = NULL;
    FILE* dst_stream = NULL;
    long stream_size = 0;
    int data_size = 0;
    char* src_buffer = NULL;
    float* dst_buffer = NULL;
    const int16_t* src_buffer16 = NULL;
    float* src_buffer_f32 = NULL;
    int total_sample_count;
    int buffer_f32_samples = 0;
    int i = 0;
    int is_succeed = 1;
    ALCdevice* oal_device = NULL;
    ALCcontext* oal_context = NULL;
    ALuint oal_effect_slot = AL_EFFECTSLOT_NULL;
    ALuint oal_effect = AL_EFFECT_NULL;
    ALuint oal_source = 0;


    if (is_succeed)
    {
        src_stream = fopen(src_file_name, "rb");

        if (!src_stream)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to open a source stream.");
        }
    }

    if (is_succeed)
    {
        if (fseek(src_stream, 0, SEEK_END) < 0)
        {
            is_succeed = 0;
        }
    }

    if (is_succeed)
    {
        stream_size = ftell(src_stream);
    }

    if (is_succeed)
    {
        if (fseek(src_stream, 0, SEEK_SET) < 0)
        {
            is_succeed = 0;
        }
    }

    if (is_succeed)
    {
        data_size = (int)stream_size;

        if (data_size < 2 || (data_size % 2) != 0)
        {
            is_succeed = 0;
            printf("%s\n", "Invalid data size.");
        }
    }

    if (is_succeed)
    {
        src_buffer = static_cast<char*>(malloc(data_size));

        if (src_buffer)
        {
            src_buffer16 = (const int16_t*)src_buffer;
        }
        else
        {
            is_succeed = 0;
            printf("%s\n", "Failed to allocate a source buffer.");
        }
    }

    if (is_succeed)
    {
        if (fread(src_buffer, 1, data_size, src_stream) != data_size)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to read data.");
        }
    }

    total_sample_count = data_size / 2;

    buffer_f32_samples = ((total_sample_count + (sample_count - 1)) / sample_count) * sample_count;

    src_buffer_f32 = static_cast<float*>(malloc(sizeof(float) * buffer_f32_samples));

    for (i = 0; i < total_sample_count; ++i)
    {
        src_buffer_f32[i] = (float)(src_buffer16[i]) / 32768.0F;
    }

    if (is_succeed)
    {
        oal_device = alcOpenDevice(NULL);

        if (!oal_device)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to open device.");
        }
    }

    if (is_succeed)
    {
        oal_context = alcCreateContext(oal_device, context_attribs);

        if (!oal_context)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to create a context.");
        }
    }

    if (is_succeed)
    {
        auto result = alcMakeContextCurrent(oal_context);

        if (result == AL_FALSE)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to make a context current.");
        }
    }


    if (is_succeed)
    {
        InitEffectParams(oal_device->effect, AL_EFFECT_EAXREVERB);
        UpdateEffectSlotProps(oal_device->effect_slot);
    }

    if (is_succeed)
    {
#if 1
        EFXEAXREVERBPROPERTIES props = EFX_REVERB_PRESET_BATHROOM;
        ALeffectProps* oal_props = &oal_device->effect->props;

        oal_props->reverb.density = props.flDensity;
        oal_props->reverb.diffusion = props.flDiffusion;
        oal_props->reverb.gain = props.flGain;
        oal_props->reverb.gain_hf = props.flGainHF;
        oal_props->reverb.gain_lf = props.flGainLF;
        oal_props->reverb.decay_time = props.flDecayTime;
        oal_props->reverb.decay_hf_ratio = props.flDecayHFRatio;
        oal_props->reverb.decay_lf_ratio = props.flDecayLFRatio;
        oal_props->reverb.reflections_gain = props.flReflectionsGain;
        oal_props->reverb.reflections_delay = props.flReflectionsDelay;
        oal_props->reverb.reflections_pan[0] = props.flReflectionsPan[0];
        oal_props->reverb.reflections_pan[1] = props.flReflectionsPan[1];
        oal_props->reverb.reflections_pan[2] = props.flReflectionsPan[2];
        oal_props->reverb.late_reverb_gain = props.flLateReverbGain;
        oal_props->reverb.late_reverb_delay = props.flLateReverbDelay;
        oal_props->reverb.late_reverb_pan[0] = props.flLateReverbPan[0];
        oal_props->reverb.late_reverb_pan[1] = props.flLateReverbPan[1];
        oal_props->reverb.late_reverb_pan[2] = props.flLateReverbPan[2];
        oal_props->reverb.echo_time = props.flEchoTime;
        oal_props->reverb.echo_depth = props.flEchoDepth;
        oal_props->reverb.modulation_time = props.flModulationTime;
        oal_props->reverb.modulation_depth = props.flModulationDepth;
        oal_props->reverb.air_absorption_gain_hf = props.flAirAbsorptionGainHF;
        oal_props->reverb.hf_reference = props.flHFReference;
        oal_props->reverb.lf_reference = props.flLFReference;
        oal_props->reverb.room_rolloff_factor = props.flRoomRolloffFactor;
        oal_props->reverb.decay_hf_limit = props.iDecayHFLimit;
#endif
    }

    if (is_succeed)
    {
        InitializeEffect(oal_device, oal_device->effect_slot, oal_device->effect);
        UpdateEffectSlotProps(oal_device->effect_slot);
    }

    if (is_succeed)
    {
        oal_device->source->send->slot = oal_device->effect_slot;
        UpdateSourceProps(oal_device->source, oal_context->voice, 1);
    }

    if (is_succeed)
    {
        alSourcePlay(oal_source);
    }

    if (is_succeed)
    {
        dst_stream = fopen(dst_file_name, "wb");

        if (!dst_stream)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to open a destination file.");
        }
    }

    if (is_succeed)
    {
        dst_buffer = static_cast<float*>(malloc(sizeof(float) * sample_count * channel_count));

        if (!dst_buffer)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to allocate a destination buffer.");
        }
    }

    if (is_succeed)
    {
        int remain = (int)(stream_size) / 2;
        int offset = 0;

        while (remain > 0 && is_succeed)
        {
            const int write_sample_count = sample_count < remain ? sample_count : remain;
            const int write_size = write_sample_count * 4 * channel_count;

            aluMixData(oal_device, dst_buffer, sample_count, &src_buffer_f32[offset]);

            if (fwrite(dst_buffer, 1, write_size, dst_stream) != write_size)
            {
                is_succeed = 0;
                printf("%s\n", "Failed to write out data.");
            }

            remain -= write_sample_count;
            offset += write_sample_count * channel_count;
        }
    }

    alSourceStop(oal_source);

    alcMakeContextCurrent(NULL);
    alcDestroyContext(oal_context);
    alcCloseDevice(oal_device);

    free(dst_buffer);
    free(src_buffer);

    fclose(src_stream);
    fclose(dst_stream);

    return (is_succeed ? 0 : 2);
}
