#ifndef AL_MAIN_H
#define AL_MAIN_H


#include <cassert>
#include <cmath>

#include <array>
#include <memory>
#include <type_traits>
#include <vector>


struct ALCdevice;
struct ALsource;
struct Effect;
struct EffectSlot;


constexpr auto max_effect_channels = 4;

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

    template<typename T>
    static void clamp_i(
        T& value,
        const T min_value,
        const T max_value)
    {
        value = clamp(value, min_value, max_value);
    }

    static float lerp(
        const float val1,
        const float val2,
        const float mu)
    {
        return val1 + ((val2 - val1) * mu);
    }

    // Find the next power-of-2 for non-power-of-2 numbers.
    static int next_power_of_2(
        const int value)
    {
        auto new_value = value;

        if (new_value > 0)
        {
            new_value -= 1;
            new_value |= new_value >> 1;
            new_value |= new_value >> 2;
            new_value |= new_value >> 4;
            new_value |= new_value >> 8;
            new_value |= new_value >> 16;
        }

        return new_value + 1;
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
constexpr int get_array_extents(
    const T&)
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

using AmbiCoeffs = std::array<float, max_ambi_coeffs>;
using Gains = std::array<float, max_channels>;


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
        AmbiCoeffs& coeffs)
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
        AmbiCoeffs& coeffs)
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
        Gains& out_gains)
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
        const int channel_count,
        const float src_gain,
        Gains& dst_gains)
    {
        for (int i = 0; i < max_channels; ++i)
        {
            dst_gains[i] = (i < channel_count ? channel_coeffs[i][0] * 1.414213562F * src_gain : 0.0F);
        }
    }

    static void compute_ambient_gains_bf(
        const int channel_count,
        const float src_gain,
        Gains& dst_gains)
    {
        for (int i = 0; i < max_channels; ++i)
        {
            dst_gains[i] = (i == 0 ? 1.414213562F * src_gain : 0.0F);
        }
    }

    // Computes panning gains using the given channel decoder coefficients and the
    // pre-calculated direction or angle coefficients.
    static void compute_panning_gains(
        const int channel_count,
        const AmbiOutput& amb_output,
        const AmbiCoeffs& coeffs,
        const float in_gain,
        Gains& out_gains)
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
        const int channel_count,
        const int coeff_count,
        const AmbiCoeffs& coeffs,
        const float src_gain,
        Gains& dst_gains)
    {
        for (int i = 0; i < max_channels; ++i)
        {
            if (i < channel_count)
            {
                auto gain = 0.0F;

                for (int j = 0; j < coeff_count; ++j)
                {
                    gain += channel_coeffs[i][j] * coeffs[j];
                }

                dst_gains[i] = Math::clamp(gain, 0.0F, 1.0F) * src_gain;
            }
            else
            {
                dst_gains[i] = 0.0F;
            }
        }
    }

    static void compute_panning_gains_bf(
        const int channel_count,
        const AmbiCoeffs& coeffs,
        const float src_gain,
        Gains& dst_gains)
    {
        for (int i = 0; i < max_channels; ++i)
        {
            dst_gains[i] = (i < channel_count ? coeffs[i] * src_gain : 0.0F);
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
        Gains& out_gains)
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
        int channel_count,
        const float matrix[4],
        float src_gain,
        Gains& dst_gains)
    {
        for (int i = 0; i < max_channels; ++i)
        {
            if (i < channel_count)
            {
                auto gain = 0.0F;

                for (int j = 0; j < 4; ++j)
                {
                    gain += channel_coeffs[i][j] * matrix[j];
                }

                dst_gains[i] = Math::clamp(gain, 0.0F, 1.0F) * src_gain;
            }
            else
            {
                dst_gains[i] = 0.0F;
            }
        }
    }

    static void compute_first_order_gains_bf(
        const int channel_count,
        const float matrix[4],
        const float src_gain,
        Gains& dst_gains)
    {
        for (int i = 0; i < max_channels; ++i)
        {
            dst_gains[i] = (i < channel_count ? matrix[i] * src_gain : 0.0F);
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

struct ALsource
{
    struct Send
    {
        struct Channel
        {
            FilterState low_pass_;
            FilterState high_pass_;
            Gains current_gains_;
            Gains target_gains_;


            void reset()
            {
                low_pass_.reset();
                high_pass_.reset();

                current_gains_.fill(0.0F);
                target_gains_.fill(0.0F);
            }
        }; // Channel

        using Channels = std::array<Channel, max_channels>;


        float gain_;
        float gain_hf_;
        float hf_reference_;
        float gain_lf_;
        float lf_reference_;

        ActiveFilters filter_type_;
        Channels channels_;
        SampleBuffers* buffers_;
        int channel_count_;
    }; // Send


    Send direct_;
    Send aux_;


    ALsource()
    {
        initialize();
    }

    void initialize()
    {
        direct_.gain_ = 1.0F;
        direct_.gain_hf_ = 1.0F;
        direct_.hf_reference_ = FilterState::lp_frequency_reference;
        direct_.gain_lf_ = 1.0F;
        direct_.lf_reference_ = FilterState::hp_frequency_reference;
        aux_.gain_ = 1.0F;
        aux_.gain_hf_ = 1.0F;
        aux_.hf_reference_ = FilterState::lp_frequency_reference;
        aux_.gain_lf_ = 1.0F;
        aux_.lf_reference_ = FilterState::hp_frequency_reference;
    }
}; // ALsource

using EffectSampleBuffer = std::vector<float>;


union EffectProps
{
    using Pan = std::array<float, 3>;


    struct Chorus
    {
        static constexpr auto waveform_sinusoid = 0;
        static constexpr auto waveform_triangle = 1;

        static constexpr auto min_waveform = waveform_sinusoid;
        static constexpr auto max_waveform = waveform_triangle;
        static constexpr auto default_waveform = waveform_triangle;

        static constexpr auto min_phase = -180;
        static constexpr auto max_phase = 180;
        static constexpr auto default_phase = 90;

        static constexpr auto min_rate = 0.0F;
        static constexpr auto max_rate = 10.0F;
        static constexpr auto default_rate = 1.1F;

        static constexpr auto min_depth = 0.0F;
        static constexpr auto max_depth = 1.0F;
        static constexpr auto default_depth = 0.1F;

        static constexpr auto min_feedback = -1.0F;
        static constexpr auto max_feedback = 1.0F;
        static constexpr auto default_feedback = 0.25F;

        static constexpr auto min_delay = 0.0F;
        static constexpr auto max_delay = 0.016F;
        static constexpr auto default_delay = 0.016F;


        int waveform_;
        int phase_;
        float rate_;
        float depth_;
        float feedback_;
        float delay_;


        void set_defaults()
        {
            waveform_ = default_waveform;
            phase_ = default_phase;
            rate_ = default_rate;
            depth_ = default_depth;
            feedback_ = default_feedback;
            delay_ = default_delay;
        }

        void normalize()
        {
            Math::clamp_i(waveform_, min_waveform, max_waveform);
            Math::clamp_i(phase_, min_phase, max_phase);
            Math::clamp_i(rate_, min_rate, max_rate);
            Math::clamp_i(depth_, min_depth, max_depth);
            Math::clamp_i(feedback_, min_feedback, max_feedback);
            Math::clamp_i(delay_, min_delay, max_delay);
        }
    }; // Chorus

    struct Compressor
    {
        static constexpr auto min_on_off = false;
        static constexpr auto max_on_off = true;
        static constexpr auto default_on_off = true;


        bool on_off_;


        void set_defaults()
        {
            on_off_ = default_on_off;
        }

        void normalize()
        {
        }
    }; // Compressor

    struct Dedicated
    {
        static constexpr auto min_gain = 0.0F;
        static constexpr auto max_gain = 1.0F;
        static constexpr auto default_gain = 1.0F;


        float gain_;


        void set_defaults()
        {
            gain_ = default_gain;
        }

        void normalize()
        {
            Math::clamp_i(gain_, EffectProps::Dedicated::min_gain, EffectProps::Dedicated::max_gain);
        }
    }; // Dedicated

    struct Distortion
    {
        static constexpr auto min_edge = 0.0F;
        static constexpr auto max_edge = 1.0F;
        static constexpr auto default_edge = 0.2F;

        static constexpr auto min_gain = 0.01F;
        static constexpr auto max_gain = 1.0F;
        static constexpr auto default_gain = 0.05F;

        static constexpr auto min_low_pass_cutoff = 80.0F;
        static constexpr auto max_low_pass_cutoff = 24000.0F;
        static constexpr auto default_low_pass_cutoff = 8000.0F;

        static constexpr auto min_eq_center = 80.0F;
        static constexpr auto max_eq_center = 24000.0F;
        static constexpr auto default_eq_center = 3600.0F;

        static constexpr auto min_eq_bandwidth = 80.0F;
        static constexpr auto max_eq_bandwidth = 24000.0F;
        static constexpr auto default_eq_bandwidth = 3600.0F;


        float edge_;
        float gain_;
        float low_pass_cutoff_;
        float eq_center_;
        float eq_bandwidth_;


        void set_defaults()
        {
            edge_ = default_edge;
            gain_ = default_gain;
            low_pass_cutoff_ = default_low_pass_cutoff;
            eq_center_ = default_eq_center;
            eq_bandwidth_ = default_eq_bandwidth;
        }

        void normalize()
        {
            Math::clamp_i(edge_, min_edge, max_edge);
            Math::clamp_i(gain_, min_gain, max_gain);
            Math::clamp_i(low_pass_cutoff_, min_low_pass_cutoff, max_low_pass_cutoff);
            Math::clamp_i(eq_center_, min_eq_center, max_eq_center);
            Math::clamp_i(eq_bandwidth_, min_eq_bandwidth, max_eq_bandwidth);
        }
    }; // Distortion

    struct Echo
    {
        static constexpr auto min_delay = 0.0F;
        static constexpr auto max_delay = 0.207F;
        static constexpr auto default_delay = 0.1F;

        static constexpr auto min_lr_delay = 0.0F;
        static constexpr auto max_lr_delay = 0.404F;
        static constexpr auto default_lr_delay = 0.1F;

        static constexpr auto min_damping = 0.0F;
        static constexpr auto max_damping = 0.99F;
        static constexpr auto default_damping = 0.5F;

        static constexpr auto min_feedback = 0.0F;
        static constexpr auto max_feedback = 1.0F;
        static constexpr auto default_feedback = 0.5F;

        static constexpr auto min_spread = -1.0F;
        static constexpr auto max_spread = 1.0F;
        static constexpr auto default_spread = -1.0F;


        float delay_;
        float lr_delay_;

        float damping_;
        float feedback_;

        float spread_;


        void set_defaults()
        {
            delay_ = default_delay;
            lr_delay_ = default_lr_delay;
            damping_ = default_damping;
            feedback_ = default_feedback;
            spread_ = default_spread;
        }

        void normalize()
        {
            Math::clamp_i(delay_, min_delay, max_delay);
            Math::clamp_i(lr_delay_, min_lr_delay, max_lr_delay);
            Math::clamp_i(damping_, min_damping, max_damping);
            Math::clamp_i(feedback_, min_feedback, max_feedback);
            Math::clamp_i(spread_, min_spread, max_spread);
        }
    }; // Echo

    struct Equalizer
    {
        static constexpr auto min_low_gain = 0.126F;
        static constexpr auto max_low_gain = 7.943F;
        static constexpr auto default_low_gain = 1.0F;

        static constexpr auto min_low_cutoff = 50.0F;
        static constexpr auto max_low_cutoff = 800.0F;
        static constexpr auto default_low_cutoff = 200.0F;

        static constexpr auto min_mid1_gain = 0.126F;
        static constexpr auto max_mid1_gain = 7.943F;
        static constexpr auto default_mid1_gain = 1.0F;

        static constexpr auto min_mid1_center = 200.0F;
        static constexpr auto max_mid1_center = 3000.0F;
        static constexpr auto default_mid1_center = 500.0F;

        static constexpr auto min_mid1_width = 0.01F;
        static constexpr auto max_mid1_width = 1.0F;
        static constexpr auto default_mid1_width = 1.0F;

        static constexpr auto min_mid2_gain = 0.126F;
        static constexpr auto max_mid2_gain = 7.943F;
        static constexpr auto default_mid2_gain = 1.0F;

        static constexpr auto min_mid2_center = 1000.0F;
        static constexpr auto max_mid2_center = 8000.0F;
        static constexpr auto default_mid2_center = 3000.0F;

        static constexpr auto min_mid2_width = 0.01F;
        static constexpr auto max_mid2_width = 1.0F;
        static constexpr auto default_mid2_width = 1.0F;

        static constexpr auto min_high_gain = 0.126F;
        static constexpr auto max_high_gain = 7.943F;
        static constexpr auto default_high_gain = 1.0F;

        static constexpr auto min_high_cutoff = 4000.0F;
        static constexpr auto max_high_cutoff = 16000.0F;
        static constexpr auto default_high_cutoff = 6000.0F;


        float low_cutoff_;
        float low_gain_;
        float mid1_center_;
        float mid1_gain_;
        float mid1_width_;
        float mid2_center_;
        float mid2_gain_;
        float mid2_width_;
        float high_cutoff_;
        float high_gain_;


        void set_defaults()
        {
            low_cutoff_ = default_low_cutoff;
            low_gain_ = default_high_gain;
            mid1_center_ = default_mid1_center;
            mid1_gain_ = default_mid1_gain;
            mid1_width_ = default_mid1_width;
            mid2_center_ = default_mid2_center;
            mid2_gain_ = default_mid2_gain;
            mid2_width_ = default_mid2_width;
            high_cutoff_ = default_high_cutoff;
            high_gain_ = default_high_gain;
        }

        void normalize()
        {
            Math::clamp_i(low_cutoff_, min_low_cutoff, max_low_cutoff);
            Math::clamp_i(low_gain_, min_high_gain, max_high_gain);
            Math::clamp_i(mid1_center_, min_mid1_center, max_mid1_center);
            Math::clamp_i(mid1_gain_, min_mid1_gain, max_mid1_gain);
            Math::clamp_i(mid1_width_, min_mid1_width, max_mid1_width);
            Math::clamp_i(mid2_center_, min_mid2_center, max_mid2_center);
            Math::clamp_i(mid2_gain_, max_mid2_gain, max_mid2_gain);
            Math::clamp_i(mid2_width_, min_mid2_width, max_mid2_width);
            Math::clamp_i(high_cutoff_, min_high_cutoff, max_high_cutoff);
            Math::clamp_i(high_gain_, min_high_gain, max_high_gain);
        }
    }; // Equalizer

    struct Flanger
    {
        static constexpr auto waveform_sinusoid = 0;
        static constexpr auto waveform_triangle = 1;

        static constexpr auto min_waveform = waveform_sinusoid;
        static constexpr auto max_waveform = waveform_triangle;
        static constexpr auto default_waveform = waveform_triangle;

        static constexpr auto min_phase = -180;
        static constexpr auto max_phase = 180;
        static constexpr auto default_phase = 0;

        static constexpr auto min_rate = 0.0F;
        static constexpr auto max_rate = 10.0F;
        static constexpr auto default_rate = 0.27F;

        static constexpr auto min_depth = 0.0F;
        static constexpr auto max_depth = 1.0F;
        static constexpr auto default_depth = 1.0F;

        static constexpr auto min_feedback = -1.0F;
        static constexpr auto max_feedback = 1.0F;
        static constexpr auto default_feedback = -0.5F;

        static constexpr auto min_delay = 0.0F;
        static constexpr auto max_delay = 0.004F;
        static constexpr auto default_delay = 0.002F;


        int waveform_;
        int phase_;
        float rate_;
        float depth_;
        float feedback_;
        float delay_;


        void set_defaults()
        {
            waveform_ = default_waveform;
            phase_ = default_phase;
            rate_ = default_rate;
            depth_ = default_depth;
            feedback_ = default_feedback;
            delay_ = default_delay;
        }

        void normalize()
        {
            Math::clamp_i(waveform_, min_waveform, max_waveform);
            Math::clamp_i(phase_, min_phase, max_phase);
            Math::clamp_i(rate_, min_rate, max_rate);
            Math::clamp_i(depth_, min_depth, max_depth);
            Math::clamp_i(feedback_, min_feedback, max_feedback);
            Math::clamp_i(delay_, min_delay, max_delay);
        }
    }; // Flanger

    struct Reverb
    {
        static constexpr auto min_density = 0.0F;
        static constexpr auto max_density = 1.0F;
        static constexpr auto default_density = 1.0F;

        static constexpr auto min_diffusion = 0.0F;
        static constexpr auto max_diffusion = 1.0F;
        static constexpr auto default_diffusion = 1.0F;

        static constexpr auto min_gain = 0.0F;
        static constexpr auto max_gain = 1.0F;
        static constexpr auto default_gain = 0.32F;

        static constexpr auto min_gain_hf = 0.0F;
        static constexpr auto max_gain_hf = 1.0F;
        static constexpr auto default_gain_hf = 0.89F;

        static constexpr auto min_gain_lf = 0.0F;
        static constexpr auto max_gain_lf = 1.0F;
        static constexpr auto default_gain_lf = 1.0F;

        static constexpr auto min_decay_time = 0.1F;
        static constexpr auto max_decay_time = 20.0F;
        static constexpr auto default_decay_time = 1.49F;

        static constexpr auto min_decay_hf_ratio = 0.1F;
        static constexpr auto max_decay_hf_ratio = 2.0F;
        static constexpr auto default_decay_hf_ratio = 0.83F;

        static constexpr auto min_decay_lf_ratio = 0.1F;
        static constexpr auto max_decay_lf_ratio = 2.0F;
        static constexpr auto default_decay_lf_ratio = 1.0F;

        static constexpr auto min_reflections_gain = 0.0F;
        static constexpr auto max_reflections_gain = 3.16F;
        static constexpr auto default_reflections_gain = 0.05F;

        static constexpr auto min_reflections_delay = 0.0F;
        static constexpr auto max_reflections_delay = 0.3F;
        static constexpr auto default_reflections_delay = 0.007F;

        static constexpr auto min_reflections_pan_xyz = -1.0F;
        static constexpr auto max_reflections_pan_xyz = 1.0F;
        static constexpr auto default_reflections_pan_xyz = 0.0F;

        static constexpr auto min_late_reverb_gain = 0.0F;
        static constexpr auto max_late_reverb_gain = 10.0F;
        static constexpr auto default_late_reverb_gain = 1.26F;

        static constexpr auto min_late_reverb_delay = 0.0F;
        static constexpr auto max_late_reverb_delay = 0.1F;
        static constexpr auto default_late_reverb_delay = 0.011F;

        static constexpr auto min_late_reverb_pan_xyz = -1.0F;
        static constexpr auto max_late_reverb_pan_xyz = 1.0F;
        static constexpr auto default_late_reverb_pan_xyz = 0.0F;

        static constexpr auto min_echo_time = 0.075F;
        static constexpr auto max_echo_time = 0.25F;
        static constexpr auto default_echo_time = 0.25F;

        static constexpr auto min_echo_depth = 0.0F;
        static constexpr auto max_echo_depth = 1.0F;
        static constexpr auto default_echo_depth = 0.0F;

        static constexpr auto min_modulation_time = 0.04F;
        static constexpr auto max_modulation_time = 4.0F;
        static constexpr auto default_modulation_time = 0.25F;

        static constexpr auto min_modulation_depth = 0.0F;
        static constexpr auto max_modulation_depth = 1.0F;
        static constexpr auto default_modulation_depth = 0.0F;

        static constexpr auto min_air_absorption_gain_hf = 0.892F;
        static constexpr auto max_air_absorption_gain_hf = 1.0F;
        static constexpr auto default_air_absorption_gain_hf = 0.994F;

        static constexpr auto min_hf_reference = 1000.0F;
        static constexpr auto max_hf_reference = 20000.0F;
        static constexpr auto default_hf_reference = 5000.0F;

        static constexpr auto min_lf_reference = 20.0F;
        static constexpr auto max_lf_reference = 1000.0F;
        static constexpr auto default_lf_reference = 250.0F;

        static constexpr auto min_room_rolloff_factor = 0.0F;
        static constexpr auto max_room_rolloff_factor = 10.0F;
        static constexpr auto default_room_rolloff_factor = 0.0F;

        static constexpr auto min_decay_hf_limit = false;
        static constexpr auto max_decay_hf_limit = true;
        static constexpr auto default_decay_hf_limit = true;


        // Shared reverb properties
        float density_;
        float diffusion_;
        float gain_;
        float gain_hf_;
        float decay_time_;
        float decay_hf_ratio_;
        float reflections_gain_;
        float reflections_delay_;
        float late_reverb_gain_;
        float late_reverb_delay_;
        float air_absorption_gain_hf_;
        float room_rolloff_factor_;
        bool decay_hf_limit_;

        // Additional EAX reverb properties
        float gain_lf_;
        float decay_lf_ratio_;
        Pan reflections_pan_;
        Pan late_reverb_pan_;
        float echo_time_;
        float echo_depth_;
        float modulation_time_;
        float modulation_depth_;
        float hf_reference_;
        float lf_reference_;


        void set_defaults()
        {
            density_ = default_density;
            diffusion_ = default_diffusion;
            gain_ = default_gain;
            gain_hf_ = default_gain_hf;
            gain_lf_ = default_gain_lf;
            decay_time_ = default_decay_time;
            decay_hf_ratio_ = default_decay_hf_ratio;
            decay_lf_ratio_ = default_decay_lf_ratio;
            reflections_gain_ = default_reflections_gain;
            reflections_delay_ = default_reflections_delay;
            reflections_pan_.fill(default_reflections_pan_xyz);
            late_reverb_gain_ = default_late_reverb_gain;
            late_reverb_delay_ = default_late_reverb_delay;
            late_reverb_pan_.fill(default_late_reverb_pan_xyz);
            echo_time_ = default_echo_time;
            echo_depth_ = default_echo_depth;
            modulation_time_ = default_modulation_time;
            modulation_depth_ = default_modulation_depth;
            air_absorption_gain_hf_ = default_air_absorption_gain_hf;
            hf_reference_ = default_hf_reference;
            lf_reference_ = default_lf_reference;
            room_rolloff_factor_ = default_room_rolloff_factor;
            decay_hf_limit_ = default_decay_hf_limit;
        }

        void normalize()
        {
            Math::clamp_i(density_, min_density, max_density);
            Math::clamp_i(diffusion_, min_diffusion, max_diffusion);
            Math::clamp_i(gain_, min_gain, max_gain);
            Math::clamp_i(gain_hf_, min_gain_hf, max_gain_hf);
            Math::clamp_i(gain_lf_, min_gain_lf, max_gain_lf);
            Math::clamp_i(decay_time_, min_decay_time, max_decay_time);
            Math::clamp_i(decay_hf_ratio_, min_decay_hf_ratio, max_decay_hf_ratio);
            Math::clamp_i(decay_lf_ratio_, min_decay_lf_ratio, max_decay_lf_ratio);
            Math::clamp_i(reflections_gain_, min_reflections_gain, max_reflections_gain);
            Math::clamp_i(reflections_delay_, min_reflections_delay, max_reflections_delay);
            Math::clamp_i(reflections_pan_[0], min_reflections_pan_xyz, max_reflections_pan_xyz);
            Math::clamp_i(reflections_pan_[1], min_reflections_pan_xyz, max_reflections_pan_xyz);
            Math::clamp_i(reflections_pan_[2], min_reflections_pan_xyz, max_reflections_pan_xyz);
            Math::clamp_i(late_reverb_gain_, min_late_reverb_gain, max_late_reverb_gain);
            Math::clamp_i(late_reverb_delay_, min_late_reverb_delay, max_late_reverb_delay);
            Math::clamp_i(late_reverb_pan_[0], min_late_reverb_pan_xyz, max_late_reverb_pan_xyz);
            Math::clamp_i(late_reverb_pan_[1], min_late_reverb_pan_xyz, max_late_reverb_pan_xyz);
            Math::clamp_i(late_reverb_pan_[2], min_late_reverb_pan_xyz, max_late_reverb_pan_xyz);
            Math::clamp_i(echo_time_, min_echo_time, max_echo_time);
            Math::clamp_i(echo_depth_, min_echo_depth, max_echo_depth);
            Math::clamp_i(modulation_time_, min_modulation_time, max_modulation_time);
            Math::clamp_i(modulation_depth_, min_modulation_depth, max_modulation_depth);
            Math::clamp_i(air_absorption_gain_hf_, min_air_absorption_gain_hf, max_air_absorption_gain_hf);
            Math::clamp_i(hf_reference_, min_hf_reference, max_hf_reference);
            Math::clamp_i(lf_reference_, min_lf_reference, max_lf_reference);
            Math::clamp_i(room_rolloff_factor_, min_room_rolloff_factor, max_room_rolloff_factor);
            Math::clamp_i(decay_hf_limit_, min_decay_hf_limit, max_decay_hf_limit);
        }
    }; // Reverb

    struct Modulator
    {
        static constexpr auto min_frequency = 0.0F;
        static constexpr auto max_frequency = 8000.0F;
        static constexpr auto default_frequency = 440.0F;

        static constexpr auto min_high_pass_cutoff = 0.0F;
        static constexpr auto max_high_pass_cutoff = 24000.0F;
        static constexpr auto default_high_pass_cutoff = 800.0F;

        static constexpr auto waveform_sinusoid = 0;
        static constexpr auto waveform_sawtooth = 1;
        static constexpr auto waveform_square = 2;

        static constexpr auto min_waveform = waveform_sinusoid;
        static constexpr auto max_waveform = waveform_square;
        static constexpr auto default_waveform = waveform_sinusoid;


        float frequency_;
        float high_pass_cutoff_;
        int waveform_;


        void set_defaults()
        {
            frequency_ = default_frequency;
            high_pass_cutoff_ = default_high_pass_cutoff;
            waveform_ = default_waveform;
        }

        void normalize()
        {
            Math::clamp_i(frequency_, min_frequency, max_frequency);
            Math::clamp_i(high_pass_cutoff_, min_high_pass_cutoff, max_high_pass_cutoff);
            Math::clamp_i(waveform_, min_waveform, max_waveform);
        }
    }; // Modulator


    Reverb reverb_;
    Chorus chorus_;
    Compressor compressor_;
    Distortion distortion_;
    Echo echo_;
    Equalizer equalizer_;
    Flanger flanger_;
    Modulator modulator_;
    Dedicated dedicated_;
}; // EffectProps


struct Effect
{
    EffectType type_;
    EffectProps props_;


    void set_defaults()
    {
        switch (type_)
        {
        case EffectType::chorus:
            props_.chorus_.set_defaults();
            break;

        case EffectType::compressor:
            props_.compressor_.set_defaults();
            break;

        case EffectType::dedicated_dialog:
        case EffectType::dedicated_low_frequency:
            props_.dedicated_.set_defaults();
            break;

        case EffectType::distortion:
            props_.distortion_.set_defaults();
            break;

        case EffectType::echo:
            props_.echo_.set_defaults();
            break;

        case EffectType::equalizer:
            props_.equalizer_.set_defaults();
            break;

        case EffectType::flanger:
            props_.flanger_.set_defaults();
            break;

        case EffectType::eax_reverb:
        case EffectType::reverb:
            props_.reverb_.set_defaults();
            break;

        case EffectType::ring_modulator:
            props_.modulator_.set_defaults();
            break;

        default:
            break;
        }
    }

    void set_type_and_defaults(
        const EffectType effect_type)
    {
        type_ = effect_type;
        set_defaults();
    }

    void set_normalize()
    {
        switch (type_)
        {
        case EffectType::chorus:
            props_.chorus_.normalize();
            break;

        case EffectType::compressor:
            props_.compressor_.normalize();
            break;

        case EffectType::dedicated_dialog:
        case EffectType::dedicated_low_frequency:
            props_.dedicated_.normalize();
            break;

        case EffectType::distortion:
            props_.distortion_.normalize();
            break;

        case EffectType::echo:
            props_.echo_.normalize();
            break;

        case EffectType::equalizer:
            props_.equalizer_.normalize();
            break;

        case EffectType::flanger:
            props_.flanger_.normalize();
            break;

        case EffectType::eax_reverb:
        case EffectType::reverb:
            props_.reverb_.normalize();
            break;

        case EffectType::ring_modulator:
            props_.modulator_.normalize();
            break;

        default:
            break;
        }
    }
}; // Effect

class EffectState
{
public:
    EffectState(
        const EffectState& that) = delete;

    EffectState& operator=(
        const EffectState& that) = delete;

    virtual ~EffectState()
    {
    }


    SampleBuffers* dst_buffers_;
    int dst_channel_count_;


    void construct()
    {
        do_construct();
    }

    void destruct()
    {
        do_destruct();
    }

    void update_device(
        ALCdevice& device)
    {
        do_update_device(device);
    }

    void update(
        ALCdevice& device,
        const EffectSlot& effect_slot,
        const EffectProps& props)
    {
        do_update(device, effect_slot, props);
    }

    void process(
        int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count)
    {
        do_process(sample_count, src_samples, dst_samples, channel_count);
    }

    static void destroy(
        EffectState*& effect_state)
    {
        if (!effect_state)
        {
            return;
        }

        effect_state->destruct();
        delete effect_state;
        effect_state = nullptr;
    }


protected:
    EffectState()
        :
        dst_buffers_{},
        dst_channel_count_{}
    {
    }


    virtual void do_construct() = 0;

    virtual void do_destruct() = 0;

    virtual void do_update_device(
        ALCdevice& device) = 0;

    virtual void do_update(
        ALCdevice& device,
        const EffectSlot& effect_slot,
        const EffectProps& props) = 0;

    virtual void do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) = 0;
}; // EffectState

class EffectStateFactory
{
public:
    static EffectState* create_by_type(
        const EffectType type)
    {
        switch (type)
        {
        case EffectType::null:
            return create_null();

        case EffectType::eax_reverb:
            return create_reverb();

        case EffectType::reverb:
            return create_reverb();

        case EffectType::chorus:
            return create_chorus();

        case EffectType::compressor:
            return create_compressor();

        case EffectType::distortion:
            return create_distortion();

        case EffectType::echo:
            return create_echo();

        case EffectType::equalizer:
            return create_equalizer();

        case EffectType::flanger:
            return create_flanger();

        case EffectType::ring_modulator:
            return create_modulator();

        case EffectType::dedicated_dialog:
        case EffectType::dedicated_low_frequency:
            return create_dedicated();

        default:
            return nullptr;
        }
    }


private:
    static EffectState* create_chorus();
    static EffectState* create_compressor();
    static EffectState* create_dedicated();
    static EffectState* create_distortion();
    static EffectState* create_echo();
    static EffectState* create_equalizer();
    static EffectState* create_flanger();
    static EffectState* create_modulator();
    static EffectState* create_null();
    static EffectState* create_reverb();


    template<typename T>
    static EffectState* create()
    {
        auto result = static_cast<EffectState*>(new (std::nothrow) T{});

        if (result)
        {
            result->construct();
        }

        return result;
    }
};

struct EffectStateDeleter
{
    void operator()(
        EffectState* effect_state)
    {
        if (!effect_state)
        {
            return;
        }

        effect_state->destruct();
        delete effect_state;
    }
};

struct ALCdevice
{
    using ChannelIds = std::array<ChannelId, max_channels>;


    int frequency_;
    int update_size_;
    ChannelFormat channel_format_;

    int channel_count_;
    ChannelIds channel_ids_;
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

        alu_init_renderer();

        sample_buffers_.clear();
        sample_buffers_.resize(channel_count_);
    }

    void uninitialize()
    {
    }

    void set_default_wfx_channel_order()
    {
        channel_ids_.fill(ChannelId::invalid);

        switch (channel_format_)
        {
        case ChannelFormat::mono:
            channel_ids_[0] = ChannelId::front_center;
            break;

        case ChannelFormat::stereo:
            channel_ids_[0] = ChannelId::front_left;
            channel_ids_[1] = ChannelId::front_right;
            break;

        case ChannelFormat::quad:
            channel_ids_[0] = ChannelId::front_left;
            channel_ids_[1] = ChannelId::front_right;
            channel_ids_[2] = ChannelId::back_left;
            channel_ids_[3] = ChannelId::back_right;
            break;

        case ChannelFormat::five_point_one:
            channel_ids_[0] = ChannelId::front_left;
            channel_ids_[1] = ChannelId::front_right;
            channel_ids_[2] = ChannelId::front_center;
            channel_ids_[3] = ChannelId::lfe;
            channel_ids_[4] = ChannelId::side_left;
            channel_ids_[5] = ChannelId::side_right;
            break;

        case ChannelFormat::five_point_one_rear:
            channel_ids_[0] = ChannelId::front_left;
            channel_ids_[1] = ChannelId::front_right;
            channel_ids_[2] = ChannelId::front_center;
            channel_ids_[3] = ChannelId::lfe;
            channel_ids_[4] = ChannelId::back_left;
            channel_ids_[5] = ChannelId::back_right;
            break;

        case ChannelFormat::six_point_one:
            channel_ids_[0] = ChannelId::front_left;
            channel_ids_[1] = ChannelId::front_right;
            channel_ids_[2] = ChannelId::front_center;
            channel_ids_[3] = ChannelId::lfe;
            channel_ids_[4] = ChannelId::back_center;
            channel_ids_[5] = ChannelId::side_left;
            channel_ids_[6] = ChannelId::side_right;
            break;

        case ChannelFormat::seven_point_one:
            channel_ids_[0] = ChannelId::front_left;
            channel_ids_[1] = ChannelId::front_right;
            channel_ids_[2] = ChannelId::front_center;
            channel_ids_[3] = ChannelId::lfe;
            channel_ids_[4] = ChannelId::back_left;
            channel_ids_[5] = ChannelId::back_right;
            channel_ids_[6] = ChannelId::side_left;
            channel_ids_[7] = ChannelId::side_right;
            break;
        }
    }

    void alu_init_renderer()
    {
        dry_.ambi_.reset();
        dry_.coeff_count_ = 0;
        set_default_wfx_channel_order();

        const ChannelPanning *channel_map = nullptr;
        auto coeff_count = 0;
        auto count = 0;

        switch (channel_format_)
        {
        case ChannelFormat::mono:
            count = get_array_extents(Panning::mono_panning);
            channel_map = Panning::mono_panning;
            coeff_count = 1;
            break;

        case ChannelFormat::stereo:
            count = get_array_extents(Panning::stereo_panning);
            channel_map = Panning::stereo_panning;
            coeff_count = 4;
            break;

        case ChannelFormat::quad:
            count = get_array_extents(Panning::quad_panning);
            channel_map = Panning::quad_panning;
            coeff_count = 4;
            break;

        case ChannelFormat::five_point_one:
            count = get_array_extents(Panning::x5_1_side_panning);
            channel_map = Panning::x5_1_side_panning;
            coeff_count = 9;
            break;

        case ChannelFormat::five_point_one_rear:
            count = get_array_extents(Panning::x5_1_rear_panning);
            channel_map = Panning::x5_1_rear_panning;
            coeff_count = 9;
            break;

        case ChannelFormat::six_point_one:
            count = get_array_extents(Panning::x6_1_panning);
            channel_map = Panning::x6_1_panning;
            coeff_count = 9;
            break;

        case ChannelFormat::seven_point_one:
            count = get_array_extents(Panning::x7_1_panning);
            channel_map = Panning::x7_1_panning;
            coeff_count = 16;
            break;
        }

        Panning::set_channel_map(
            channel_ids_.data(),
            dry_.ambi_.coeffs_.data(),
            channel_map,
            count,
            &channel_count_);

        dry_.coeff_count_ = coeff_count;

        foa_.ambi_.reset();

        for (int i = 0; i < channel_count_; ++i)
        {
            foa_.ambi_.coeffs_[i][0] = dry_.ambi_.coeffs_[i][0];

            for (int j = 1; j < 4; ++j)
            {
                foa_.ambi_.coeffs_[i][j] = dry_.ambi_.coeffs_[i][j];
            }
        }

        foa_.coeff_count_ = 4;
    }

    // Returns the index for the given channel name (e.g. FrontCenter), or -1 if it
    // doesn't exist.
    int get_channel_index(
        const ChannelId channel_id_to_find)
    {
        const auto it_begin = channel_ids_.cbegin();
        const auto it_end = channel_ids_.cbegin();

        const auto it = std::find_if(
            it_begin,
            it_end,
            [=](const ChannelId channel_id)
            {
                return channel_id == channel_id_to_find;
            }
        );

        if (it == it_end)
        {
            return -1;
        }

        return static_cast<int>(it - it_begin);
    }
}; // ALCdevice

