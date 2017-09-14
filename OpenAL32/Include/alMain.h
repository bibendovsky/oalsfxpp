#ifndef AL_MAIN_H
#define AL_MAIN_H

#include <cstring>
#include <cassert>
#include <cmath>

#include <array>
#include <type_traits>
#include <vector>


struct ALCdevice;


// Set up the appropriate panning method and mixing method given the device
// properties.
void alu_init_renderer(
    ALCdevice* device);

void alu_mix_data(
    ALCdevice* device,
    void* dst_buffer,
    const int sample_count,
    const float* src_samples);


constexpr auto max_mix_gain = 16.0F; // +24dB

constexpr auto silence_threshold_gain = 0.00001F; // -100dB


enum class EffectType
{
    null,
    chorus,
    compressor,
    dedicated_dialog,
    dedicated_low_frequency,
    distortion,
    echo,
    equalizer,
    flanger,
    ring_modulator,
    reverb,
    eax_reverb,
}; // EffectType

enum class ActiveFilters
{
    none = 0,
    low_pass = 1,
    high_pass = 2,
    band_pass = low_pass | high_pass
}; // ActiveFilters

struct Math
{
    static constexpr auto pi = 3.14159265358979323846F;
    static constexpr auto pi_2 = 1.57079632679489661923F;
    static constexpr auto tau = 6.28318530717958647692F;


    static constexpr float deg_to_rad(
        const float x)
    {
        return x * (pi / 180.0F);
    }

    template<typename T>
    static T clamp(
        const T value,
        const T min_value,
        const T max_value)
    {
        return std::min(max_value, std::max(min_value, value));
    }

    static float lerp(
        const float val1,
        const float val2,
        const float mu)
    {
        return val1 + ((val2 - val1) * mu);
    }
}; // Math

struct Mat4F
{
    using Items = float[4][4];

    Items m_;


    float& operator()(
        const int row_index,
        const int column_index)
    {
        return m_[row_index][column_index];
    }

    const float& operator()(
        const int row_index,
        const int column_index) const
    {
        return m_[row_index][column_index];
    }
}; // Mat4F

constexpr Mat4F mat4f_identity = {{
    {1.0F, 0.0F, 0.0F, 0.0F,},
    {0.0F, 1.0F, 0.0F, 0.0F,},
    {0.0F, 0.0F, 1.0F, 0.0F,},
    {0.0F, 0.0F, 0.0F, 1.0F,},
}};


template<typename T>
constexpr int count_of(const T&)
{
    static_assert(std::rank_v<T> == 1, "Expected an one-dimensional array.");
    return static_cast<int>(std::extent_v<T>);
}


constexpr auto max_channels = 8;

// The maximum number of Ambisonics coefficients. For a given order (o), the
// size needed will be (o+1)**2, thus zero-order has 1, first-order has 4,
// second-order has 9, third-order has 16, and fourth-order has 25.
constexpr auto max_ambi_order = 3;
constexpr auto max_ambi_coeffs = (max_ambi_order + 1) * (max_ambi_order + 1);

// Size for temporary storage of buffer data, in ALfloats. Larger values need
// more memory, while smaller values may need more iterations. The value needs
// to be a sensible size, however, as it constrains the max stepping value used
// for mixing, as well as the maximum number of samples per mixing iteration.
constexpr auto max_sample_buffer_size = 2048;


namespace detail
{


template<typename T, int TExtent1, std::size_t... TExtents>
struct MdArray
{
    using Type = typename std::array<typename MdArray<T, TExtents...>::Type, TExtent1>;
};

template<typename T, std::size_t TExtent>
struct MdArray<T, TExtent>
{
    using Type = std::array<T, TExtent>;
};


} // detail


// Multidimensional std::array.
template<typename T, std::size_t... TExtents>
using MdArray = typename detail::MdArray<T, TExtents...>::Type;


// Find the next power-of-2 for non-power-of-2 numbers.
inline int next_power_of_2(
    int value)
{
    if (value > 0)
    {
        value -= 1;
        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;
    }

    return value + 1;
}

enum class ChannelId
{
    front_left,
    front_right,
    front_center,
    lfe,
    back_left,
    back_right,
    back_center,
    side_left,
    side_right,

    invalid,
}; // ChannelId

// Device formats
enum class ChannelFormat
{
    none,
    mono,
    stereo,
    quad,
    five_point_one,
    five_point_one_rear,
    six_point_one,
    seven_point_one,
}; // ChannelFormat


inline int channel_format_to_channel_count(
    const ChannelFormat channel_format)
{
    switch (channel_format)
    {
    case ChannelFormat::mono:
        return 1;

    case ChannelFormat::stereo:
        return 2;

    case ChannelFormat::quad:
        return 4;

    case ChannelFormat::five_point_one:
    case ChannelFormat::five_point_one_rear:
        return 6;

    case ChannelFormat::six_point_one:
        return 7;

    case ChannelFormat::seven_point_one:
        return 8;

    default:
        return 0;
    }
}

