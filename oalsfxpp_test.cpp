#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "oalsfxpp.h"


int main()
{
    const auto src_file_name = "f:\\temp\\rev\\in.raw";
    const auto dst_file_name = "f:\\temp\\rev\\out.raw";
    const auto channel_count = 1;
    const auto sampling_rate = 44100;
    auto is_succeed = true;

    auto src_stream = std::ifstream{};

    if (is_succeed)
    {
        src_stream.open(src_file_name, std::ios_base::in | std::ios_base::binary | std::ios_base::ate);

        if (!src_stream.is_open())
        {
            is_succeed = false;
            std::cout << "Failed to open a source stream." << std::endl;
        }
    }


    auto src_data_size = 0;

    if (is_succeed)
    {
        src_data_size = static_cast<int>(src_stream.tellg());

        src_stream.seekg(0);

        if (src_stream.fail())
        {
            is_succeed = false;
            std::cout << "Failed to reset a source stream's position." << std::endl;
        }
    }

    if (is_succeed)
    {
        if (src_data_size < 2 || (src_data_size % 2) != 0)
        {
            is_succeed = false;
            std::cout << "Invalid data size." << std::endl;
        }
    }


    using SrcBuffer = std::vector<char>;
    auto src_buffer = SrcBuffer{};
    const int16_t* src_buffer16 = nullptr;

    if (is_succeed)
    {
        src_buffer.resize(src_data_size);
        src_buffer16 = reinterpret_cast<const int16_t*>(src_buffer.data());

        src_stream.read(src_buffer.data(), src_data_size);

        if (src_stream.fail())
        {
            is_succeed = false;
            std::cout << "Failed to read a source file." << std::endl;
        }
        else
        {
            if (src_stream.gcount() != src_data_size)
            {
                is_succeed = false;
                std::cout << "Failed to read a source file." << std::endl;
            }
        }
    }

    const auto total_sample_count = src_data_size / 2;

    using SrcBufferF = std::vector<float>;
    auto src_buffer_f32 = SrcBufferF{};
    src_buffer_f32.resize(total_sample_count);

    for (int i = 0; i < total_sample_count; ++i)
    {
        src_buffer_f32[i] = static_cast<float>(src_buffer16[i]) / 32768.0F;
    }

    Api api;

    if (is_succeed)
    {
        const auto channel_format = Api::channel_count_to_channel_format(channel_count);

        is_succeed = api.initialize(channel_format, sampling_rate, 1);
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
        std::cout << "11. Ring modulator\n";
        std::cout << "12. Null\n" << std::endl;

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

            case 12:
                effect_type = EffectType::null;
                break;

            default:
                effect_number = 0;
                break;
            }
        }

        api.set_effect_type(0, effect_type);
        api.apply_changes();
    }

    auto dst_stream = std::ofstream{};

    if (is_succeed)
    {
        dst_stream.open(dst_file_name, std::ios_base::out | std::ios_base::binary);

        if (!dst_stream.is_open())
        {
            is_succeed = false;
            std::cout << "Failed to open a destination file.\n";
        }
    }

    using DstBuffer = std::vector<float>;
    auto dst_buffer = DstBuffer{};

    if (is_succeed)
    {
        const auto dst_data_size = sizeof(float) * total_sample_count * channel_count;

        dst_buffer.resize(total_sample_count * channel_count);

        api.mix(total_sample_count, src_buffer_f32.data(), dst_buffer.data());

        dst_stream.write(reinterpret_cast<const char*>(dst_buffer.data()), dst_data_size);

        if (dst_stream.bad())
        {
            is_succeed = false;
            std::cout << "Failed to write out data.\n";
        }
    }

    return (is_succeed ? 0 : 2);
}
