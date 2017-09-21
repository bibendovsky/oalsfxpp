#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "oalsfxpp.h"


enum class EndianId
{
    none,
    big,
    little,
}; // EndianId


namespace detail
{


struct EndianSwap
{
    static constexpr uint8_t swap(
        const uint8_t value)
    {
        return value;
    }

    static constexpr int8_t swap(
        const int8_t value)
    {
        return value;
    }

    static constexpr uint16_t swap(
        const uint16_t value)
    {
        return (value >> 8) | (value << 8);
    }

    static constexpr int16_t swap(
        const int16_t value)
    {
        return static_cast<int16_t>(swap(static_cast<uint16_t>(value)));
    }

    static constexpr uint32_t swap(
        const uint32_t value)
    {
        constexpr auto ooxxooxx = uint32_t{0x00FF00FF};
        constexpr auto xxooxxoo = uint32_t{0xFF00FF00};

        const auto swap16 = (value << 16) | (value >> 16);
        return ((swap16 << 8) & xxooxxoo) | ((swap16 >> 8) & ooxxooxx);
    }

    static constexpr int32_t swap(
        const int32_t value)
    {
        return static_cast<int32_t>(swap(static_cast<uint32_t>(value)));
    }

    static constexpr uint64_t swap(
        const uint64_t value)
    {
        constexpr auto ooxxooxx = uint64_t{0x0000FFFF0000FFFF};
        constexpr auto xxooxxoo = uint64_t{0xFFFF0000FFFF0000};

        constexpr auto oxoxoxox = uint64_t{0x00FF00FF00FF00FF};
        constexpr auto xoxoxoxo = uint64_t{0xFF00FF00FF00FF00};

        const auto swap32 = (value << 32) | (value >> 32);
        const auto swap16 = ((swap32 & ooxxooxx) << 16) | ((swap32 & xxooxxoo) >> 16);
        return ((swap16 & oxoxoxox) << 8) | ((swap16 & xoxoxoxo) >> 8);
    }

    static constexpr int64_t swap(
        const int64_t value)
    {
        return static_cast<int64_t>(swap(static_cast<uint64_t>(value)));
    }
}; // EndianSwap


template<EndianId TId>
struct Endian
{
    // Returns swaped bytes on little-endian platform or as-is otherwise.
    template<typename T>
    static T big(
        const T value) = delete;

    // Returns swaped bytes on big-endian platform or as-is otherwise.
    template<typename T>
    static T little(
        const T value) = delete;
}; // Endian


template<>
struct Endian<EndianId::big>
{
    template<typename T>
    static T big(
        const T value)
    {
        return value;
    }

    template<typename T>
    static T little(
        const T value)
    {
        return detail::EndianSwap::swap(value);
    }
}; // Endian


template<>
struct Endian<EndianId::little>
{
    template<typename T>
    static T big(
        const T value)
    {
        return detail::EndianSwap::swap(value);
    }

    template<typename T>
    static T little(
        const T value)
    {
        return value;
    }
}; // Endian


} // detail


#ifdef OALSFXPP_BIG_ENDIAN
using Endian = detail::Endian<EndianId::big>;
#else
using Endian = detail::Endian<EndianId::little>;
#endif // OALSFXPP_BIG_ENDIAN


struct StreamHelper
{
    using Stream = std::iostream;


    StreamHelper(
        Stream& stream)
        :
        stream_{stream}
    {
    }

    template<typename T>
    T read()
    {
        auto value = T{};
        stream_.read(reinterpret_cast<char*>(&value), sizeof(T));
        return value;
    }

    bool read(
        const std::size_t raw_buffer_size,
        void* raw_buffer)
    {
        stream_.read(static_cast<char*>(raw_buffer), raw_buffer_size);

        if (stream_.fail())
        {
            return false;
        }

        if (stream_.gcount() != raw_buffer_size)
        {
            return false;
        }

        return true;
    }