inline ChannelFormat channel_count_to_channel_format(
    const int channel_count)
{
    switch (channel_count)
    {
    case 1:
        return ChannelFormat::mono;

    case 2:
        return ChannelFormat::stereo;

    case 4:
        return ChannelFormat::quad;

    case 6:
        return ChannelFormat::five_point_one;

    case 7:
        return ChannelFormat::six_point_one;

    case 8:
        return ChannelFormat::seven_point_one;

    default:
        return ChannelFormat::none;
    }
}


using ChannelConfig = std::array<float, max_ambi_coeffs>;

struct AmbiConfig
{
    using Coeffs = std::array<ChannelConfig, max_channels>;


    // Ambisonic coefficients for mixing to the dry buffer.
    Coeffs coeffs_;

    void reset()
    {
        for (auto& coeff : coeffs_)
        {
            coeff.fill(0.0F);
        }
    }
}; // AmbiConfig


using SampleBuffer = std::array<float, max_sample_buffer_size>;
using SampleBuffers = std::vector<SampleBuffer>;

using MixerFunc = void (*)(
    const float* data,
    const int channel_count,
    SampleBuffers& dst_buffers,
    float* current_gains,
    const float* target_gains,
    const int counter,
    const int dst_position,
    const int buffer_size);

using RowMixerFunc = void (*)(
    float* dst_buffer,
    const float* gains,
    const SampleBuffers& src_buffers,
    const int channel_count,
    const int src_position,
    const int buffer_size);

struct AmbiOutput
{
    AmbiConfig ambi_;

    // Number of coefficients in each Ambi.Coeffs to mix together (4 for
    // first-order, 9 for second-order, etc). If the count is 0, Ambi.Map
    // is used instead to map each output to a coefficient index.
    //
    // Will only be 4 or 0 (first-order ambisonics output).
    int coeff_count_;
}; // AmbiOutput

struct ChannelPanning
{
    ChannelId name;
    ChannelConfig config;
}; // ChannelPanning

struct Panning
{
    static constexpr ChannelPanning mono_panning[1] = {
        {ChannelId::front_center, {1.0F}},
    };

    static constexpr ChannelPanning stereo_panning[2] = {
        {ChannelId::front_left, {5.00000000E-1F, 2.88675135E-1F, 0.0F, 1.19573156E-1F}},
        {ChannelId::front_right, {5.00000000E-1F, -2.88675135E-1F, 0.0F, 1.19573156E-1F}},
    };

    static constexpr ChannelPanning quad_panning[4] = {
        {ChannelId::back_left, {3.53553391E-1F, 2.04124145E-1F, 0.0F, -2.04124145E-1F}},
        {ChannelId::front_left, {3.53553391E-1F, 2.04124145E-1F, 0.0F, 2.04124145E-1F}},
        {ChannelId::front_right, {3.53553391E-1F, -2.04124145E-1F, 0.0F, 2.04124145E-1F}},
        {ChannelId::back_right, {3.53553391E-1F, -2.04124145E-1F, 0.0F, -2.04124145E-1F}},
    };

    static constexpr ChannelPanning x5_1_side_panning[5] = {
        {ChannelId::side_left, {3.33001372E-1F, 1.89085671E-1F, 0.0F, -2.00041334E-1F, -2.12309737E-2F, 0.0F, 0.0F, 0.0F, -1.14573483E-2F}},
        {ChannelId::front_left, {1.47751298E-1F, 1.28994110E-1F, 0.0F, 1.15190495E-1F, 7.44949143E-2F, 0.0F, 0.0F, 0.0F, -6.47739980E-3F}},
        {ChannelId::front_center, {7.73595729E-2F, 0.00000000E+0F, 0.0F, 9.71390298E-2F, 0.00000000E+0F, 0.0F, 0.0F, 0.0F, 5.18625335E-2F}},
        {ChannelId::front_right, {1.47751298E-1F, -1.28994110E-1F, 0.0F, 1.15190495E-1F, -7.44949143E-2F, 0.0F, 0.0F, 0.0F, -6.47739980E-3F}},
        {ChannelId::side_right, {3.33001372E-1F, -1.89085671E-1F, 0.0F, -2.00041334E-1F, 2.12309737E-2F, 0.0F, 0.0F, 0.0F, -1.14573483E-2F}},
    };

