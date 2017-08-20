#define NOMINMAX


#include <cassert>
#include <cmath>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include "AL\al.h"
#include "AL\alc.h"
#include "AL\alext.h"
#include "AL\efx.h"
#include "AL\efx-presets.h"


extern "C" void aluMixData(
    ALCdevice* device,
    ALvoid* OutBuffer,
    ALsizei NumSamples);


float mb_to_gain(
    float mb)
{
    return std::log10f(mb / 2000.0F);
}


int main()
{
    const auto src_file_name = "f:\\temp\\rev\\in.raw";
    const auto dst_file_name = "f:\\temp\\rev\\out.raw";


    std::ifstream src_stream(
        src_file_name,
        std::ios_base::in | std::ios_base::binary | std::ios_base::ate);

    if (!src_stream.is_open())
    {
        std::cout << "Failed to open a stream \"" << src_file_name << "\"." << std::endl;
        return 1;
    }

    auto stream_size = src_stream.tellg();

    src_stream.seekg(0);

    if (src_stream.bad() || src_stream.fail())
    {
        std::cout << "Failed to seek to beginning." << std::endl;
        return 1;
    }


    auto data_size = static_cast<std::streamoff>(stream_size);

    if (data_size < 2 || (data_size % 2) != 0)
    {
        std::cout << "Invalid data size." << std::endl;
        return 1;
    }

    using Buffer = std::vector<char>;
    Buffer buffer(static_cast<size_t>(data_size));

    src_stream.read(buffer.data(), data_size);

    if (src_stream.gcount() != data_size)
    {
        std::cout << "Failed to read data." << std::endl;
        return 1;
    }


    auto is_succeed = true;

    ALCdevice* oal_device = nullptr;

    if (is_succeed)
    {
        oal_device = ::alcOpenDevice(nullptr);

        if (!oal_device)
        {
            is_succeed = false;
            std::cout << "Failed to open device." << std::endl;
        }
    }

    if (is_succeed)
    {
        if (::alcIsExtensionPresent(oal_device, "ALC_EXT_EFX") == AL_FALSE)
        {
            is_succeed = false;
            std::cout << "No EFX." << std::endl;
        }
    }

    const std::array<ALCint, 5> context_attribs = {
        ALC_MAX_AUXILIARY_SENDS,
        1,
        ALC_FORMAT_CHANNELS_SOFT,
        ALC_MONO_SOFT,

        0,
    };

    ALCcontext* oal_context = nullptr;

    if (is_succeed)
    {
        oal_context = ::alcCreateContext(oal_device, context_attribs.data());

        if (!oal_context)
        {
            is_succeed = false;
            std::cout << "Failed to create a context." << std::endl;
        }
    }

    if (is_succeed)
    {
        auto result = ::alcMakeContextCurrent(oal_context);

        if (result == AL_FALSE)
        {
            is_succeed = false;
            std::cout << "Failed to make a context current." << std::endl;
        }
    }


    ALCint sends_count = 0;

    ::alcGetIntegerv(oal_device, ALC_MAX_AUXILIARY_SENDS, 1, &sends_count);


    LPALGENEFFECTS alGenEffects = nullptr;
    LPALDELETEEFFECTS alDeleteEffects = nullptr;
    LPALISEFFECT alIsEffect = nullptr;
    LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots = nullptr;
    LPALEFFECTI alEffecti = nullptr;
    LPALEFFECTF alEffectf = nullptr;
    LPALEFFECTFV alEffectfv = nullptr;
    LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti = nullptr;
    LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots = nullptr;

    if (is_succeed)
    {
        alGenEffects = reinterpret_cast<LPALGENEFFECTS>(::alGetProcAddress("alGenEffects"));

        if (!alGenEffects)
        {
            is_succeed = false;
            std::cout << "Failed to get alGenEffects symbol." << std::endl;
        }


        alDeleteEffects = reinterpret_cast<LPALDELETEEFFECTS>(::alGetProcAddress("alDeleteEffects"));

        if (!alDeleteEffects)
        {
            is_succeed = false;
            std::cout << "Failed to get alDeleteEffects symbol." << std::endl;
        }


        alIsEffect = reinterpret_cast<LPALISEFFECT>(::alGetProcAddress("alIsEffect"));

        if (!alIsEffect)
        {
            is_succeed = false;
            std::cout << "Failed to get alIsEffect symbol." << std::endl;
        }


        alGenAuxiliaryEffectSlots = reinterpret_cast<LPALGENAUXILIARYEFFECTSLOTS>(::alGetProcAddress("alGenAuxiliaryEffectSlots"));

        if (!alGenAuxiliaryEffectSlots)
        {
            is_succeed = false;
            std::cout << "Failed to get alGenAuxiliaryEffectSlots symbol." << std::endl;
        }


        alEffecti = reinterpret_cast<LPALEFFECTI>(::alGetProcAddress("alEffecti"));

        if (!alEffecti)
        {
            is_succeed = false;
            std::cout << "Failed to get alEffecti symbol." << std::endl;
        }


        alEffectf = reinterpret_cast<LPALEFFECTF>(::alGetProcAddress("alEffectf"));

        if (!alEffectf)
        {
            is_succeed = false;
            std::cout << "Failed to get alEffectf symbol." << std::endl;
        }


        alEffectfv = reinterpret_cast<LPALEFFECTFV>(::alGetProcAddress("alEffectfv"));

        if (!alEffectfv)
        {
            is_succeed = false;
            std::cout << "Failed to get alEffectfv symbol." << std::endl;
        }


        alAuxiliaryEffectSloti = reinterpret_cast<LPALAUXILIARYEFFECTSLOTI>(::alGetProcAddress("alAuxiliaryEffectSloti"));

        if (!alAuxiliaryEffectSloti)
        {
            is_succeed = false;
            std::cout << "Failed to get alAuxiliaryEffectSloti symbol." << std::endl;
        }


        alDeleteAuxiliaryEffectSlots = reinterpret_cast<LPALDELETEAUXILIARYEFFECTSLOTS>(::alGetProcAddress("alDeleteAuxiliaryEffectSlots"));

        if (!alDeleteAuxiliaryEffectSlots)
        {
            is_succeed = false;
            std::cout << "Failed to get alDeleteAuxiliaryEffectSlots symbol." << std::endl;
        }
    }


    ALuint oal_effect_slot = AL_EFFECTSLOT_NULL;
    ALuint oal_effect = AL_EFFECT_NULL;

    if (is_succeed)
    {
        static_cast<void>(::alGetError());

        alGenAuxiliaryEffectSlots(1, &oal_effect_slot);

        if (::alGetError() != AL_NO_ERROR)
        {
            is_succeed = false;
            std::cout << "Failed to generate an effect slot." << std::endl;
        }
    }

    if (is_succeed)
    {
        static_cast<void>(::alGetError());

        alGenEffects(1, &oal_effect);

        if (::alGetError() != AL_NO_ERROR)
        {
            is_succeed = false;
            std::cout << "Failed to generate an effect." << std::endl;
        }
    }

    if (is_succeed)
    {
        static_cast<void>(::alGetError());

        alEffecti(oal_effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);

        if (::alGetError() != AL_NO_ERROR)
        {
            is_succeed = false;
            std::cout << "EAX reverb not supported." << std::endl;
        }
    }

    if (is_succeed)
    {
#if 1
        EFXEAXREVERBPROPERTIES props = EFX_REVERB_PRESET_BATHROOM;

        static_cast<void>(::alGetError());

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

        if (::alGetError() != AL_NO_ERROR)
        {
            is_succeed = false;
            std::cout << "Failed to set EAX reverb properties." << std::endl;
        }
#endif
    }

    if (is_succeed)
    {
        static_cast<void>(::alGetError());

        alAuxiliaryEffectSloti(oal_effect_slot, AL_EFFECTSLOT_EFFECT, oal_effect);

        if (::alGetError() != AL_NO_ERROR)
        {
            is_succeed = false;
            std::cout << "Failed to load effect in slot." << std::endl;
        }
    }


    ALuint oal_buffer = 0;

    if (is_succeed)
    {
        static_cast<void>(::alGetError());

        ::alGenBuffers(1, &oal_buffer);

        if (::alGetError() != AL_NO_ERROR)
        {
            is_succeed = false;
            std::cout << "Failed to generate a buffer." << std::endl;
        }
    }

    if (is_succeed)
    {
        static_cast<void>(::alGetError());

        ::alBufferData(oal_buffer, AL_FORMAT_MONO16, buffer.data(), static_cast<ALsizei>(data_size), 44100);

        if (::alGetError() != AL_NO_ERROR)
        {
            is_succeed = false;
            std::cout << "Failed to fill OAL buffer." << std::endl;
        }
    }


    ALuint oal_source = 0;

    if (is_succeed)
    {
        static_cast<void>(::alGetError());

        ::alGenSources(1, &oal_source);

        if (::alGetError() != AL_NO_ERROR)
        {
            is_succeed = false;
            std::cout << "Failed to generate a source." << std::endl;
        }
    }

    if (is_succeed)
    {
        ::alSourcei(oal_source, AL_SOURCE_RELATIVE, AL_TRUE);
        ::alSource3f(oal_source, AL_POSITION, 0.0F, 0.0F, 0.0F);
        ::alSource3f(oal_source, AL_VELOCITY, 0.0F, 0.0F, 0.0F);

        if (::alGetError() != AL_NO_ERROR)
        {
            is_succeed = false;
            std::cout << "Failed to make a source relative." << std::endl;
        }
    }

    if (is_succeed)
    {
        static_cast<void>(::alGetError());

        ::alSourcei(oal_source, AL_BUFFER, static_cast<ALint>(oal_buffer));

        if (::alGetError() != AL_NO_ERROR)
        {
            is_succeed = false;
            std::cout << "Failed to assign a buffer to a source." << std::endl;
        }
    }

    if (is_succeed)
    {
        static_cast<void>(::alGetError());

        ::alSourcei(oal_source, AL_LOOPING, AL_TRUE);

        if (::alGetError() != AL_NO_ERROR)
        {
            is_succeed = false;
            std::cout << "Failed to set a looping on a source." << std::endl;
        }
    }

    if (is_succeed)
    {
        static_cast<void>(::alGetError());

        ::alSource3i(oal_source, AL_AUXILIARY_SEND_FILTER, oal_effect_slot, 0, 0);

        if (::alGetError() != AL_NO_ERROR)
        {
            is_succeed = false;
            std::cout << "Failed to configure send." << std::endl;
        }
    }

    if (is_succeed)
    {
        static_cast<void>(::alGetError());

        ::alSourcePlay(oal_source);

        if (::alGetError() != AL_NO_ERROR)
        {
            is_succeed = false;
            std::cout << "Failed to play a source." << std::endl;
        }
    }


    std::ofstream dst_stream{
        dst_file_name,
        std::ios_base::out | std::ios_base::binary | std::ios_base::trunc};

    if (is_succeed)
    {
        if (!dst_stream.is_open())
        {
            is_succeed = false;
            std::cout << "Failed to open a destination file." << std::endl;
        }
    }

    if (is_succeed)
    {
        constexpr auto sample_count = 1024;
        constexpr auto channel_count = 2;

        using DstBuffer = std::vector<float>;
        auto dst_buffer = DstBuffer{};
        dst_buffer.resize(sample_count * channel_count);

        auto remain = static_cast<int>(stream_size) / 2;

        while (remain > 0)
        {
            ::aluMixData(oal_device, dst_buffer.data(), sample_count);

            const auto write_sample_count = std::min(sample_count, remain);
            const auto write_size = write_sample_count * 4 * channel_count;

            dst_stream.write(
                reinterpret_cast<const char*>(dst_buffer.data()),
                    write_size);

            remain -= write_sample_count;
        }
    }

    ::alSourceStop(oal_source);

    ::alSourcei(oal_source, AL_BUFFER, 0);

    ::alDeleteSources(1, &oal_source);

    ::alDeleteBuffers(1, &oal_buffer);

    alDeleteEffects(1, &oal_effect);

    alDeleteAuxiliaryEffectSlots(1, &oal_effect_slot);

    static_cast<void>(::alcMakeContextCurrent(nullptr));

    ::alcDestroyContext(oal_context);

    static_cast<void>(::alcCloseDevice(oal_device));

    return (is_succeed ? 0 : 2);
}