    template<typename T>
    void write(
        const T value)
    {
        stream_.write(reinterpret_cast<const char*>(&value), sizeof(T));
    }

    template<typename TDst, typename TSrc>
    void write_le(
        const TSrc value)
    {
        write(Endian::little(static_cast<TDst>(value)));
    }

    template<typename TDst, typename TSrc>
    void write_be(
        const TSrc value)
    {
        write(Endian::big(static_cast<TDst>(value)));
    }


private:
    Stream& stream_;
}; // StreamHelper


using FourCc = uint32_t;
using FourCcString = const char (&)[5];

struct FourCcs
{
    static constexpr FourCc from_string(
        FourCcString string)
    {
        return
            (static_cast<FourCc>(string[0]) << 24) |
            (static_cast<FourCc>(string[1]) << 16) |
            (static_cast<FourCc>(string[2]) << 8) |
            (static_cast<FourCc>(string[3]) << 0);
    }

    static constexpr FourCc get_riff()
    {
        return from_string("RIFF");
    }

    static constexpr FourCc get_wave()
    {
        return from_string("WAVE");
    }

    static constexpr FourCc get_fmt()
    {
        return from_string("fmt ");
    }

    static constexpr FourCc get_data()
    {
        return from_string("data");
    }
}; // FourCcs

struct WavFile
{
    using Buffer = std::vector<char>;
    using SampleBuffer = std::vector<float>;


    static constexpr auto min_format_chunk_size = 16;
    static constexpr auto pcm_format_tag = 1;