    static constexpr ChannelPanning x5_1_rear_panning[5] = {
        {ChannelId::back_left, {3.33001372E-1F, 1.89085671E-1F, 0.0F, -2.00041334E-1F, -2.12309737E-2F, 0.0F, 0.0F, 0.0F, -1.14573483E-2F}},
        {ChannelId::front_left, {1.47751298E-1F, 1.28994110E-1F, 0.0F, 1.15190495E-1F, 7.44949143E-2F, 0.0F, 0.0F, 0.0F, -6.47739980E-3F}},
        {ChannelId::front_center, {7.73595729E-2F, 0.00000000E+0F, 0.0F, 9.71390298E-2F, 0.00000000E+0F, 0.0F, 0.0F, 0.0F, 5.18625335E-2F}},
        {ChannelId::front_right, {1.47751298E-1F, -1.28994110E-1F, 0.0F, 1.15190495E-1F, -7.44949143E-2F, 0.0F, 0.0F, 0.0F, -6.47739980E-3F}},
        {ChannelId::back_right, {3.33001372E-1F, -1.89085671E-1F, 0.0F, -2.00041334E-1F, 2.12309737E-2F, 0.0F, 0.0F, 0.0F, -1.14573483E-2F}},
    };

    static constexpr ChannelPanning x6_1_panning[6] = {
        {ChannelId::side_left, {2.04462744E-1F, 2.17178497E-1F, 0.0F, -4.39990188E-2F, -2.60787329E-2F, 0.0F, 0.0F, 0.0F, -6.87238843E-2F}},
        {ChannelId::front_left, {1.18130342E-1F, 9.34633906E-2F, 0.0F, 1.08553749E-1F, 6.80658795E-2F, 0.0F, 0.0F, 0.0F, 1.08999485E-2F}},
        {ChannelId::front_center, {7.73595729E-2F, 0.00000000E+0F, 0.0F, 9.71390298E-2F, 0.00000000E+0F, 0.0F, 0.0F, 0.0F, 5.18625335E-2F}},
        {ChannelId::front_right, {1.18130342E-1F, -9.34633906E-2F, 0.0F, 1.08553749E-1F, -6.80658795E-2F, 0.0F, 0.0F, 0.0F, 1.08999485E-2F}},
        {ChannelId::side_right, {2.04462744E-1F, -2.17178497E-1F, 0.0F, -4.39990188E-2F, 2.60787329E-2F, 0.0F, 0.0F, 0.0F, -6.87238843E-2F}},
        {ChannelId::back_center, {2.50001688E-1F, 0.00000000E+0F, 0.0F, -2.50000094E-1F, 0.00000000E+0F, 0.0F, 0.0F, 0.0F, 6.05133395E-2F}},
    };

    static constexpr ChannelPanning x7_1_panning[6] = {
        {ChannelId::back_left, {2.04124145E-1F, 1.08880247E-1F, 0.0F, -1.88586120E-1F, -1.29099444E-1F, 0.0F, 0.0F, 0.0F, 7.45355993E-2F, 3.73460789E-2F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.00000000E+0F}},
        {ChannelId::side_left, {2.04124145E-1F, 2.17760495E-1F, 0.0F, 0.00000000E+0F, 0.00000000E+0F, 0.0F, 0.0F, 0.0F, -1.49071198E-1F, -3.73460789E-2F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.00000000E+0F}},
        {ChannelId::front_left, {2.04124145E-1F, 1.08880247E-1F, 0.0F, 1.88586120E-1F, 1.29099444E-1F, 0.0F, 0.0F, 0.0F, 7.45355993E-2F, 3.73460789E-2F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.00000000E+0F}},
        {ChannelId::front_right, {2.04124145E-1F, -1.08880247E-1F, 0.0F, 1.88586120E-1F, -1.29099444E-1F, 0.0F, 0.0F, 0.0F, 7.45355993E-2F, -3.73460789E-2F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.00000000E+0F}},
        {ChannelId::side_right, {2.04124145E-1F, -2.17760495E-1F, 0.0F, 0.00000000E+0F, 0.00000000E+0F, 0.0F, 0.0F, 0.0F, -1.49071198E-1F, 3.73460789E-2F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.00000000E+0F}},
        {ChannelId::back_right, {2.04124145E-1F, -1.08880247E-1F, 0.0F, -1.88586120E-1F, 1.29099444E-1F, 0.0F, 0.0F, 0.0F, 7.45355993E-2F, -3.73460789E-2F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.00000000E+0F}},
    };


