#define NOMINMAX


#include <malloc.h>
#include <stdio.h>
#include <stdint.h>
#include "AL\al.h"
#include "AL\alc.h"
#include "AL\alext.h"
#include "AL\efx.h"
#include "AL\efx-presets.h"


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
    ALCint sends_count = 0;
    LPALGENEFFECTS alGenEffects = NULL;
    LPALDELETEEFFECTS alDeleteEffects = NULL;
    LPALISEFFECT alIsEffect = NULL;
    LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots = NULL;
    LPALEFFECTI alEffecti = NULL;
    LPALEFFECTF alEffectf = NULL;
    LPALEFFECTFV alEffectfv = NULL;
    LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti = NULL;
    LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots = NULL;
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
        src_buffer = malloc(data_size);

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

    src_buffer_f32 = malloc(sizeof(float) * buffer_f32_samples);

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
        if (alcIsExtensionPresent(oal_device, "ALC_EXT_EFX") == AL_FALSE)
        {
            is_succeed = 0;
            printf("%s\n", "No EFX.");
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
        alcGetIntegerv(oal_device, ALC_MAX_AUXILIARY_SENDS, 1, &sends_count);
    }


    if (is_succeed)
    {
        alGenEffects = (LPALGENEFFECTS)(alGetProcAddress("alGenEffects"));

        if (!alGenEffects)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to make a alGenEffects current.");
        }

        alDeleteEffects = (LPALDELETEEFFECTS)(alGetProcAddress("alDeleteEffects"));

        if (!alDeleteEffects)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to make a alDeleteEffects current.");
        }


        alIsEffect = (LPALISEFFECT)(alGetProcAddress("alIsEffect"));

        if (!alIsEffect)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to make a alIsEffect current.");
        }


        alGenAuxiliaryEffectSlots = (LPALGENAUXILIARYEFFECTSLOTS)(alGetProcAddress("alGenAuxiliaryEffectSlots"));

        if (!alGenAuxiliaryEffectSlots)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to make a alGenAuxiliaryEffectSlots current.");
        }


        alEffecti = (LPALEFFECTI)(alGetProcAddress("alEffecti"));

        if (!alEffecti)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to make a alEffecti current.");
        }


        alEffectf = (LPALEFFECTF)(alGetProcAddress("alEffectf"));

        if (!alEffectf)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to make a alEffectf current.");
        }


        alEffectfv = (LPALEFFECTFV)(alGetProcAddress("alEffectfv"));

        if (!alEffectfv)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to make a alEffectfv current.");
        }


        alAuxiliaryEffectSloti = (LPALAUXILIARYEFFECTSLOTI)(alGetProcAddress("alAuxiliaryEffectSloti"));

        if (!alAuxiliaryEffectSloti)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to make a alAuxiliaryEffectSloti current.");
        }


        alDeleteAuxiliaryEffectSlots = (LPALDELETEAUXILIARYEFFECTSLOTS)(alGetProcAddress("alDeleteAuxiliaryEffectSlots"));

        if (!alDeleteAuxiliaryEffectSlots)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to make a alDeleteAuxiliaryEffectSlots current.");
        }
    }

    if (is_succeed)
    {
        alGetError();

        alGenAuxiliaryEffectSlots(1, &oal_effect_slot);

        if (alGetError() != AL_NO_ERROR)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to generate an effect slot.");
        }
    }

    if (is_succeed)
    {
        alGetError();

        alGenEffects(1, &oal_effect);

        if (alGetError() != AL_NO_ERROR)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to generate an effect.");
        }
    }

    if (is_succeed)
    {
        alGetError();

        alEffecti(oal_effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);

        if (alGetError() != AL_NO_ERROR)
        {
            is_succeed = 0;
            printf("%s\n", "EAX reverb not supported.");
        }
    }

    if (is_succeed)
    {
#if 1
        EFXEAXREVERBPROPERTIES props = EFX_REVERB_PRESET_BATHROOM;

        alGetError();

        alEffectf(oal_effect, AL_EAXREVERB_DENSITY, props.flDensity);
        alEffectf(oal_effect, AL_EAXREVERB_DIFFUSION, props.flDiffusion);
        alEffectf(oal_effect, AL_EAXREVERB_GAIN, props.flGain);
        alEffectf(oal_effect, AL_EAXREVERB_GAINHF, props.flGainHF);
        alEffectf(oal_effect, AL_EAXREVERB_GAINLF, props.flGainLF);
        alEffectf(oal_effect, AL_EAXREVERB_DECAY_TIME, props.flDecayTime);
        alEffectf(oal_effect, AL_EAXREVERB_DECAY_HFRATIO, props.flDecayHFRatio);
        alEffectf(oal_effect, AL_EAXREVERB_DECAY_LFRATIO, props.flDecayLFRatio);
        alEffectf(oal_effect, AL_EAXREVERB_REFLECTIONS_GAIN, props.flReflectionsGain);
        alEffectf(oal_effect, AL_EAXREVERB_REFLECTIONS_DELAY, props.flReflectionsDelay);
        alEffectfv(oal_effect, AL_EAXREVERB_REFLECTIONS_PAN, props.flReflectionsPan);
        alEffectf(oal_effect, AL_EAXREVERB_LATE_REVERB_GAIN, props.flLateReverbGain);
        alEffectf(oal_effect, AL_EAXREVERB_LATE_REVERB_DELAY, props.flLateReverbDelay);
        alEffectfv(oal_effect, AL_EAXREVERB_LATE_REVERB_PAN, props.flLateReverbPan);
        alEffectf(oal_effect, AL_EAXREVERB_ECHO_TIME, props.flEchoTime);
        alEffectf(oal_effect, AL_EAXREVERB_ECHO_DEPTH, props.flEchoDepth);
        alEffectf(oal_effect, AL_EAXREVERB_MODULATION_TIME, props.flModulationTime);
        alEffectf(oal_effect, AL_EAXREVERB_MODULATION_DEPTH, props.flModulationDepth);
        alEffectf(oal_effect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, props.flAirAbsorptionGainHF);
        alEffectf(oal_effect, AL_EAXREVERB_HFREFERENCE, props.flHFReference);
        alEffectf(oal_effect, AL_EAXREVERB_LFREFERENCE, props.flLFReference);
        alEffectf(oal_effect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, props.flRoomRolloffFactor);
        alEffecti(oal_effect, AL_EAXREVERB_DECAY_HFLIMIT, props.iDecayHFLimit);

        if (alGetError() != AL_NO_ERROR)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to set EAX reverb properties.");
        }