    bool read(
        const std::string& file_name)
    {
        if (file_name.empty())
        {
            error_message_ = "No file name.";
            return false;
        }

        auto stream = std::fstream{
            file_name.c_str(),
            std::ios_base::in | std::ios_base::binary | std::ios_base::ate};

        if (!stream.is_open())
        {
            error_message_ = "Failed to open a file \"" + file_name + "\".";
            return false;
        }

        const auto stream_size = static_cast<std::uint32_t>(stream.tellg());

        stream.seekg(0);


        auto stream_helper = StreamHelper{stream};


        // RIFF id
        //
        const auto riff_four_cc = Endian::big(stream_helper.read<uint32_t>());

        if (riff_four_cc != FourCcs::get_riff())
        {
            error_message_ = "Not a WAV stream.";
            return false;
        }

        // RIFF size
        //
        const auto riff_chunk_size = Endian::little(stream_helper.read<uint32_t>());

        if ((riff_chunk_size + 8) < stream_size)
        {
            error_message_ = "Truncated RIFF stream.";
            return false;
        }

        // WAVE id
        //
        const auto wave_four_cc = Endian::big(stream_helper.read<uint32_t>());

        if (wave_four_cc != FourCcs::get_wave())
        {
            error_message_ = "Not a WAV stream.";
            return false;
        }

        // Chunks
        //
        auto is_format_read = false;
        auto is_data_read = false;

        while (!stream.eof() && !stream.fail() && !(is_format_read && is_data_read))
        {
            auto is_format = false;
            auto is_data = false;

            // Chunk id
            //
            const auto chunk_four_cc = Endian::big(stream_helper.read<uint32_t>());

            if (chunk_four_cc == FourCcs::get_fmt())
            {
                is_format = true;

                if (is_format_read)
                {
                    error_message_ = "Multiple format chunks.";
                    return false;
                }
            }
            else if (chunk_four_cc == FourCcs::get_data())
            {
                is_data = true;

                if (is_data_read)
                {
                    error_message_ = "Multiple data chunks.";
                    return false;
                }
            }

            // Chunk size
            //
            const auto chunk_size = Endian::little(stream_helper.read<uint32_t>());

            if (is_format && chunk_size < min_format_chunk_size)
            {
                error_message_ = "Invalid format chunk.";
                return false;
            }
            else if (is_data && chunk_size == 0)
            {
                error_message_ = "No data to read.";
                return false;
            }

            const auto aligned_chunk_size = ((chunk_size + 1) / 2) * 2;


            // Chunk data
            //
            if (is_format)
            {
                is_format_read = true;

                // Format tag
                //
                const auto format_tag = Endian::little(stream_helper.read<uint16_t>());

                if (format_tag != pcm_format_tag)
                {
                    error_message_ = "Expected a PCM codec.";
                    return false;
                }

                // Channel count
                //
                const auto channel_count = Endian::little(stream_helper.read<uint16_t>());

                if (channel_count < oalsfxpp::Api::get_min_channels() ||
                    channel_count > oalsfxpp::Api::get_max_channels())
                {
                    error_message_ = "Channel count is out of range.";
                    return false;
                }

                // Sampling rate
                //
                const auto sampling_rate = Endian::little(stream_helper.read<uint32_t>());

                if (sampling_rate < static_cast<uint32_t>(oalsfxpp::Api::get_min_sampling_rate()) ||
                    sampling_rate > static_cast<uint32_t>(oalsfxpp::Api::get_max_sampling_rate()))
                {
                    error_message_ = "Sampling rate is out of range.";
                    return false;
                }

                // Average bytes per sec
                //
                const auto avg_bytes_per_sec = Endian::little(stream_helper.read<uint32_t>());

                // Block align
                //
                const auto block_align = Endian::little(stream_helper.read<uint16_t>());

                // Bit depth
                //
                const auto bit_depth = Endian::little(stream_helper.read<uint16_t>());

                if (bit_depth != 8 && bit_depth != 16)
                {
                    error_message_ = "Unsupported bit depth.";
                    return false;
                }

                // Remain data
                //
                const auto format_remain = aligned_chunk_size - min_format_chunk_size;

                if (format_remain > 0)
                {
                    stream.seekg(format_remain, std::ios_base::cur);
                }

                channel_count_ = channel_count;
                sampling_rate_ = sampling_rate;
                bit_depth_ = bit_depth;
            }
            else if (is_data)
            {
                is_data_read = true;

                auto read_size = chunk_size;

                if (bit_depth_ == 16 && chunk_size != aligned_chunk_size)
                {
                    read_size += 1;
                }

                sample_count_ = read_size / channel_count_ / (bit_depth_ / 8);

                auto buffer = Buffer{};
                buffer.resize(read_size);

                if (!stream_helper.read(read_size, buffer.data()))
                {
                    error_message_ = "Failed to read a data chunk.";
                    return false;
                }

                convert_samples(buffer);
            }
            else
            {
                stream.seekg(aligned_chunk_size, std::ios_base::cur);
            }
        }

        if (!is_format_read)
        {
            error_message_ = "Format chunk not found.";
            return false;
        }

        if (!is_data_read)
        {
            error_message_ = "Data chunk not found.";
            return false;
        }

        return true;
    }