    // Calculates ambisonic coefficients based on a direction vector. The vector
    // must be normalized (unit length), and the spread is the angular width of the
    // sound (0...tau).
    static void calc_direction_coeffs(
        const float dir[3],
        const float spread,
        float coeffs[max_ambi_coeffs])
    {
        // Convert from OpenAL coords to Ambisonics.
        const auto x = -dir[2];
        const auto y = -dir[0];
        const auto z = dir[1];

        // Zeroth-order
        coeffs[0] = 1.0F; // ACN 0 = 1

        // First-order
        coeffs[1] = 1.732050808F * y; // ACN 1 = sqrt(3) * Y
        coeffs[2] = 1.732050808F * z; // ACN 2 = sqrt(3) * Z
        coeffs[3] = 1.732050808F * x; // ACN 3 = sqrt(3) * X

        // Second-order
        coeffs[4] = 3.872983346F * x * y; // ACN 4 = sqrt(15) * X * Y
        coeffs[5] = 3.872983346F * y * z; // ACN 5 = sqrt(15) * Y * Z
        coeffs[6] = 1.118033989F * ((3.0F * z * z) - 1.0F); // ACN 6 = sqrt(5)/2 * (3*Z*Z - 1)
        coeffs[7] = 3.872983346F * x * z; // ACN 7 = sqrt(15) * X * Z
        coeffs[8] = 1.936491673F * ((x * x) - (y * y)); // ACN 8 = sqrt(15)/2 * (X*X - Y*Y)

        // Third-order
        coeffs[9] = 2.091650066F * y * ((3.0F * x * x) - (y * y)); // ACN  9 = sqrt(35/8) * Y * (3*X*X - Y*Y)
        coeffs[10] = 10.246950766F * z * x * y; // ACN 10 = sqrt(105) * Z * X * Y
        coeffs[11] = 1.620185175F * y * ((5.0F * z * z) - 1.0F); // ACN 11 = sqrt(21/8) * Y * (5*Z*Z - 1)
        coeffs[12] = 1.322875656F * z * ((5.0F * z * z) - 3.0F); // ACN 12 = sqrt(7)/2 * Z * (5*Z*Z - 3)
        coeffs[13] = 1.620185175F * x * ((5.0F * z * z) - 1.0F); // ACN 13 = sqrt(21/8) * X * (5*Z*Z - 1)
        coeffs[14] = 5.123475383F * z * ((x * x) - (y * y)); // ACN 14 = sqrt(105)/2 * Z * (X*X - Y*Y)
        coeffs[15] = 2.091650066F * x * ((x * x) - (3.0F * y * y)); // ACN 15 = sqrt(35/8) * X * (X*X - 3*Y*Y)

        if (spread > 0.0F)
        {
            // Implement the spread by using a spherical source that subtends the
            // angle spread. See:
            // http://www.ppsloan.org/publications/StupidSH36.pdf - Appendix A3
            //
            // When adjusted for N3D normalization instead of SN3D, these
            // calculations are:
            //
            // ZH0 = -sqrt(pi) * (-1+ca);
            // ZH1 =  0.5*sqrt(pi) * sa*sa;
            // ZH2 = -0.5*sqrt(pi) * ca*(-1+ca)*(ca+1);
            // ZH3 = -0.125*sqrt(pi) * (-1+ca)*(ca+1)*(5*ca*ca - 1);
            // ZH4 = -0.125*sqrt(pi) * ca*(-1+ca)*(ca+1)*(7*ca*ca - 3);
            // ZH5 = -0.0625*sqrt(pi) * (-1+ca)*(ca+1)*(21*ca*ca*ca*ca - 14*ca*ca + 1);
            //
            // The gain of the source is compensated for size, so that the
            // loundness doesn't depend on the spread. Thus:
            //
            // ZH0 = 1.0f;
            // ZH1 = 0.5f * (ca+1.0f);
            // ZH2 = 0.5f * (ca+1.0f)*ca;
            // ZH3 = 0.125f * (ca+1.0f)*(5.0f*ca*ca - 1.0f);
            // ZH4 = 0.125f * (ca+1.0f)*(7.0f*ca*ca - 3.0f)*ca;
            // ZH5 = 0.0625f * (ca+1.0f)*(21.0f*ca*ca*ca*ca - 14.0f*ca*ca + 1.0f);

            const auto ca = std::cos(spread * 0.5F);

            // Increase the source volume by up to +3dB for a full spread.
            const auto scale = std::sqrt(1.0F + (spread / Math::tau));

            const auto zh0_norm = scale;
            const auto zh1_norm = 0.5F * (ca + 1.0F) * scale;
            const auto zh2_norm = 0.5F * (ca + 1.0F) * ca * scale;
            const auto zh3_norm = 0.125F * (ca + 1.0F) * ((5.0F * ca * ca) - 1.0F) * scale;

            // Zeroth-order
            coeffs[0] *= zh0_norm;

            // First-order
            coeffs[1] *= zh1_norm;
            coeffs[2] *= zh1_norm;
            coeffs[3] *= zh1_norm;

            // Second-order
            coeffs[4] *= zh2_norm;
            coeffs[5] *= zh2_norm;
            coeffs[6] *= zh2_norm;
            coeffs[7] *= zh2_norm;
            coeffs[8] *= zh2_norm;

            // Third-order
            coeffs[9] *= zh3_norm;
            coeffs[10] *= zh3_norm;
            coeffs[11] *= zh3_norm;
            coeffs[12] *= zh3_norm;
            coeffs[13] *= zh3_norm;
            coeffs[14] *= zh3_norm;
            coeffs[15] *= zh3_norm;
        }
    }


