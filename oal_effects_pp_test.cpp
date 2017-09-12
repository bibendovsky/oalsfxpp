#include <malloc.h>
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <string>
#include "config.h"
#include "alMain.h"
#include "alEffect.h"
#include "alAuxEffectSlot.h"
#include "alSource.h"


void alu_mix_data(
    ALCdevice* device,
    void* OutBuffer,
    const int NumSamples,
    const float* src_samples);

ALCdevice* alcOpenDevice();

void alcCloseDevice(
    ALCdevice* device);


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
        oal_device = alcOpenDevice();

        if (!oal_device)
        {
            is_succeed = 0;
            printf("%s\n", "Failed to open device.");
        }
    }

    if (is_succeed)
    {
        std::cout << "1. EAX Reverb\n";
        std::cout << "2. Reverb\n";
        std::cout << "3. Chorus\n";
        std::cout << "4. Compressor\n";
        std::cout << "5. Dedicated (dialog)\n";
        std::cout << "6. Dedicated (low frequency)\n";
        std::cout << "7. Distortion\n";
        std::cout << "8. Echo\n";
        std::cout << "9. Equalizer\n";
        std::cout << "10. Flanger\n";
        std::cout << "11. Ring modulator\n\n";

        auto effect_number = 0;
        auto effect_number_string = std::string{};
        auto effect_type = EffectType::null;

        while (effect_number == 0)
        {
            std::cout << "Enter effect number: ";
            std::cin >> effect_number_string;

            try
            {
                effect_number = std::stoi(effect_number_string);
            }
            catch (const std::invalid_argument&)
            {
            }
            catch (const std::out_of_range&)
            {
            }

            switch (effect_number)
            {
            case 1:
                effect_type = EffectType::eax_reverb;
                break;

            case 2:
                effect_type = EffectType::reverb;
                break;

            case 3:
                effect_type = EffectType::chorus;
                break;

            case 4:
                effect_type = EffectType::compressor;
                break;

            case 5:
                effect_type = EffectType::dedicated_dialog;
                break;

            case 6:
                effect_type = EffectType::dedicated_low_frequency;
                break;

            case 7:
                effect_type = EffectType::distortion;
                break;

            case 8:
                effect_type = EffectType::echo;
                break;

            case 9:
                effect_type = EffectType::equalizer;
                break;

            case 10:
                effect_type = EffectType::flanger;
                break;

            case 11:
                effect_type = EffectType::ring_modulator;
                break;

            default:
                effect_number = 0;
                break;
            }
        }

        oal_device->effect_->initialize(effect_type);
    }

    if (is_succeed)
    {
        oal_device->effect_slot_->initialize_effect(oal_device);
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

            alu_mix_data(oal_device, dst_buffer, sample_count, &src_buffer_f32[offset]);

            if (fwrite(dst_buffer, 1, write_size, dst_stream) != write_size)
            {
                is_succeed = 0;
                printf("%s\n", "Failed to write out data.");
            }

            remain -= write_sample_count;
            offset += write_sample_count * channel_count;
        }
    }

    alcCloseDevice(oal_device);

    free(dst_buffer);
    free(src_buffer);

    fclose(src_stream);
    fclose(dst_stream);

    return (is_succeed ? 0 : 2);
}