    bool write_pcm_s16_le(
        const std::string& file_name,
        const SampleBuffer& sample_buffer)
    {
        if (file_name.empty())
        {
            error_message_ = "No file name.";
            return false;
        }

        if (sample_buffer.empty())
        {
            error_message_ = "No data to write.";
            return false;
        }

        const auto total_samples = sample_count_ * channel_count_;

        if (sample_buffer.size() != static_cast<std::size_t>(total_samples))
        {
            error_message_ = "Sample count mismatch.";
            return false;
        }

        auto stream = std::fstream{
            file_name,
            std::ios_base::out | std::ios_base::binary};

        if (!stream.is_open())
        {
            error_message_ = "Failed to open a file \"" + file_name + "\".";
            return false;
        }

        auto stream_helper = StreamHelper{stream};

        const auto chunk_header_size =
            4 + // id
            4 + // size
            0
        ;

        const auto data_chunk_size = 2 * channel_count_ * total_samples;

        const auto riff_chunk_size =
            4 + // "WAVE"
            chunk_header_size + min_format_chunk_size + // format chunk
            chunk_header_size + data_chunk_size + // data chunk
            0
        ;


        // "RIFF"
        //
        stream_helper.write_be<uint32_t>(FourCcs::get_riff());
        stream_helper.write_le<uint32_t>(riff_chunk_size);

        // "WAVE"
        //
        stream_helper.write_be<uint32_t>(FourCcs::get_wave());

        // "fmt "
        //
        const auto bit_depth = 16;
        const auto block_align = channel_count_ * (bit_depth / 8);
        const auto avg_bytes_per_sec = block_align * sampling_rate_;
        stream_helper.write_be<uint32_t>(FourCcs::get_fmt());
        stream_helper.write_le<uint32_t>(min_format_chunk_size);
        stream_helper.write_le<uint16_t>(pcm_format_tag);
        stream_helper.write_le<uint16_t>(channel_count_);
        stream_helper.write_le<uint32_t>(sampling_rate_);
        stream_helper.write_le<uint32_t>(avg_bytes_per_sec);
        stream_helper.write_le<uint16_t>(block_align);
        stream_helper.write_le<uint16_t>(bit_depth);

        // "data"
        //
        stream_helper.write_be<uint32_t>(FourCcs::get_data());
        stream_helper.write_le<uint32_t>(data_chunk_size);

        // Calculate a scale to avoid clipping
        //
        auto min_gain = -1.0F;
        auto max_gain = 1.0F;

        for (int i = 0; i < total_samples; ++i)
        {
            if (sample_buffer[i] < min_gain)
            {
                min_gain = sample_buffer[i];
            }
            else if (sample_buffer[i] > max_gain)
            {
                max_gain = sample_buffer[i];
            }
        }

        const auto scale = 1.0F / std::max(max_gain, -min_gain);


        // Convert samples into temporary buffer and write them out
        //
        auto src_buffer_offset = 0;
        auto remain_sample_count = total_samples;

        auto dst_buffer = Buffer16{};
        dst_buffer.resize(max_write_buffer_samples);

        while (remain_sample_count > 0)
        {
            const auto sample_count_to_write = std::min(remain_sample_count, max_write_buffer_samples);

            for (int i = 0; i < sample_count_to_write; ++i)
            {
                dst_buffer[i] = Endian::little(static_cast<int16_t>(scale * sample_buffer[src_buffer_offset + i] * 32767.0F));
            }

            const auto data_size = 2 * sample_count_to_write;

            stream.write(reinterpret_cast<const char*>(dst_buffer.data()), data_size);

            if (stream.bad())
            {
                error_message_ = "Failed to write data.";
                return false;
            }

            src_buffer_offset += sample_count_to_write;
            remain_sample_count -= sample_count_to_write;
        }

        return true;
    }

    const SampleBuffer& get_samples() const
    {
        return samples_;
    }

    int get_channel_count() const
    {
        return channel_count_;
    }

    int get_sampling_rate() const
    {
        return sampling_rate_;
    }

    int get_bit_depth() const
    {
        return bit_depth_;
    }

    int get_sample_count() const
    {
        return sample_count_;
    }

    const std::string& get_error_message() const
    {
        return error_message_;
    }


private:
    static constexpr auto max_write_buffer_samples = 4096;


    using Buffer16 = std::vector<int16_t>;


    int channel_count_;
    int sampling_rate_;
    int bit_depth_;
    int sample_count_;

    SampleBuffer samples_;
    std::string error_message_;