    // Calculates ambisonic coefficients based on azimuth and elevation. The
    // azimuth and elevation parameters are in radians, going right and up
    // respectively.
    static void calc_angle_coeffs(
        const float azimuth,
        const float elevation,
        const float spread,
        float coeffs[max_ambi_coeffs])
    {
        float dir[3] = {
            std::sin(azimuth) * std::cos(elevation),
            std::sin(elevation),
            -std::cos(azimuth) * std::cos(elevation)
        };

        calc_direction_coeffs(dir, spread, coeffs);
    }

    // Computes channel gains for ambient, omni-directional sounds.
    static void compute_ambient_gains(
        const int channel_count,
        const AmbiOutput& amb_output,
        const float in_gain,
        float* const out_gains)
    {
        if (amb_output.coeff_count_ > 0)
        {
            compute_ambient_gains_mc(amb_output.ambi_.coeffs_.data(), channel_count, in_gain, out_gains);
        }
        else
        {
            compute_ambient_gains_bf(channel_count, in_gain, out_gains);
        }
    }

    static void compute_ambient_gains_mc(
        const ChannelConfig* channel_coeffs,
        const int num_channels,
        const float in_gain,
        float gains[max_channels])
    {
        for (int i = 0; i < max_channels; ++i)
        {
            if (i < num_channels)
            {
                gains[i] = channel_coeffs[i][0] * 1.414213562F * in_gain;
            }
            else
            {
                gains[i] = 0.0F;
            }
        }
    }

    static void compute_ambient_gains_bf(
        const int num_channels,
        const float in_gain,
        float gains[max_channels])
    {
        auto gain = 0.0F;

        for (int i = 0; i < num_channels; ++i)
        {
            if (i == 0)
            {
                gain += 1.0F;
            }
        }

        gains[0] = gain * 1.414213562F * in_gain;

        for (int i = 1; i < max_channels; i++)
        {
            gains[i] = 0.0F;
        }
    }

    // Computes panning gains using the given channel decoder coefficients and the
    // pre-calculated direction or angle coefficients.
    static void compute_panning_gains(
        const int channel_count,
        const AmbiOutput& amb_output,
        const float* const coeffs,
        const float in_gain,
        float* const out_gains)
    {
        if (amb_output.coeff_count_ > 0)
        {
            compute_panning_gains_mc(
                amb_output.ambi_.coeffs_.data(),
                channel_count,
                amb_output.coeff_count_,
                coeffs,
                in_gain,
                out_gains);
        }
        else
        {
            compute_panning_gains_bf(
                channel_count,
                coeffs,
                in_gain,
                out_gains);
        }
    }

    static void compute_panning_gains_mc(
        const ChannelConfig* channel_coeffs,
        const int num_channels,
        const int num_coeffs,
        const float coeffs[max_ambi_coeffs],
        const float in_gain,
        float gains[max_channels])
    {
        for (int i = 0; i < max_channels; ++i)
        {
            if (i < num_channels)
            {
                auto gain = 0.0F;

                for (int j = 0; j < num_coeffs; ++j)
                {
                    gain += channel_coeffs[i][j] * coeffs[j];
                }

                gains[i] = Math::clamp(gain, 0.0F, 1.0F) * in_gain;
            }
            else
            {
                gains[i] = 0.0F;
            }
        }
    }

    static void compute_panning_gains_bf(
        const int num_channels,
        const float coeffs[max_ambi_coeffs],
        const float in_gain,
        float gains[max_channels])
    {
        for (int i = 0; i < max_channels; ++i)
        {
            if (i < num_channels)
            {
                gains[i] = coeffs[i] * in_gain;
            }
            else
            {
                gains[i] = 0.0F;
            }
        }
    }

    // Sets channel gains for a first-order ambisonics input channel. The matrix is
    // a 1x4 'slice' of a transform matrix for the input channel, used to scale and
    // orient the sound samples.
    static void compute_first_order_gains(
        const int channel_count,
        const AmbiOutput& amb_output,
        const float* const matrix,
        const float in_gain,
        float* const out_gains)
    {
        if (amb_output.coeff_count_ > 0)
        {
            compute_first_order_gains_mc(amb_output.ambi_.coeffs_.data(), channel_count, matrix, in_gain, out_gains);
        }
        else
        {
            compute_first_order_gains_bf(channel_count, matrix, in_gain, out_gains);
        }
    }

    static void compute_first_order_gains_mc(
        const ChannelConfig* channel_coeffs,
        int num_channels,
        const float mtx[4],
        float in_gain,
        float gains[max_channels])
    {
        for (int i = 0; i < num_channels; ++i)
        {
            if (i < num_channels)
            {
                auto gain = 0.0F;

                for (int j = 0; j < 4; ++j)
                {
                    gain += channel_coeffs[i][j] * mtx[j];
                }

                gains[i] = Math::clamp(gain, 0.0F, 1.0F) * in_gain;
            }
            else
            {
                gains[i] = 0.0F;
            }
        }
    }