#endif
    }

    if (is_succeed)
    {
        alGetError();

        alAuxiliaryEffectSloti(oal_effect_slot, AL_EFFECTSLOT_EFFECT, oal_effect);

        if (alGetError() != AL_NO_ERROR)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to load effect in slot.");
        }
    }

    if (is_succeed)
    {
        alGetError();

        alGenSources(1, &oal_source);

        if (alGetError() != AL_NO_ERROR)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to generate a source.");
        }
    }

    if (is_succeed)
    {
        alGetError();

        alSource3i(oal_source, AL_AUXILIARY_SEND_FILTER, oal_effect_slot, 0, 0);

        if (alGetError() != AL_NO_ERROR)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to configure a send.");
        }
    }

    if (is_succeed)
    {
        alGetError();

        alSourcePlay(oal_source);

        if (alGetError() != AL_NO_ERROR)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to play a source.");
        }
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
        dst_buffer = malloc(sizeof(float) * sample_count * channel_count);

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
    alDeleteSources(1, &oal_source);

    if (alDeleteEffects)
    {
        alDeleteEffects(1, &oal_effect);
    }

    if (alDeleteAuxiliaryEffectSlots)
    {
        alDeleteAuxiliaryEffectSlots(1, &oal_effect_slot);
    }

    alcMakeContextCurrent(NULL);
    alcDestroyContext(oal_context);
    alcCloseDevice(oal_device);

    free(dst_buffer);
    free(src_buffer);

    fclose(src_stream);
    fclose(dst_stream);

    return (is_succeed ? 0 : 2);
}