struct EffectSlot
{
    using EffectStateUPtr = std::unique_ptr<EffectState, EffectStateDeleter>;


    Effect effect_;
    EffectStateUPtr effect_state_;
    bool is_props_updated_;

    // Wet buffer configuration is ACN channel order with N3D scaling:
    // * Channel 0 is the unattenuated mono signal.
    // * Channel 1 is OpenAL -X
    // * Channel 2 is OpenAL Y
    // * Channel 3 is OpenAL -Z
    // Consequently, effects that only want to work with mono input can use
    // channel 0 by itself. Effects that want multichannel can process the
    // ambisonics signal and make a B-Format pan (ComputeFirstOrderGains) for
    // first-order device output (FOAOut).
    SampleBuffers wet_buffer_;


    EffectSlot()
        :
        effect_{},
        effect_state_{},
        is_props_updated_{},
        wet_buffer_{SampleBuffers::size_type{max_effect_channels}}
    {
        initialize();
    }


    EffectSlot(
        const EffectSlot& that) = delete;

    EffectSlot& operator=(
        const EffectSlot& that) = delete;

    ~EffectSlot()
    {
        uninitialize();
    }

    void initialize()
    {
        uninitialize();

        effect_.type_ = EffectType::null;
        effect_state_.reset(EffectStateFactory::create_by_type(EffectType::null));
        is_props_updated_ = true;
    }

    void uninitialize()
    {
        effect_state_.reset(nullptr);
    }

    void set_effect(
        ALCdevice& device,
        Effect& effect)
    {
        if (effect_.type_ != effect.type_)
        {
            effect_state_.reset(EffectStateFactory::create_by_type(effect.type_));

            effect_state_->dst_buffers_ = &device.sample_buffers_;
            effect_state_->dst_channel_count_ = device.channel_count_;
            effect_state_->update_device(device);

            effect_.type_ = effect.type_;
            effect_.props_ = effect.props_;
        }
        else
        {
            effect_.props_ = effect.props_;
        }

        is_props_updated_ = true;
    }
}; // EffectSlot


#endif