    static void compute_first_order_gains_bf(
        const int num_channels,
        const float mtx[4],
        const float in_gain,
        float gains[max_channels])
    {
        for (int i = 0; i < max_channels; ++i)
        {
            if (i < num_channels)
            {
                gains[i] = mtx[i] * in_gain;
            }
            else
            {
                gains[i] = 0.0F;
            }
        }
    }

    static void set_channel_map(
        const ChannelId* device_channels,
        ChannelConfig* ambi_coeffs,
        const ChannelPanning* channel_panning,
        const int count,
        int* out_count)
    {
        int i;

        for (i = 0; i < max_channels && device_channels[i] != ChannelId::invalid; ++i)
        {
            if (device_channels[i] == ChannelId::lfe)
            {
                for (int j = 0; j < max_ambi_coeffs; ++j)
                {
                    ambi_coeffs[i][j] = 0.0F;
                }

                continue;
            }

            for (int j = 0; j < count; ++j)
            {
                if (device_channels[i] != channel_panning[j].name)
                {
                    continue;
                }

                for (int k = 0; k < max_ambi_coeffs; ++k)
                {
                    ambi_coeffs[i][k] = channel_panning[j].config[k];
                }

                break;
            }
        }

        *out_count = i;
    }
}; // Panning

// Filters implementation is based on the "Cookbook formulae for audio
// EQ biquad filter coefficients" by Robert Bristow-Johnson
// http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
//
// Implementation note: For the shelf filters, the specified gain is for the
// reference frequency, which is the centerpoint of the transition band. This
// better matches EFX filter design. To set the gain for the shelf itself, use
// the square root of the desired linear gain (or halve the dB gain).

enum class FilterType
{
    // EFX-style low-pass filter, specifying a gain and reference frequency.
    high_shelf,

    // EFX-style high-pass filter, specifying a gain and reference frequency.
    low_shelf,

    // Peaking filter, specifying a gain and reference frequency.
    peaking,

    // Low-pass cut-off filter, specifying a cut-off frequency.
    low_pass,

    // High-pass cut-off filter, specifying a cut-off frequency.
    high_pass,

    // Band-pass filter, specifying a center frequency.
    band_pass,
}; // FilterType


struct FilterState
{
    static constexpr auto lp_frequency_reference = 5000.0F;
    static constexpr auto hp_frequency_reference = 250.0F;


    float x_[2]; // History of two last input samples
    float y_[2]; // History of two last output samples

    // Transfer function coefficients "b"
    float b0_;
    float b1_;
    float b2_;

    // Transfer function coefficients "a" (a0 is pre-applied)
    float a1_;
    float a2_;


    void reset()
    {
        x_[0] = 0.0F;
        x_[1] = 0.0F;

        y_[0] = 0.0F;
        y_[1] = 0.0F;

        b0_ = 0.0F;
        b1_ = 0.0F;
        b2_ = 0.0F;

        a1_ = 0.0F;
        a2_ = 0.0F;
    }

    void clear()
    {
        x_[0] = 0.0F;
        x_[1] = 0.0F;
        y_[0] = 0.0F;
        y_[1] = 0.0F;
    }

