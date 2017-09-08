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
    int num_sends);

void aluMixData(
    ALCdevice* device,
    void* OutBuffer,
    int NumSamples,
    const float* src_samples);


int main()
{
    const char* src_file_name = "f:\\temp\\rev\\in.raw";
    const char* dst_file_name = "f:\\temp\\rev\\out.raw";
    const int sample_count = 1024;
    const int channel_count = 1;
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
        alcCreateContext(oal_device, nullptr);
    }

#if 1
    if (is_succeed)
    {
        InitEffectParams(oal_device->effect, AL_EFFECT_EAXREVERB);
        UpdateEffectSlotProps(oal_device->effect_slot);
    }

    if (is_succeed)
    {
        EFXEAXREVERBPROPERTIES props = EFX_REVERB_PRESET_BATHROOM;
        auto oal_props = &oal_device->effect->props;

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
    }
#endif

#if 0
    if (is_succeed)
    {
        InitEffectParams(oal_device->effect, AL_EFFECT_CHORUS);
        UpdateEffectSlotProps(oal_device->effect_slot);
    }

    if (is_succeed)
    {
        auto oal_props = &oal_device->effect->props;
        oal_props->chorus.delay = AL_CHORUS_DEFAULT_DELAY;
        oal_props->chorus.depth = AL_CHORUS_DEFAULT_DEPTH;
        oal_props->chorus.feedback = AL_CHORUS_DEFAULT_FEEDBACK;
        oal_props->chorus.phase = AL_CHORUS_DEFAULT_PHASE;
        oal_props->chorus.rate = AL_CHORUS_DEFAULT_RATE;
        oal_props->chorus.waveform = AL_CHORUS_DEFAULT_WAVEFORM;
    }
#endif

#if 0
    if (is_succeed)
    {
        InitEffectParams(oal_device->effect, AL_EFFECT_COMPRESSOR);
        UpdateEffectSlotProps(oal_device->effect_slot);
    }

    if (is_succeed)
    {
        auto oal_props = &oal_device->effect->props;
        oal_props->compressor.on_off = AL_TRUE;
    }
#endif

#if 0
    if (is_succeed)
    {
        InitEffectParams(oal_device->effect, AL_EFFECT_DEDICATED_DIALOGUE);
        UpdateEffectSlotProps(oal_device->effect_slot);
    }

    if (is_succeed)
    {
        auto oal_props = &oal_device->effect->props;
        oal_props->dedicated.gain = 1.0F;
    }
#endif

#if 0
    if (is_succeed)
    {
        InitEffectParams(oal_device->effect, AL_EFFECT_DISTORTION);
        UpdateEffectSlotProps(oal_device->effect_slot);
    }

    if (is_succeed)
    {
        auto oal_props = &oal_device->effect->props;
        oal_props->distortion.edge = AL_DISTORTION_DEFAULT_EDGE;
        oal_props->distortion.eq_bandwidth = AL_DISTORTION_DEFAULT_EQBANDWIDTH;
        oal_props->distortion.eq_center = AL_DISTORTION_DEFAULT_EQCENTER;
        oal_props->distortion.gain = AL_DISTORTION_DEFAULT_GAIN;
        oal_props->distortion.lowpass_cutoff = AL_DISTORTION_DEFAULT_LOWPASS_CUTOFF;
    }
#endif

#if 0
    if (is_succeed)
    {
        InitEffectParams(oal_device->effect, AL_EFFECT_ECHO);
        UpdateEffectSlotProps(oal_device->effect_slot);
    }

    if (is_succeed)
    {
        auto oal_props = &oal_device->effect->props;
        oal_props->echo.damping = AL_ECHO_DEFAULT_DAMPING;
        oal_props->echo.delay = AL_ECHO_DEFAULT_DELAY;
        oal_props->echo.feedback = AL_ECHO_DEFAULT_FEEDBACK;
        oal_props->echo.lr_delay = AL_ECHO_DEFAULT_LRDELAY;
        oal_props->echo.spread = AL_ECHO_DEFAULT_SPREAD;
    }