    void convert_samples(
        const Buffer& raw_buffer)
    {
        const auto total_samples = sample_count_ * channel_count_;

        auto& dst_buffer = samples_;
        dst_buffer.resize(total_samples);

        switch (bit_depth_)
        {
        case 8:
        {
            auto src_buffer = reinterpret_cast<const uint8_t*>(raw_buffer.data());

            for (int i = 0; i < total_samples; ++i)
            {
                dst_buffer[i] = (static_cast<int>(src_buffer[i]) - 128) / 128.0F;
            }

            break;
        }

        case 16:
        {
            auto src_buffer = reinterpret_cast<const int16_t*>(raw_buffer.data());

            for (int i = 0; i < total_samples; ++i)
            {
                dst_buffer[i] = Endian::little(src_buffer[i]) / 32768.0F;
            }

            break;
        }

        default:
            throw std::runtime_error{"Invalid bit depth."};
        }
    }
}; // WavFile


int main(
    int argc,
    char* argv[])
{
    if (argc != 3)
    {
        std::cout << "Usage:" << std::endl;
        std::cout << "program <src_file_name> <dst_file_name>" << std::endl;
        return 1;
    }

    const auto src_file_name = argv[1];
    const auto dst_file_name = argv[2];

    auto is_succeed = true;

    auto wav_file = WavFile{};

    if (is_succeed)
    {
        if (!wav_file.read(src_file_name))
        {
            is_succeed = false;
            std::cout << wav_file.get_error_message() << std::endl;
        }
    }

    oalsfxpp::Api api;

    if (is_succeed)
    {
        const auto channel_format = oalsfxpp::Api::channel_count_to_channel_format(wav_file.get_channel_count());

        is_succeed = api.initialize(channel_format, wav_file.get_sampling_rate(), 1);

        if (!is_succeed)
        {
            std::cout << api.get_error_message() << std::endl;
        }
    }

    if (is_succeed)
    {
        std::cout <<
            "1. EAX Reverb\n" <<
            "2. Reverb\n" <<
            "3. Chorus\n" <<
            "4. Compressor\n" <<
            "5. Dedicated (dialog)\n" <<
            "6. Dedicated (low frequency)\n" <<
            "7. Distortion\n" <<
            "8. Echo\n" <<
            "9. Equalizer\n" <<
            "10. Flanger\n" <<
            "11. Ring modulator\n" <<
            "12. Null\n" <<
            std::endl;

        auto effect_number = 0;
        auto effect_number_string = std::string{};
        auto effect_type = oalsfxpp::EffectType::null;

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
                effect_type = oalsfxpp::EffectType::eax_reverb;
                break;

            case 2:
                effect_type = oalsfxpp::EffectType::reverb;
                break;

            case 3:
                effect_type = oalsfxpp::EffectType::chorus;
                break;

            case 4:
                effect_type = oalsfxpp::EffectType::compressor;
                break;

            case 5:
                effect_type = oalsfxpp::EffectType::dedicated_dialog;
                break;

            case 6:
                effect_type = oalsfxpp::EffectType::dedicated_low_frequency;
                break;

            case 7:
                effect_type = oalsfxpp::EffectType::distortion;
                break;

            case 8:
                effect_type = oalsfxpp::EffectType::echo;
                break;

            case 9:
                effect_type = oalsfxpp::EffectType::equalizer;
                break;

            case 10:
                effect_type = oalsfxpp::EffectType::flanger;
                break;

            case 11:
                effect_type = oalsfxpp::EffectType::ring_modulator;
                break;

            case 12:
                effect_type = oalsfxpp::EffectType::null;
                break;

            default:
                effect_number = 0;
                break;
            }
        }

        api.set_effect_type(0, effect_type);
        api.apply_changes();
    }

    if (is_succeed)
    {
        const auto& src_samples = wav_file.get_samples();
        auto dst_samples = WavFile::SampleBuffer{};
        dst_samples.resize(src_samples.size());

        api.mix(wav_file.get_sample_count(), src_samples.data(), dst_samples.data());

        if (!wav_file.write_pcm_s16_le(dst_file_name, dst_samples))
        {
            is_succeed = false;
            std::cout << wav_file.get_error_message() << std::endl;
        }
    }

    return (is_succeed ? 0 : 2);
}