    void set_params(
        const FilterType type,
        const float gain,
        const float freq_mult,
        const float rcp_q)
    {
        // Limit gain to -100dB
        assert(gain > 0.00001F);


        const auto w0 = Math::tau * freq_mult;
        const auto sin_w0 = std::sin(w0);
        const auto cos_w0 = std::cos(w0);
        const auto alpha = sin_w0 / 2.0F * rcp_q;

        auto sqrt_gain_alpha_2 = 0.0F;

        float a[3];
        float b[3];


        // Calculate filter coefficients depending on filter type
        switch (type)
        {
        case FilterType::high_shelf:
            sqrt_gain_alpha_2 = 2.0F * std::sqrt(gain) * alpha;

            b[0] = gain * ((gain + 1.0F) + ((gain - 1.0F) * cos_w0) + sqrt_gain_alpha_2);
            b[1] = -2.0F * gain * ((gain - 1.0F) + ((gain + 1.0F) * cos_w0));
            b[2] = gain * ((gain + 1.0F) + ((gain - 1.0F) * cos_w0) - sqrt_gain_alpha_2);

            a[0] = (gain + 1.0F) - ((gain - 1.0F) * cos_w0) + sqrt_gain_alpha_2;
            a[1] = 2.0F * ((gain - 1.0F) - ((gain + 1.0F) * cos_w0));
            a[2] = (gain + 1.0F) - ((gain - 1.0F) * cos_w0) - sqrt_gain_alpha_2;

            break;

        case FilterType::low_shelf:
            sqrt_gain_alpha_2 = 2.0F * std::sqrt(gain) * alpha;

            b[0] = gain * ((gain + 1.0F) - ((gain - 1.0F) * cos_w0) + sqrt_gain_alpha_2);
            b[1] = 2.0F * gain * ((gain - 1.0F) - ((gain + 1.0F) * cos_w0));
            b[2] = gain * ((gain + 1.0F) - ((gain - 1.0F) * cos_w0) - sqrt_gain_alpha_2);

            a[0] = (gain + 1.0F) + ((gain - 1.0F) * cos_w0) + sqrt_gain_alpha_2;
            a[1] = -2.0F * ((gain - 1.0F) + ((gain + 1.0F) * cos_w0));
            a[2] = (gain + 1.0F) + ((gain - 1.0F) * cos_w0) - sqrt_gain_alpha_2;

            break;

        case FilterType::peaking:
        {
            const auto sqrt_gain = std::sqrt(gain);

            b[0] = 1.0F + (alpha * sqrt_gain);
            b[1] = -2.0F * cos_w0;
            b[2] = 1.0F - (alpha * sqrt_gain);

            a[0] = 1.0F + (alpha / sqrt_gain);
            a[1] = -2.0F * cos_w0;
            a[2] = 1.0F - (alpha / sqrt_gain);

            break;
        }

        case FilterType::low_pass:
            b[0] = (1.0F - cos_w0) / 2.0F;
            b[1] = 1.0F - cos_w0;
            b[2] = (1.0F - cos_w0) / 2.0F;

            a[0] = 1.0F + alpha;
            a[1] = -2.0F * cos_w0;
            a[2] = 1.0F - alpha;

            break;

        case FilterType::high_pass:
            b[0] = (1.0F + cos_w0) / 2.0F;
            b[1] = -(1.0F + cos_w0);
            b[2] = (1.0F + cos_w0) / 2.0F;

            a[0] = 1.0F + alpha;
            a[1] = -2.0F * cos_w0;
            a[2] = 1.0F - alpha;

            break;

        case FilterType::band_pass:
            b[0] = alpha;
            b[1] = 0;
            b[2] = -alpha;

            a[0] = 1.0F + alpha;
            a[1] = -2.0F * cos_w0;
            a[2] = 1.0F - alpha;

            break;

        default:
            b[0] = 1.0F;
            b[1] = 0.0F;
            b[2] = 0.0F;

            a[0] = 1.0F;
            a[1] = 0.0F;
            a[2] = 0.0F;

            break;
        }

        a1_ = a[1] / a[0];
        a2_ = a[2] / a[0];
        b0_ = b[0] / a[0];
        b1_ = b[1] / a[0];
        b2_ = b[2] / a[0];
    }

    void process(
        const int sample_count,
        const float* src_samples,
        float* dst_samples)
    {
        if (sample_count > 1)
        {
            dst_samples[0] =
                (b0_ * src_samples[0]) +
                (b1_ * x_[0]) +
                (b2_ * x_[1]) -
                (a1_ * y_[0]) -
                (a2_ * y_[1]);

            dst_samples[1] =
                (b0_ * src_samples[1]) +
                (b1_ * src_samples[0]) +
                (b2_ * x_[0]) -
                (a1_ * dst_samples[0]) -
                (a2_ * y_[0]);

            auto i = 0;

            for (i = 2; i < sample_count; ++i)
            {
                dst_samples[i] =
                    (b0_ * src_samples[i]) +
                    (b1_ * src_samples[i - 1]) +
                    (b2_ * src_samples[i - 2]) -
                    (a1_ * dst_samples[i - 1]) -
                    (a2_ * dst_samples[i - 2]);
            }

            x_[0] = src_samples[i - 1];
            x_[1] = src_samples[i - 2];
            y_[0] = dst_samples[i - 1];
            y_[1] = dst_samples[i - 2];
        }
        else if (sample_count == 1)
        {
            dst_samples[0] =
                (b0_ * src_samples[0]) +
                (b1_ * x_[0]) +
                (b2_ * x_[1]) -
                (a1_ * y_[0]) -
                (a2_ * y_[1]);

            x_[1] = x_[0];
            x_[0] = src_samples[0];
            y_[1] = y_[0];
            y_[0] = dst_samples[0];
        }
    }

    void process_pass_through(
        const int sample_count,
        const float* src_samples)
    {
        if (sample_count >= 2)
        {
            x_[1] = src_samples[sample_count - 2];
            x_[0] = src_samples[sample_count - 1];
            y_[1] = src_samples[sample_count - 2];
            y_[0] = src_samples[sample_count - 1];
        }
        else if (sample_count == 1)
        {
            x_[1] = x_[0];
            x_[0] = src_samples[0];
            y_[1] = y_[0];
            y_[0] = src_samples[0];
        }
    }


    static void copy_params(
        const FilterState& src_state,
        FilterState& dst_state)
    {
        dst_state.b0_ = src_state.b0_;
        dst_state.b1_ = src_state.b1_;
        dst_state.b2_ = src_state.b2_;
        dst_state.a1_ = src_state.a1_;
        dst_state.a2_ = src_state.a2_;
    }