#endif

#if 0
    if (is_succeed)
    {
        InitEffectParams(oal_device->effect, AL_EFFECT_EQUALIZER);
        UpdateEffectSlotProps(oal_device->effect_slot);
    }

    if (is_succeed)
    {
        auto oal_props = &oal_device->effect->props;
        oal_props->equalizer.high_cutoff = AL_EQUALIZER_DEFAULT_HIGH_CUTOFF;
        oal_props->equalizer.high_gain = AL_EQUALIZER_DEFAULT_HIGH_GAIN;
        oal_props->equalizer.low_cutoff = AL_EQUALIZER_DEFAULT_LOW_CUTOFF;
        oal_props->equalizer.low_gain = AL_EQUALIZER_DEFAULT_LOW_GAIN;
        oal_props->equalizer.mid1_center = AL_EQUALIZER_DEFAULT_MID1_CENTER;
        oal_props->equalizer.mid1_gain = AL_EQUALIZER_DEFAULT_MID1_GAIN;
        oal_props->equalizer.mid1_width = AL_EQUALIZER_DEFAULT_MID1_WIDTH;
        oal_props->equalizer.mid2_center = AL_EQUALIZER_DEFAULT_MID2_CENTER;
        oal_props->equalizer.mid2_gain = AL_EQUALIZER_DEFAULT_MID2_GAIN;
        oal_props->equalizer.mid2_width = AL_EQUALIZER_DEFAULT_MID2_WIDTH;
    }
#endif

#if 0
    if (is_succeed)
    {
        InitEffectParams(oal_device->effect, AL_EFFECT_FLANGER);
        UpdateEffectSlotProps(oal_device->effect_slot);
    }

    if (is_succeed)
    {
        auto oal_props = &oal_device->effect->props;
        oal_props->flanger.delay = AL_FLANGER_DEFAULT_DELAY;
        oal_props->flanger.depth = AL_FLANGER_DEFAULT_DEPTH;
        oal_props->flanger.feedback = AL_FLANGER_DEFAULT_FEEDBACK;
        oal_props->flanger.phase = AL_FLANGER_DEFAULT_PHASE;
        oal_props->flanger.rate = AL_FLANGER_DEFAULT_RATE;
        oal_props->flanger.waveform = AL_FLANGER_DEFAULT_WAVEFORM;
    }
#endif

#if 0
    if (is_succeed)
    {
        InitEffectParams(oal_device->effect, AL_EFFECT_RING_MODULATOR);
        UpdateEffectSlotProps(oal_device->effect_slot);
    }

    if (is_succeed)
    {
        auto oal_props = &oal_device->effect->props;
        oal_props->modulator.frequency = AL_RING_MODULATOR_DEFAULT_FREQUENCY;
        oal_props->modulator.high_pass_cutoff = AL_RING_MODULATOR_DEFAULT_HIGHPASS_CUTOFF;
        oal_props->modulator.waveform = AL_RING_MODULATOR_DEFAULT_WAVEFORM;
    }
#endif

    if (is_succeed)
    {
        InitializeEffect(oal_device, oal_device->effect_slot, oal_device->effect);
        UpdateEffectSlotProps(oal_device->effect_slot);
    }

    if (is_succeed)
    {
        oal_device->source->send->slot = oal_device->effect_slot;
        UpdateSourceProps(oal_device->source, oal_device->voice, 1);
    }

    if (is_succeed)
    {
        alSourcePlay(0);
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

    alSourceStop(0);

    alcDestroyContext(nullptr);
    alcCloseDevice(oal_device);

    free(dst_buffer);
    free(src_buffer);

    fclose(src_stream);
    fclose(dst_stream);

    return (is_succeed ? 0 : 2);
}