    // Calculates the rcpQ (i.e. 1/Q) coefficient for shelving filters, using the
    // reference gain and shelf slope parameter.
    // 0 < gain
    // 0 < slope <= 1
    static float calc_rcp_q_from_slope(
        const float gain,
        const float slope)
    {
        return std::sqrt((gain + (1.0F / gain)) * ((1.0F / slope) - 1.0F) + 2.0F);
    }

    // Calculates the rcpQ (i.e. 1/Q) coefficient for filters, using the frequency
    // multiple (i.e. ref_freq / sampling_freq) and bandwidth.
    // 0 < freq_mult < 0.5.
    static float calc_rcp_q_from_bandwidth(
        const float freq_mult,
        const float bandwidth)
    {
        const auto w0 = Math::tau * freq_mult;
        return 2.0F * std::sinh(std::log(2.0F) / 2.0F * bandwidth * w0 / std::sin(w0));
    }
}; // FilterState

struct ALCdevice
{
    using ChannelNames = std::array<ChannelId, max_channels>;


    int frequency_;
    int update_size_;
    ChannelFormat channel_format_;

    int channel_count_;
    ChannelNames channel_names_;
    SampleBuffers sample_buffers_;

    // Temp storage used for each source when mixing.
    SampleBuffer resampled_data_;
    SampleBuffer filtered_data_;

    // The "dry" path corresponds to the main output.
    AmbiOutput dry_;

    // First-order ambisonics output, to be upsampled to the dry buffer if different.
    AmbiOutput foa_;

    const float* source_samples_;


    void initialize(
        const ChannelFormat channel_format,
        const int sampling_rate)
    {
        channel_count_ = channel_format_to_channel_count(channel_format);

        // Set output format
        channel_format_ = channel_format;
        frequency_ = sampling_rate;
        update_size_ = 1024;

        alu_init_renderer(this);

        sample_buffers_.clear();
        sample_buffers_.resize(channel_count_);
    }

    void uninitialize()
    {
    }

    void set_default_wfx_channel_order()
    {
        channel_names_.fill(ChannelId::invalid);

        switch (channel_format_)
        {
        case ChannelFormat::mono:
            channel_names_[0] = ChannelId::front_center;
            break;

        case ChannelFormat::stereo:
            channel_names_[0] = ChannelId::front_left;
            channel_names_[1] = ChannelId::front_right;
            break;

        case ChannelFormat::quad:
            channel_names_[0] = ChannelId::front_left;
            channel_names_[1] = ChannelId::front_right;
            channel_names_[2] = ChannelId::back_left;
            channel_names_[3] = ChannelId::back_right;
            break;

        case ChannelFormat::five_point_one:
            channel_names_[0] = ChannelId::front_left;
            channel_names_[1] = ChannelId::front_right;
            channel_names_[2] = ChannelId::front_center;
            channel_names_[3] = ChannelId::lfe;
            channel_names_[4] = ChannelId::side_left;
            channel_names_[5] = ChannelId::side_right;
            break;

        case ChannelFormat::five_point_one_rear:
            channel_names_[0] = ChannelId::front_left;
            channel_names_[1] = ChannelId::front_right;
            channel_names_[2] = ChannelId::front_center;
            channel_names_[3] = ChannelId::lfe;
            channel_names_[4] = ChannelId::back_left;
            channel_names_[5] = ChannelId::back_right;
            break;

        case ChannelFormat::six_point_one:
            channel_names_[0] = ChannelId::front_left;
            channel_names_[1] = ChannelId::front_right;
            channel_names_[2] = ChannelId::front_center;
            channel_names_[3] = ChannelId::lfe;
            channel_names_[4] = ChannelId::back_center;
            channel_names_[5] = ChannelId::side_left;
            channel_names_[6] = ChannelId::side_right;
            break;

        case ChannelFormat::seven_point_one:
            channel_names_[0] = ChannelId::front_left;
            channel_names_[1] = ChannelId::front_right;
            channel_names_[2] = ChannelId::front_center;
            channel_names_[3] = ChannelId::lfe;
            channel_names_[4] = ChannelId::back_left;
            channel_names_[5] = ChannelId::back_right;
            channel_names_[6] = ChannelId::side_left;
            channel_names_[7] = ChannelId::side_right;
            break;
        }
    }
}; // ALCdevice


extern ALCdevice* g_device;
extern struct ALsource* g_source;
extern struct Effect* g_effect;
extern struct EffectSlot* g_effect_slot;


// Returns the index for the given channel name (e.g. FrontCenter), or -1 if it
// doesn't exist.
inline int get_channel_index(
    const ALCdevice::ChannelNames& names,
    const ChannelId chan)
{
    auto i = 0;

    for (const auto& name : names)
    {
        if(name == chan)
        {
            return i;
        }

        i += 1;
    }

    return -1;
}


#endif
