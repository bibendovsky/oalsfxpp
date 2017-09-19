#include "oalsfxpp.h"
#include <cassert>
#include <cmath>
#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <type_traits>
#include <vector>


struct Device;
struct EffectSlot;


constexpr auto max_channels = 8;

constexpr auto min_effects = 1;
constexpr auto max_effects = 4;

constexpr auto max_effect_channels = 4;

constexpr auto min_sampling_rate = 8'000;
constexpr auto max_sampling_rate = 8'000'000;

constexpr auto max_mix_gain = 16.0F; // +24dB

constexpr auto silence_threshold_gain = 0.000'01F; // -100dB

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

enum class ActiveFilters
{
    none = 0,
    low_pass = 1,
    high_pass = 2,
    band_pass = low_pass | high_pass
}; // ActiveFilters


using AmbiCoeffs = std::array<float, max_ambi_coeffs>;
using Gains = std::array<float, max_channels>;
using WetGains = std::array<float, max_effects>;
using ChannelConfig = std::array<float, max_ambi_coeffs>;
using SampleBuffer = std::array<float, max_sample_buffer_size>;
using SampleBuffers = std::vector<SampleBuffer>;
using EffectSampleBuffer = std::vector<float>;


namespace detail
{


template<typename T, std::size_t TExtent1, std::size_t... TExtents>
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

    static constexpr float get_epsilon()
    {
        return std::numeric_limits<float>::epsilon();
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
    static_assert(std::rank<T>::value == 1, "Expected an one-dimensional array.");
    return static_cast<int>(std::extent<T>::value);
}


int channel_format_to_channel_count(
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
        static_cast<void>(channel_count);

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
struct FilterState
{
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

struct Source
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


        SendProps props_;
        SendProps deferred_props_;

        ActiveFilters filter_type_;
        Channels channels_;
        SampleBuffers* buffers_;
        int channel_count_;
    }; // Send

    using Sends = std::vector<Send>;


    Send direct_;
    Sends auxes_;
    bool are_props_changed_;


    void initialize(
        const int effect_count)
    {
        direct_.props_.set_defaults();
        direct_.deferred_props_.set_defaults();

        auxes_.clear();
        auxes_.resize(effect_count);

        for (auto& aux : auxes_)
        {
            aux.props_.set_defaults();
            aux.deferred_props_.set_defaults();
        }

        are_props_changed_ = true;
    }
}; // Source


// ==========================================================================
// EffectProps

void EffectProps::Chorus::set_defaults()
{
    waveform_ = default_waveform;
    phase_ = default_phase;
    rate_ = default_rate;
    depth_ = default_depth;
    feedback_ = default_feedback;
    delay_ = default_delay;
}

void EffectProps::Chorus::normalize()
{
    Math::clamp_i(waveform_, min_waveform, max_waveform);
    Math::clamp_i(phase_, min_phase, max_phase);
    Math::clamp_i(rate_, min_rate, max_rate);
    Math::clamp_i(depth_, min_depth, max_depth);
    Math::clamp_i(feedback_, min_feedback, max_feedback);
    Math::clamp_i(delay_, min_delay, max_delay);
}

bool EffectProps::Chorus::are_equal(
    const Chorus& a,
    const Chorus& b)
{
    return
        a.waveform_ == b.waveform_ &&
        a.phase_ == b.phase_ &&
        a.rate_ == b.rate_ &&
        a.depth_ == b.depth_ &&
        a.feedback_ == b.feedback_ &&
        a.delay_ == b.delay_;
}

void EffectProps::Compressor::set_defaults()
{
    on_off_ = default_on_off;
}

void EffectProps::Compressor::normalize()
{
}

bool EffectProps::Compressor::are_equal(
    const Compressor& a,
    const Compressor& b)
{
    return
        a.on_off_ == b.on_off_;
}

void EffectProps::Dedicated::set_defaults()
{
    gain_ = default_gain;
}

void EffectProps::Dedicated::normalize()
{
    Math::clamp_i(gain_, min_gain, max_gain);
}

bool EffectProps::Dedicated::are_equal(
    const Dedicated& a,
    const Dedicated& b)
{
    return
        a.gain_ == b.gain_;
}

void EffectProps::Distortion::set_defaults()
{
    edge_ = default_edge;
    gain_ = default_gain;
    low_pass_cutoff_ = default_low_pass_cutoff;
    eq_center_ = default_eq_center;
    eq_bandwidth_ = default_eq_bandwidth;
}

void EffectProps::Distortion::normalize()
{
    Math::clamp_i(edge_, min_edge, max_edge);
    Math::clamp_i(gain_, min_gain, max_gain);
    Math::clamp_i(low_pass_cutoff_, min_low_pass_cutoff, max_low_pass_cutoff);
    Math::clamp_i(eq_center_, min_eq_center, max_eq_center);
    Math::clamp_i(eq_bandwidth_, min_eq_bandwidth, max_eq_bandwidth);
}

bool EffectProps::Distortion::are_equal(
    const Distortion& a,
    const Distortion& b)
{
    return
        a.edge_ == b.edge_ &&
        a.gain_ == b.gain_ &&
        a.low_pass_cutoff_ == b.low_pass_cutoff_ &&
        a.eq_center_ == b.eq_center_ &&
        a.eq_bandwidth_ == b.eq_bandwidth_;
}

void EffectProps::Echo::set_defaults()
{
    delay_ = default_delay;
    lr_delay_ = default_lr_delay;
    damping_ = default_damping;
    feedback_ = default_feedback;
    spread_ = default_spread;
}

void EffectProps::Echo::normalize()
{
    Math::clamp_i(delay_, min_delay, max_delay);
    Math::clamp_i(lr_delay_, min_lr_delay, max_lr_delay);
    Math::clamp_i(damping_, min_damping, max_damping);
    Math::clamp_i(feedback_, min_feedback, max_feedback);
    Math::clamp_i(spread_, min_spread, max_spread);
}

bool EffectProps::Echo::are_equal(
    const Echo& a,
    const Echo& b)
{
    return
        a.delay_ == b.delay_ &&
        a.lr_delay_ == b.lr_delay_ &&
        a.damping_ == b.damping_ &&
        a.feedback_ == b.feedback_ &&
        a.spread_ == b.spread_;
}

void EffectProps::Equalizer::set_defaults()
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

void EffectProps::Equalizer::normalize()
{
    Math::clamp_i(low_cutoff_, min_low_cutoff, max_low_cutoff);
    Math::clamp_i(low_gain_, min_low_gain, max_low_gain);
    Math::clamp_i(mid1_center_, min_mid1_center, max_mid1_center);
    Math::clamp_i(mid1_gain_, min_mid1_gain, max_mid1_gain);
    Math::clamp_i(mid1_width_, min_mid1_width, max_mid1_width);
    Math::clamp_i(mid2_center_, min_mid2_center, max_mid2_center);
    Math::clamp_i(mid2_gain_, min_mid2_gain, max_mid2_gain);
    Math::clamp_i(mid2_width_, min_mid2_width, max_mid2_width);
    Math::clamp_i(high_cutoff_, min_high_cutoff, max_high_cutoff);
    Math::clamp_i(high_gain_, min_high_gain, max_high_gain);
}

bool EffectProps::Equalizer::are_equal(
    const Equalizer& a,
    const Equalizer& b)
{
    return
        a.low_cutoff_ == b.low_cutoff_ &&
        a.low_gain_ == b.low_gain_ &&
        a.mid1_center_ == b.mid1_center_ &&
        a.mid1_gain_ == b.mid1_gain_ &&
        a.mid1_width_ == b.mid1_width_ &&
        a.mid2_center_ == b.mid2_center_ &&
        a.mid2_gain_ == b.mid2_gain_ &&
        a.mid2_width_ == b.mid2_width_ &&
        a.high_cutoff_ == b.high_cutoff_ &&
        a.high_gain_ == b.high_gain_;
}

void EffectProps::Flanger::set_defaults()
{
    waveform_ = default_waveform;
    phase_ = default_phase;
    rate_ = default_rate;
    depth_ = default_depth;
    feedback_ = default_feedback;
    delay_ = default_delay;
}

void EffectProps::Flanger::normalize()
{
    Math::clamp_i(waveform_, min_waveform, max_waveform);
    Math::clamp_i(phase_, min_phase, max_phase);
    Math::clamp_i(rate_, min_rate, max_rate);
    Math::clamp_i(depth_, min_depth, max_depth);
    Math::clamp_i(feedback_, min_feedback, max_feedback);
    Math::clamp_i(delay_, min_delay, max_delay);
}

bool EffectProps::Flanger::are_equal(
    const Flanger& a,
    const Flanger& b)
{
    return
        a.waveform_ == b.waveform_ &&
        a.phase_ == b.phase_ &&
        a.rate_ == b.rate_ &&
        a.depth_ == b.depth_ &&
        a.feedback_ == b.feedback_ &&
        a.delay_ == b.delay_;
}

void EffectProps::Reverb::set_defaults()
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

void EffectProps::Reverb::normalize()
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

bool EffectProps::Reverb::are_equal(
    const Reverb& a,
    const Reverb& b)
{
    return
        a.density_ == b.density_ &&
        a.diffusion_ == b.diffusion_ &&
        a.gain_ == b.gain_ &&
        a.gain_hf_ == b.gain_hf_ &&
        a.decay_time_ == b.decay_time_ &&
        a.decay_hf_ratio_ == b.decay_hf_ratio_ &&
        a.reflections_gain_ == b.reflections_gain_ &&
        a.reflections_delay_ == b.reflections_delay_ &&
        a.late_reverb_gain_ == b.late_reverb_gain_ &&
        a.late_reverb_delay_ == b.late_reverb_delay_ &&
        a.air_absorption_gain_hf_ == b.air_absorption_gain_hf_ &&
        a.room_rolloff_factor_ == b.room_rolloff_factor_ &&
        a.decay_hf_limit_ == b.decay_hf_limit_ &&
        a.gain_lf_ == b.gain_lf_ &&
        a.decay_lf_ratio_ == b.decay_lf_ratio_ &&
        a.reflections_pan_ == b.reflections_pan_ &&
        a.late_reverb_pan_ == b.late_reverb_pan_ &&
        a.echo_time_ == b.echo_time_ &&
        a.echo_depth_ == b.echo_depth_ &&
        a.modulation_time_ == b.modulation_time_ &&
        a.modulation_depth_ == b.modulation_depth_ &&
        a.hf_reference_ == b.hf_reference_ &&
        a.lf_reference_ == b.lf_reference_;
}

void EffectProps::RingModulator::set_defaults()
{
    frequency_ = default_frequency;
    high_pass_cutoff_ = default_high_pass_cutoff;
    waveform_ = default_waveform;
}

void EffectProps::RingModulator::normalize()
{
    Math::clamp_i(frequency_, min_frequency, max_frequency);
    Math::clamp_i(high_pass_cutoff_, min_high_pass_cutoff, max_high_pass_cutoff);
    Math::clamp_i(waveform_, min_waveform, max_waveform);
}

bool EffectProps::RingModulator::are_equal(
    const RingModulator& a,
    const RingModulator& b)
{
    return
        a.frequency_ == b.frequency_ &&
        a.high_pass_cutoff_ == b.high_pass_cutoff_ &&
        a.waveform_ == b.waveform_;
}

// EffectProps
// ==========================================================================


// ==========================================================================
// Effect

void Effect::set_defaults()
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
        props_.ring_modulator_.set_defaults();
        break;

    case EffectType::null:
    default:
        break;
    }
}

void Effect::set_type_and_defaults(
    const EffectType effect_type)
{
    type_ = effect_type;
    set_defaults();
}

void Effect::normalize()
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
        props_.ring_modulator_.normalize();
        break;

    case EffectType::null:
    default:
        break;
    }
}

bool Effect::are_equal(
    const Effect& a,
    const Effect& b)
{
    if (a.type_ != b.type_)
    {
        return false;
    }

    switch (a.type_)
    {
    case EffectType::null:
        return true;

    case EffectType::chorus:
        return EffectProps::Chorus::are_equal(a.props_.chorus_, b.props_.chorus_);

    case EffectType::compressor:
        return EffectProps::Compressor::are_equal(a.props_.compressor_, b.props_.compressor_);

    case EffectType::dedicated_dialog:
    case EffectType::dedicated_low_frequency:
        return EffectProps::Dedicated::are_equal(a.props_.dedicated_, b.props_.dedicated_);

    case EffectType::distortion:
        return EffectProps::Distortion::are_equal(a.props_.distortion_, b.props_.distortion_);

    case EffectType::echo:
        return EffectProps::Echo::are_equal(a.props_.echo_, b.props_.echo_);

    case EffectType::equalizer:
        return EffectProps::Equalizer::are_equal(a.props_.equalizer_, b.props_.equalizer_);

    case EffectType::flanger:
        return EffectProps::Flanger::are_equal(a.props_.flanger_, b.props_.flanger_);

    case EffectType::eax_reverb:
    case EffectType::reverb:
        return EffectProps::Reverb::are_equal(a.props_.reverb_, b.props_.reverb_);

    case EffectType::ring_modulator:
        return EffectProps::RingModulator::are_equal(a.props_.ring_modulator_, b.props_.ring_modulator_);

    default:
        return false;
    }
}

// Effect
// ==========================================================================


// ==========================================================================
// SendProps

void SendProps::set_defaults()
{
    gain_ = default_gain;
    gain_hf_ = default_gain_hf;
    gain_lf_ = default_gain_lf;
}

void SendProps::normalize()
{
    Math::clamp_i(gain_, min_gain, max_gain);
    Math::clamp_i(gain_hf_, min_gain_hf, max_gain_hf);
    Math::clamp_i(gain_lf_, min_gain_lf, max_gain_lf);
}

bool SendProps::are_equal(
    const SendProps& a,
    const SendProps& b)
{
    return
        a.gain_ == b.gain_ &&
        a.gain_hf_ == b.gain_hf_ &&
        a.gain_lf_ == b.gain_lf_;
}

// SendProps
// ==========================================================================


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
        Device& device)
    {
        do_update_device(device);
    }

    void update(
        Device& device,
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
        Device& device) = 0;

    virtual void do_update(
        Device& device,
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

        case EffectType::chorus:
            return create_chorus();

        case EffectType::compressor:
            return create_compressor();

        case EffectType::dedicated_dialog:
        case EffectType::dedicated_low_frequency:
            return create_dedicated();

        case EffectType::distortion:
            return create_distortion();

        case EffectType::echo:
            return create_echo();

        case EffectType::equalizer:
            return create_equalizer();

        case EffectType::flanger:
            return create_flanger();

        case EffectType::eax_reverb:
        case EffectType::reverb:
            return create_reverb();

        case EffectType::ring_modulator:
            return create_ring_modulator();

        default:
            return nullptr;
        }
    }


private:
    static EffectState* create_null();
    static EffectState* create_chorus();
    static EffectState* create_compressor();
    static EffectState* create_dedicated();
    static EffectState* create_distortion();
    static EffectState* create_echo();
    static EffectState* create_equalizer();
    static EffectState* create_flanger();
    static EffectState* create_reverb();
    static EffectState* create_ring_modulator();


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
}; // EffectStateFactory

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
}; // EffectStateDeleter

struct Device
{
    using ChannelIds = std::array<ChannelId, max_channels>;


    int sampling_rate_;
    int channel_count_;
    ChannelFormat channel_format_;
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
        sampling_rate_ = sampling_rate;

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

        case ChannelFormat::none:
        default:
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

        case ChannelFormat::none:
        default:
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
}; // Device

struct EffectSlot
{
    using EffectStateUPtr = std::unique_ptr<EffectState, EffectStateDeleter>;


    Effect effect_;
    EffectStateUPtr effect_state_;
    bool is_props_changed_;

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
        is_props_changed_{},
        wet_buffer_{SampleBuffers::size_type{max_effect_channels}}
    {
    }

    EffectSlot(
        const EffectSlot& that) = delete;

    EffectSlot(
        EffectSlot&& that) = default;

    EffectSlot& operator=(
        const EffectSlot& that) = delete;

    EffectSlot& operator=(
        EffectSlot&& that) = default;

    ~EffectSlot()
    {
        uninitialize();
    }

    void initialize()
    {
        uninitialize();

        effect_.type_ = EffectType::null;
        effect_state_.reset(EffectStateFactory::create_by_type(EffectType::null));
        is_props_changed_ = true;
    }

    void uninitialize()
    {
        effect_state_.reset(nullptr);
    }

    void set_effect(
        Device& device,
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

        is_props_changed_ = true;
    }
}; // EffectSlot

struct EffectContext
{
    Effect deferred_effect_;
    EffectSlot effect_slot_;
}; // EffectContext

using EffectContexts = std::vector<EffectContext>;


struct MixHelpers
{
    // Basically the inverse of the "mix". Rather than one input going to multiple
    // outputs (each with its own gain), it's multiple inputs (each with its own
    // gain) going to one output. This applies one row (vs one column) of a matrix
    // transform. And as the matrices are more or less static once set up, no
    // stepping is necessary.
    static void mix_row(
        float* dst_buffer,
        const float* gains,
        const SampleBuffers& src_buffers,
        const int channel_count,
        const int src_position,
        const int buffer_size)
    {
        for (int c = 0; c < channel_count; ++c)
        {
            const auto gain = gains[c];

            if (!(std::abs(gain) > silence_threshold_gain))
            {
                continue;
            }

            for (int i = 0; i < buffer_size; ++i)
            {
                dst_buffer[i] += src_buffers[c][src_position + i] * gain;
            }
        }
    }

    static void mix(
        const float* data,
        const int channel_count,
        SampleBuffers& dst_buffers,
        float* current_gains,
        const float* target_gains,
        const int counter,
        const int dst_position,
        const int buffer_size)
    {
        const auto delta = ((counter > 0) ? 1.0F / static_cast<float>(counter) : 0.0F);

        for (int c = 0; c < channel_count; ++c)
        {
            auto pos = 0;
            auto gain = current_gains[c];
            const auto step = (target_gains[c] - gain) * delta;

            if (std::abs(step) > Math::get_epsilon())
            {
                const auto size = std::min(buffer_size, counter);

                for ( ; pos < size; ++pos)
                {
                    dst_buffers[c][dst_position + pos] += data[pos] * gain;
                    gain += step;
                }

                if (pos == counter)
                {
                    gain = target_gains[c];
                }

                current_gains[c] = gain;
            }

            if (!(std::abs(gain) > silence_threshold_gain))
            {
                continue;
            }

            for ( ; pos < buffer_size; ++pos)
            {
                dst_buffers[c][dst_position + pos] += data[pos] * gain;
            }
        }
    }
}; // MixHelpers


// ==========================================================================
// Api::Impl

struct ApiImplErrorMessages
{
    static constexpr auto NoError = "";
    static constexpr auto InvalidChannelFormat = "Invalid channel format.";
    static constexpr auto SamplingRateOutOfRange = "Sampling rate is out of range.";
    static constexpr auto EffectCountOutOfRange = "Effect count is out of range.";
}; // ApiImplErrorMessages


class Api::Impl
{
public:
    Device device_;
    Source source_;
    EffectContexts effect_contexts_;
    int effect_count_;
    const char* error_message_;


    Impl()
        :
        device_{},
        source_{},
        effect_contexts_{},
        effect_count_{},
        error_message_{ApiImplErrorMessages::NoError}
    {
    }

    ~Impl()
    {
        uninitialize();
    }


    bool initialize(
        const ChannelFormat channel_format,
        const int sampling_rate,
        const int effect_count)
    {
        uninitialize();

        const auto channel_count = channel_format_to_channel_count(channel_format);

        if (channel_count == 0)
        {
            error_message_ = ApiImplErrorMessages::InvalidChannelFormat;
            return false;
        }

        if (sampling_rate < min_sampling_rate)
        {
            error_message_ = ApiImplErrorMessages::SamplingRateOutOfRange;
            return false;
        }

        if (effect_count <= 0 || effect_count > max_effects)
        {
            error_message_ = ApiImplErrorMessages::EffectCountOutOfRange;
            return false;
        }

        device_.initialize(channel_format, sampling_rate);

        effect_count_ = effect_count;

        effect_contexts_.clear();
        effect_contexts_.resize(effect_count_);

        for (auto& effect_context : effect_contexts_)
        {
            effect_context.deferred_effect_.set_type_and_defaults(EffectType::null);
            effect_context.effect_slot_.initialize();

            auto effect_state = effect_context.effect_slot_.effect_state_.get();
            effect_state->dst_buffers_ = &device_.sample_buffers_;
            effect_state->dst_channel_count_ = device_.channel_count_;
            effect_state->update_device(device_);
            effect_context.effect_slot_.is_props_changed_ = true;
        }

        source_.initialize(effect_count);

        for (int i = 0; i < device_.channel_count_; ++i)
        {
            source_.direct_.channels_[i].reset();

            for (auto& aux : source_.auxes_)
            {
                aux.channels_[i].reset();
            }
        }

        return true;
    }

    void uninitialize()
    {
        for (auto& effect_context : effect_contexts_)
        {
            effect_context.effect_slot_.uninitialize();
        }

        device_.uninitialize();
    }


    void mix_source(
        const int sample_count)
    {
        const auto channel_count = device_.channel_count_;

        for (int chan = 0; chan < channel_count; ++chan)
        {
            for (int i = 0; i < sample_count; ++i)
            {
                device_.resampled_data_[i] = device_.source_samples_[(i * channel_count) + chan];
            }


            auto parms = &source_.direct_.channels_[chan];

            auto samples = apply_filters(
                &parms->low_pass_,
                &parms->high_pass_,
                device_.filtered_data_.data(),
                device_.resampled_data_.data(),
                sample_count,
                source_.direct_.filter_type_);

            parms->current_gains_ = parms->target_gains_;

            MixHelpers::mix(
                samples,
                source_.direct_.channel_count_,
                *source_.direct_.buffers_,
                parms->current_gains_.data(),
                parms->target_gains_.data(),
                0,
                0,
                sample_count);

            for (auto& aux : source_.auxes_)
            {
                if (!aux.buffers_)
                {
                    continue;
                }

                parms = &aux.channels_[chan];

                samples = apply_filters(
                    &parms->low_pass_,
                    &parms->high_pass_,
                    device_.filtered_data_.data(),
                    device_.resampled_data_.data(),
                    sample_count,
                    aux.filter_type_);

                parms->current_gains_ = parms->target_gains_;

                MixHelpers::mix(
                    samples,
                    aux.channel_count_,
                    *aux.buffers_,
                    parms->current_gains_.data(),
                    parms->target_gains_.data(),
                    0,
                    0,
                    sample_count);
            }
        }
    }

    void mix_data(
        const int sample_count,
        const float* src_samples,
        float* dst_samples)
    {
        device_.source_samples_ = src_samples;

        for (int samples_done = 0; samples_done < sample_count; )
        {
            const auto samples_to_do = std::min(sample_count - samples_done, max_sample_buffer_size);

            for (int c = 0; c < device_.channel_count_; ++c)
            {
                std::fill_n(device_.sample_buffers_[c].begin(), samples_to_do, 0.0F);
            }

            update_context_sources();

            for (auto& effect_context : effect_contexts_)
            {
                for (int c = 0; c < max_effect_channels; ++c)
                {
                    std::fill_n(effect_context.effect_slot_.wet_buffer_[c].begin(), samples_to_do, 0.0F);
                }
            }

            // source processing
            mix_source(samples_to_do);

            // effect slot processing
            for (auto& effect_context : effect_contexts_)
            {
                auto state = effect_context.effect_slot_.effect_state_.get();

                state->process(
                    samples_to_do,
                    effect_context.effect_slot_.wet_buffer_,
                    *state->dst_buffers_,
                    state->dst_channel_count_);
            }

            if (dst_samples)
            {
                write_f32(
                    device_.sample_buffers_,
                    dst_samples,
                    samples_done,
                    samples_to_do,
                    device_.channel_count_);
            }

            samples_done += samples_to_do;
        }
    }


private:
    struct ChannelMap
    {
        ChannelId channel_id;
        float angle;
        float elevation;
    }; // ChannelMap

    static constexpr ChannelMap mono_map[1] = {
        {ChannelId::front_center, 0.0F, 0.0F}
    };

    static constexpr ChannelMap stereo_map[2] = {
        {ChannelId::front_left, Math::deg_to_rad(-30.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::front_right, Math::deg_to_rad(30.0F), Math::deg_to_rad(0.0F)}
    };

    static constexpr ChannelMap quad_map[4] = {
        {ChannelId::front_left, Math::deg_to_rad(-45.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::front_right, Math::deg_to_rad(45.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::back_left, Math::deg_to_rad(-135.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::back_right, Math::deg_to_rad(135.0F), Math::deg_to_rad(0.0F)}
    };

    static constexpr ChannelMap x5_1_map[6] = {
        {ChannelId::front_left, Math::deg_to_rad(-30.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::front_right, Math::deg_to_rad(30.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::front_center, Math::deg_to_rad(0.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::lfe, 0.0F, 0.0F},
        {ChannelId::side_left, Math::deg_to_rad(-110.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::side_right, Math::deg_to_rad(110.0F), Math::deg_to_rad(0.0F)}
    };

    static constexpr ChannelMap x6_1_map[7] = {
        {ChannelId::front_left, Math::deg_to_rad(-30.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::front_right, Math::deg_to_rad(30.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::front_center, Math::deg_to_rad(0.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::lfe, 0.0F, 0.0F},
        {ChannelId::back_center, Math::deg_to_rad(180.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::side_left, Math::deg_to_rad(-90.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::side_right, Math::deg_to_rad(90.0F), Math::deg_to_rad(0.0F)}
    };

    static constexpr ChannelMap x7_1_map[8] = {
        {ChannelId::front_left, Math::deg_to_rad(-30.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::front_right, Math::deg_to_rad(30.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::front_center, Math::deg_to_rad(0.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::lfe, 0.0F, 0.0F},
        {ChannelId::back_left, Math::deg_to_rad(-150.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::back_right, Math::deg_to_rad(150.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::side_left, Math::deg_to_rad(-90.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::side_right, Math::deg_to_rad(90.0F), Math::deg_to_rad(0.0F)}
    };


    static const float* apply_filters(
        FilterState* lp_filter,
        FilterState* hp_filter,
        float* dst_samples,
        const float* src_samples,
        const int sample_count,
        const ActiveFilters filter_type)
    {
        switch (filter_type)
        {
        case ActiveFilters::none:
            lp_filter->process_pass_through(sample_count, src_samples);
            hp_filter->process_pass_through(sample_count, src_samples);
            break;

        case ActiveFilters::low_pass:
            lp_filter->process(sample_count, src_samples, dst_samples);
            hp_filter->process_pass_through(sample_count, dst_samples);
            return dst_samples;

        case ActiveFilters::high_pass:
            lp_filter->process_pass_through(sample_count, src_samples);
            hp_filter->process(sample_count, src_samples, dst_samples);
            return dst_samples;

        case ActiveFilters::band_pass:
            for (int i = 0; i < sample_count; )
            {
                float temp[256];

                const auto todo = std::min(256, sample_count - i);

                lp_filter->process(todo, src_samples + i, temp);
                hp_filter->process(todo, temp, dst_samples + i);

                i += todo;
            }

            return dst_samples;
        }

        return src_samples;
    }

    bool calc_effect_slot_params(
        EffectSlot& effect_slot)
    {
        if (!effect_slot.is_props_changed_)
        {
            return false;
        }

        effect_slot.is_props_changed_ = false;
        effect_slot.effect_state_->update(device_, effect_slot, effect_slot.effect_.props_);

        return true;
    }

    bool calc_source_params(
        Source& source)
    {
        if (!source.are_props_changed_)
        {
            return false;
        }

        source.are_props_changed_ = false;

        return true;
    }

    void calc_panning_and_filters(
        const float distance,
        const float* dir,
        const float spread,
        const float dry_gain,
        const float dry_gain_hf,
        const float dry_gain_lf,
        const WetGains& wet_gain,
        const WetGains& wet_gain_lf,
        const WetGains& wet_gain_hf)
    {
        static_cast<void>(distance);
        static_cast<void>(dir);

        const auto frequency = device_.sampling_rate_;
        const ChannelMap* channel_map = nullptr;
        auto channel_count = 0;

        switch (device_.channel_format_)
        {
        case ChannelFormat::mono:
            channel_map = mono_map;
            channel_count = 1;
            break;

        case ChannelFormat::stereo:
            channel_map = stereo_map;
            channel_count = 2;
            break;

        case ChannelFormat::quad:
            channel_map = quad_map;
            channel_count = 4;
            break;

        case ChannelFormat::five_point_one:
            channel_map = x5_1_map;
            channel_count = 6;
            break;

        case ChannelFormat::six_point_one:
            channel_map = x6_1_map;
            channel_count = 7;
            break;

        case ChannelFormat::seven_point_one:
            channel_map = x7_1_map;
            channel_count = 8;
            break;

        case ChannelFormat::none:
        default:
            break;
        }

        // Non-HRTF rendering. Use normal panning to the output.
        for (int c = 0; c < channel_count; ++c)
        {
            AmbiCoeffs coeffs;

            // Special-case LFE
            if (channel_map[c].channel_id == ChannelId::lfe)
            {
                source_.direct_.channels_[c].target_gains_.fill(0.0F);

                const auto idx = device_.get_channel_index(channel_map[c].channel_id);

                if (idx != -1)
                {
                    source_.direct_.channels_[c].target_gains_[idx] = dry_gain;
                }

                for (auto& aux : source_.auxes_)
                {
                    aux.channels_[c].target_gains_.fill(0.0F);
                }

                continue;
            }

            Panning::calc_angle_coeffs(channel_map[c].angle, channel_map[c].elevation, spread, coeffs);

            Panning::compute_panning_gains(
                device_.channel_count_,
                device_.dry_,
                coeffs,
                dry_gain,
                source_.direct_.channels_[c].target_gains_);

            for (int i = 0; i < effect_count_; ++i)
            {
                Panning::compute_panning_gains_bf(
                    max_effect_channels,
                    coeffs,
                    wet_gain[i],
                    source_.auxes_[i].channels_[c].target_gains_);
            }
        }

        const auto hf_scale = SendProps::hp_frequency_reference / frequency;
        const auto lf_scale = SendProps::lp_frequency_reference / frequency;
        auto gain_hf = std::max(dry_gain_hf, 0.001F); // Limit -60dB
        auto gain_lf = std::max(dry_gain_lf, 0.001F);

        source_.direct_.filter_type_ = ActiveFilters::none;

        if (gain_hf != 1.0F)
        {
            source_.direct_.filter_type_ = static_cast<ActiveFilters>(
                static_cast<int>(source_.direct_.filter_type_) | static_cast<int>(ActiveFilters::low_pass));
        }

        if (gain_lf != 1.0F)
        {
            source_.direct_.filter_type_ = static_cast<ActiveFilters>(
                static_cast<int>(source_.direct_.filter_type_) | static_cast<int>(ActiveFilters::high_pass));
        }

        source_.direct_.channels_[0].low_pass_.set_params(
            FilterType::high_shelf,
            gain_hf,
            hf_scale,
            FilterState::calc_rcp_q_from_slope(gain_hf, 1.0F));

        source_.direct_.channels_[0].high_pass_.set_params(
            FilterType::low_shelf,
            gain_lf,
            lf_scale,
            FilterState::calc_rcp_q_from_slope(gain_lf, 1.0F));

        for (int c = 1; c < channel_count; ++c)
        {
            FilterState::copy_params(source_.direct_.channels_[0].low_pass_, source_.direct_.channels_[c].low_pass_);
            FilterState::copy_params(source_.direct_.channels_[0].high_pass_, source_.direct_.channels_[c].high_pass_);
        }

        for (int i = 0; i < effect_count_; ++i)
        {
            auto& aux = source_.auxes_[i];
            gain_hf = std::max(wet_gain_hf[i], 0.001F);
            gain_lf = std::max(wet_gain_lf[i], 0.001F);

            aux.filter_type_ = ActiveFilters::none;

            if (gain_hf != 1.0F)
            {
                aux.filter_type_ = static_cast<ActiveFilters>(
                    static_cast<int>(aux.filter_type_) | static_cast<int>(ActiveFilters::low_pass));
            }

            if (gain_lf != 1.0F)
            {
                aux.filter_type_ = static_cast<ActiveFilters>(
                    static_cast<int>(aux.filter_type_) | static_cast<int>(ActiveFilters::high_pass));
            }

            aux.channels_[0].low_pass_.set_params(
                FilterType::high_shelf,
                gain_hf,
                hf_scale,
                FilterState::calc_rcp_q_from_slope(gain_hf, 1.0F));

            aux.channels_[0].high_pass_.set_params(
                FilterType::low_shelf,
                gain_lf,
                lf_scale,
                FilterState::calc_rcp_q_from_slope(gain_lf, 1.0F));

            for (int c = 1; c < channel_count; ++c)
            {
                FilterState::copy_params(aux.channels_[0].low_pass_, aux.channels_[c].low_pass_);
                FilterState::copy_params(aux.channels_[0].high_pass_, aux.channels_[c].high_pass_);
            }
        }
    }

    void calc_non_attn_source_params()
    {
        source_.direct_.buffers_ = &device_.sample_buffers_;
        source_.direct_.channel_count_ = device_.channel_count_;

        for (int i = 0; i < effect_count_; ++i)
        {
            if (effect_contexts_[i].effect_slot_.effect_.type_ == EffectType::null)
            {
                source_.auxes_[i].buffers_ = nullptr;
                source_.auxes_[i].channel_count_ = 0;
            }
            else
            {
                source_.auxes_[i].buffers_ = &effect_contexts_[i].effect_slot_.wet_buffer_;
                source_.auxes_[i].channel_count_ = max_effect_channels;
            }
        }

        // Calculate gains
        const auto dry_gain = std::min(source_.direct_.props_.gain_, max_mix_gain);
        const auto dry_gain_hf = source_.direct_.props_.gain_hf_;
        const auto dry_gain_lf = source_.direct_.props_.gain_lf_;

        constexpr float dir[3] = {0.0F, 0.0F, -1.0F};

        auto wet_gain = WetGains{};
        auto wet_gain_hf = WetGains{};
        auto wet_gain_lf = WetGains{};

        for (int i = 0; i < effect_count_; ++i)
        {
            wet_gain[i] = std::min(source_.auxes_[i].props_.gain_, max_mix_gain);
            wet_gain_hf[i] = source_.auxes_[i].props_.gain_hf_;
            wet_gain_lf[i] = source_.auxes_[i].props_.gain_lf_;
        }

        calc_panning_and_filters(
            0.0F,
            dir,
            0.0F,
            dry_gain,
            dry_gain_hf,
            dry_gain_lf,
            wet_gain,
            wet_gain_lf,
            wet_gain_hf);
    }

    void update_context_sources()
    {
        auto is_props_updated = false;

        for (auto& effect_context : effect_contexts_)
        {
            is_props_updated |= calc_effect_slot_params(effect_context.effect_slot_);
        }

        is_props_updated |= calc_source_params(source_);

        if (is_props_updated)
        {
            calc_non_attn_source_params();
        }
    }

    static void write_f32(
        const SampleBuffers& src_buffers,
        float* dst_buffer,
        const int offset,
        const int sample_count,
        const int channel_count)
    {
        for (int j = 0; j < channel_count; ++j)
        {
            const auto& src_buffer = src_buffers[j];
            auto out = &dst_buffer[(offset * channel_count) + j];

            for (int i = 0; i < sample_count; ++i)
            {
                out[i * channel_count] = src_buffer[i];
            }
        }
    }
}; // Impl

// Api::Impl
// ==========================================================================


// ==========================================================================
// Api

struct ApiErrorMessages
{
    static constexpr auto NoError = "";
    static constexpr auto AllocateImpl = "Failed to allocate implementaion class.";
    static constexpr auto NotInitialized = "Not initialized.";
    static constexpr auto EffectIndexOutOfRange = "Effect index is out of range.";
    static constexpr auto NoSrcSamples = "No source samples.";
    static constexpr auto NoDstSamples = "No destination samples.";
}; // ApiErrorMessages


Api::Api()
    :
    pimpl_{},
    error_message_{ApiErrorMessages::NoError}
{
}

Api::~Api()
{
    uninitialize();
}

bool Api::initialize(
    const ChannelFormat channel_format,
    const int sampling_rate,
    const int effect_count)
{
    uninitialize();

    pimpl_.reset(new (std::nothrow) Impl{});

    if (!pimpl_)
    {
        error_message_ = ApiErrorMessages::AllocateImpl;
        return false;
    }

    const auto initialize_result = pimpl_->initialize(channel_format, sampling_rate, effect_count);

    if (!initialize_result)
    {
        error_message_ = pimpl_->error_message_;
        uninitialize();
    }

    return initialize_result;
}

bool Api::is_initialized() const
{
    return pimpl_ != nullptr;
}

int Api::get_sampling_rate() const
{
    if (!is_initialized())
    {
        error_message_ = ApiErrorMessages::NotInitialized;
        return 0;
    }

    return pimpl_->device_.sampling_rate_;
}

ChannelFormat Api::get_channel_format() const
{
    if (!is_initialized())
    {
        error_message_ = ApiErrorMessages::NotInitialized;
        return ChannelFormat::none;
    }

    return pimpl_->device_.channel_format_;
}

int Api::get_channel_count() const
{
    if (!is_initialized())
    {
        error_message_ = ApiErrorMessages::NotInitialized;
        return false;
    }

    return pimpl_->device_.channel_count_;
}

int Api::get_effect_count() const
{
    if (!is_initialized())
    {
        error_message_ = ApiErrorMessages::NotInitialized;
        return false;
    }

    return pimpl_->effect_count_;
}

bool Api::get_effect(
    const int effect_index,
    Effect& effect) const
{
    if (!is_initialized())
    {
        error_message_ = ApiErrorMessages::NotInitialized;
        return false;
    }

    if (effect_index < 0 || effect_index >= pimpl_->effect_count_)
    {
        error_message_ = ApiErrorMessages::EffectIndexOutOfRange;
        return false;
    }

    effect = pimpl_->effect_contexts_[effect_index].effect_slot_.effect_;

    return true;
}

bool Api::get_deferred_effect(
    const int effect_index,
    Effect& effect) const
{
    if (!is_initialized())
    {
        error_message_ = ApiErrorMessages::NotInitialized;
        return false;
    }

    if (effect_index < 0 || effect_index >= pimpl_->effect_count_)
    {
        error_message_ = ApiErrorMessages::EffectIndexOutOfRange;
        return false;
    }

    effect = pimpl_->effect_contexts_[effect_index].deferred_effect_;

    return true;
}

bool Api::set_effect_type(
    const int effect_index,
    const EffectType effect_type)
{
    if (!is_initialized())
    {
        error_message_ = ApiErrorMessages::NotInitialized;
        return false;
    }

    if (effect_index < 0 || effect_index >= pimpl_->effect_count_)
    {
        error_message_ = ApiErrorMessages::EffectIndexOutOfRange;
        return false;
    }

    pimpl_->effect_contexts_[effect_index].deferred_effect_.set_type_and_defaults(effect_type);

    return true;
}

bool Api::set_effect_props(
    const int effect_index,
    const EffectProps& effect_props)
{
    if (!is_initialized())
    {
        error_message_ = ApiErrorMessages::NotInitialized;
        return false;
    }

    if (effect_index < 0 || effect_index >= pimpl_->effect_count_)
    {
        error_message_ = ApiErrorMessages::EffectIndexOutOfRange;
        return false;
    }

    pimpl_->effect_contexts_[effect_index].deferred_effect_.props_ = effect_props;

    return true;
}

bool Api::set_effect(
    const int effect_index,
    const Effect& effect)
{
    if (!is_initialized())
    {
        error_message_ = ApiErrorMessages::NotInitialized;
        return false;
    }

    if (effect_index < 0 || effect_index >= pimpl_->effect_count_)
    {
        error_message_ = ApiErrorMessages::EffectIndexOutOfRange;
        return false;
    }

    pimpl_->effect_contexts_[effect_index].deferred_effect_ = effect;

    return false;
}

bool Api::get_send_props(
    const int effect_index,
    SendProps& send_props) const
{
    if (!is_initialized())
    {
        error_message_ = ApiErrorMessages::NotInitialized;
        return false;
    }

    if (effect_index >= pimpl_->effect_count_)
    {
        error_message_ = ApiErrorMessages::EffectIndexOutOfRange;
        return false;
    }

    const auto& props = (
        effect_index < 0 ?
        pimpl_->source_.direct_.props_ :
        pimpl_->source_.auxes_[effect_index].props_);

    send_props = props;

    return true;
}

bool Api::get_deferred_send_props(
    const int effect_index,
    SendProps& send_props) const
{
    if (!is_initialized())
    {
        error_message_ = ApiErrorMessages::NotInitialized;
        return false;
    }

    if (effect_index >= pimpl_->effect_count_)
    {
        error_message_ = ApiErrorMessages::EffectIndexOutOfRange;
        return false;
    }

    const auto& props = (
        effect_index < 0 ?
        pimpl_->source_.direct_.deferred_props_ :
        pimpl_->source_.auxes_[effect_index].deferred_props_);

    send_props = props;

    return true;
}

bool Api::set_send_props(
    const int effect_index,
    const SendProps& send_props)
{
    if (!is_initialized())
    {
        error_message_ = ApiErrorMessages::NotInitialized;
        return false;
    }

    if (effect_index >= pimpl_->effect_count_)
    {
        error_message_ = ApiErrorMessages::EffectIndexOutOfRange;
        return false;
    }

    auto& props = (
        effect_index < 0 ?
        pimpl_->source_.direct_.deferred_props_ :
        pimpl_->source_.auxes_[effect_index].props_);

    props = send_props;

    return true;
}

bool Api::apply_changes()
{
    if (!is_initialized())
    {
        error_message_ = ApiErrorMessages::NotInitialized;
        return false;
    }

    // Effects
    //
    for (auto& effect_context : pimpl_->effect_contexts_)
    {
        effect_context.deferred_effect_.normalize();

        if (!Effect::are_equal(effect_context.deferred_effect_, effect_context.effect_slot_.effect_))
        {
            effect_context.effect_slot_.set_effect(pimpl_->device_, effect_context.deferred_effect_);
        }
    }

    // Direct send
    //
    auto& source = pimpl_->source_;
    auto& direct_send = source.direct_;
    direct_send.deferred_props_.normalize();

    if (!SendProps::are_equal(direct_send.deferred_props_, direct_send.props_))
    {
        source.are_props_changed_ = true;
        direct_send.props_ = direct_send.deferred_props_;
    }

    // Aux sends
    //
    for (auto& aux_send : source.auxes_)
    {
        aux_send.deferred_props_.normalize();

        if (!SendProps::are_equal(aux_send.props_, aux_send.deferred_props_))
        {
            source.are_props_changed_ = true;
        }
    }

    return true;
}

bool Api::mix(
    const int sample_count,
    const float* src_samples,
    float* dst_samples)
{
    if (!is_initialized())
    {
        error_message_ = ApiErrorMessages::NotInitialized;
        return false;
    }

    if (sample_count == 0)
    {
        return true;
    }

    if (!src_samples)
    {
        error_message_ = ApiErrorMessages::NoSrcSamples;
        return false;
    }

    if (!dst_samples)
    {
        error_message_ = ApiErrorMessages::NoDstSamples;
        return false;
    }

    const auto channel_count = pimpl_->device_.channel_count_;

    auto buffer_offset = 0;
    auto remain_count = sample_count;

    while (remain_count > 0)
    {
        const auto count = std::min(remain_count, max_sample_buffer_size);

        pimpl_->mix_data(count, &src_samples[buffer_offset], &dst_samples[buffer_offset]);

        buffer_offset += count * channel_count;
        remain_count -= count;
    }

    return true;
}

void Api::uninitialize()
{
    pimpl_ = nullptr;
}

const char* Api::get_error_message() const
{
    return pimpl_->error_message_;
}

int Api::get_min_sampling_rate()
{
    return min_sampling_rate;
}

int Api::get_max_sampling_rate()
{
    return max_sampling_rate;
}

int Api::get_min_effect_count()
{
    return min_effects;
}

int Api::get_max_effect_count()
{
    return max_effects;
}

ChannelFormat Api::channel_count_to_channel_format(
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
// Api
// ==========================================================================


// ==========================================================================
// Effects

class NullEffectState :
    public EffectState
{
public:
    NullEffectState()
        :
        EffectState{}
    {
    }

    virtual ~NullEffectState()
    {
    }


protected:
    void do_construct() final
    {
    }

    void do_destruct() final
    {
    }

    void do_update_device(
        Device& device) final
    {
        static_cast<void>(device);
    }

    void do_update(
        Device& device,
        const EffectSlot& effect_slot,
        const EffectProps& effect_props) final
    {
        static_cast<void>(device);
        static_cast<void>(effect_slot);
        static_cast<void>(effect_props);
    }

    void do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final
    {
        static_cast<void>(sample_count);
        static_cast<void>(src_samples);
        static_cast<void>(dst_samples);
        static_cast<void>(channel_count);
    }
}; // NullEffectState


EffectState* EffectStateFactory::create_null()
{
    return create<NullEffectState>();
}


class ChorusEffectState :
    public EffectState
{
public:
    ChorusEffectState()
        :
        EffectState{},
        sample_buffers_{},
        buffer_length_{},
        offset_{},
        lfo_range_{},
        lfo_scale_{},
        lfo_disp_{},
        sides_gains_{},
        waveform_{},
        delay_{},
        depth_{},
        feedback_{}
    {
    }

    virtual ~ChorusEffectState()
    {
    }


protected:
    void do_construct() final
    {
        buffer_length_ = 0;

        for (auto& buffer : sample_buffers_)
        {
            buffer = SampleBuffer{};
        }

        offset_ = 0;
        lfo_range_ = 1;
        waveform_ = Waveform::triangle;
    }

    void do_destruct() final
    {
        for (auto& buffer : sample_buffers_)
        {
            buffer = SampleBuffer{};
        }
    }

    void do_update_device(
        Device& device) final
    {
        auto max_len = static_cast<int>(EffectProps::Chorus::max_delay * 2.0F * device.sampling_rate_) + 1;

        max_len = Math::next_power_of_2(max_len);

        if (max_len != buffer_length_)
        {
            sample_buffers_[0].resize(max_len);
            sample_buffers_[1].resize(max_len);

            buffer_length_ = max_len;
        }

        for (auto& buffer : sample_buffers_)
        {
            std::fill(buffer.begin(), buffer.end(), 0.0F);
        }
    }

    void do_update(
        Device& device,
        const EffectSlot& effect_slot,
        const EffectProps& effect_props) final
    {
        static_cast<void>(effect_slot);

        const auto frequency = static_cast<float>(device.sampling_rate_);

        switch (effect_props.chorus_.waveform_)
        {
        case EffectProps::Chorus::waveform_triangle:
            waveform_ = Waveform::triangle;
            break;

        case EffectProps::Chorus::waveform_sinusoid:
            waveform_ = Waveform::sinusoid;
            break;
        }

        feedback_ = effect_props.chorus_.feedback_;
        delay_ = static_cast<int>(effect_props.chorus_.delay_ * frequency);

        // The LFO depth is scaled to be relative to the sample delay.
        depth_ = effect_props.chorus_.depth_ * delay_;

        AmbiCoeffs coeffs;

        // Gains for left and right sides
        Panning::calc_angle_coeffs(-Math::pi_2, 0.0F, 0.0F, coeffs);
        Panning::compute_panning_gains(device.channel_count_, device.dry_, coeffs, 1.0F, sides_gains_[0]);
        Panning::calc_angle_coeffs(Math::pi_2, 0.0F, 0.0F, coeffs);
        Panning::compute_panning_gains(device.channel_count_, device.dry_, coeffs, 1.0F, sides_gains_[1]);

        const auto phase = effect_props.chorus_.phase_;
        const auto rate = effect_props.chorus_.rate_;

        if (!(rate > 0.0F))
        {
            lfo_scale_ = 0.0F;
            lfo_range_ = 1;
            lfo_disp_ = 0;
        }
        else
        {
            // Calculate LFO coefficient
            lfo_range_ = static_cast<int>(frequency / rate + 0.5F);

            switch (waveform_)
            {
            case Waveform::triangle:
                lfo_scale_ = 4.0F / lfo_range_;
                break;

            case Waveform::sinusoid:
                lfo_scale_ = Math::tau / lfo_range_;
                break;
            }

            // Calculate lfo phase displacement
            if (phase >= 0)
            {
                lfo_disp_ = static_cast<int>(lfo_range_ * (phase / 360.0F));
            }
            else
            {
                lfo_disp_ = static_cast<int>(lfo_range_ * ((360 + phase) / 360.0F));
            }
        }
    }

    void do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final
    {
        auto& left_buf = sample_buffers_[0];
        auto& right_buf = sample_buffers_[1];
        const auto buf_mask = buffer_length_ - 1;

        for (int base = 0; base < sample_count; )
        {
            float temps[128][2];
            int mod_delays[2][128];
            const auto todo = std::min(128, sample_count - base);

            switch (waveform_)
            {
            case Waveform::triangle:
                GetTriangleDelays(
                    mod_delays[0],
                    offset_ % lfo_range_,
                    lfo_range_,
                    lfo_scale_,
                    depth_,
                    delay_,
                    todo);

                GetTriangleDelays(
                    mod_delays[1],
                    (offset_ + lfo_disp_) % lfo_range_,
                    lfo_range_,
                    lfo_scale_,
                    depth_,
                    delay_,
                    todo);

                break;

            case Waveform::sinusoid:
                GetSinusoidDelays(
                    mod_delays[0],
                    offset_ % lfo_range_,
                    lfo_range_,
                    lfo_scale_,
                    depth_,
                    delay_,
                    todo);

                GetSinusoidDelays(
                    mod_delays[1],
                    (offset_ + lfo_disp_) % lfo_range_,
                    lfo_range_,
                    lfo_scale_,
                    depth_,
                    delay_,
                    todo);

                break;
            }

            for (int i = 0; i < todo; ++i)
            {
                left_buf[offset_ & buf_mask] = src_samples[0][base + i];
                temps[i][0] = left_buf[(offset_ - mod_delays[0][i]) & buf_mask] * feedback_;
                left_buf[offset_ & buf_mask] += temps[i][0];

                right_buf[offset_ & buf_mask] = src_samples[0][base + i];
                temps[i][1] = right_buf[(offset_ - mod_delays[1][i]) & buf_mask] * feedback_;
                right_buf[offset_ & buf_mask] += temps[i][1];

                offset_ += 1;
            }

            for (int c = 0; c < channel_count; ++c)
            {
                auto channel_gain = sides_gains_[0][c];

                if (std::abs(channel_gain) > silence_threshold_gain)
                {
                    for (int i = 0; i < todo; ++i)
                    {
                        dst_samples[c][i + base] += temps[i][0] * channel_gain;
                    }
                }

                channel_gain = sides_gains_[1][c];

                if (std::abs(channel_gain) > silence_threshold_gain)
                {
                    for (int i = 0; i < todo; ++i)
                    {
                        dst_samples[c][i + base] += temps[i][1] * channel_gain;
                    }
                }
            }

            base += todo;
        }
    }


private:
    enum class Waveform
    {
        triangle = EffectProps::Chorus::waveform_triangle,
        sinusoid = EffectProps::Chorus::waveform_sinusoid
    }; // Waveform


    using SampleBuffer = EffectSampleBuffer;
    using SampleBuffers = std::array<SampleBuffer, 2>;

    using SidesGains = std::array<Gains, 2>;


    SampleBuffers sample_buffers_;
    int buffer_length_;
    int offset_;
    int lfo_range_;
    float lfo_scale_;
    int lfo_disp_;

    // Gains for left and right sides
    SidesGains sides_gains_;

    // effect parameters
    Waveform waveform_;
    int delay_;
    float depth_;
    float feedback_;


    static void GetTriangleDelays(
        int* delays,
        int offset,
        const int lfo_range,
        const float lfo_scale,
        const float depth,
        const int delay,
        const int todo)
    {
        for (int i = 0; i < todo; ++i)
        {
            delays[i] = static_cast<int>((1.0F - std::abs(2.0F - (lfo_scale * offset))) * depth) + delay;
            offset = (offset + 1) % lfo_range;
        }
    }

    static void GetSinusoidDelays(
        int* delays,
        int offset,
        const int lfo_range,
        const float lfo_scale,
        const float depth,
        const int delay,
        const int todo)
    {
        for (int i = 0; i < todo; ++i)
        {
            delays[i] = static_cast<int>(std::sin(lfo_scale * offset) * depth) + delay;
            offset = (offset + 1) % lfo_range;
        }
    }
}; // ChorusEffectState


EffectState* EffectStateFactory::create_chorus()
{
    return create<ChorusEffectState>();
}


class CompressorEffectState :
    public EffectState
{
public:
    CompressorEffectState()
        :
        EffectState{},
        channels_gains_{},
        is_enabled_{},
        attack_rate_{},
        release_rate_{},
        gain_control_{}
    {
    }

    virtual ~CompressorEffectState()
    {
    }


protected:
    void do_construct() final
    {
        is_enabled_ = true;
        attack_rate_ = 0.0F;
        release_rate_ = 0.0F;
        gain_control_ = 1.0F;
    }

    void do_destruct() final
    {
    }

    void do_update_device(
        Device& device) final
    {
        const auto attackTime = device.sampling_rate_ * 0.2F; // 200ms Attack
        const auto releaseTime = device.sampling_rate_ * 0.4F; // 400ms Release

        attack_rate_ = 1.0F / attackTime;
        release_rate_ = 1.0F / releaseTime;
    }

    void do_update(
        Device& device,
        const EffectSlot& effect_slot,
        const EffectProps& effect_props) final
    {
        static_cast<void>(effect_slot);

        is_enabled_ = effect_props.compressor_.on_off_;

        dst_buffers_ = &device.sample_buffers_;
        dst_channel_count_ = device.channel_count_;

        for (int i = 0; i < 4; ++i)
        {
            Panning::compute_first_order_gains(
                device.channel_count_,
                device.foa_,
                mat4f_identity.m_[i],
                1.0F,
                channels_gains_[i]);
        }
    }

    void do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final
    {
        for (int base = 0; base < sample_count; )
        {
            float temps[64][4];

            const auto td = std::min(64, sample_count - base);

            // Load samples into the temp buffer first.
            for (int j = 0; j < 4; ++j)
            {
                for (int i = 0; i < td; ++i)
                {
                    temps[i][j] = src_samples[j][i + base];
                }
            }

            if (is_enabled_)
            {
                for (int i = 0; i < td; ++i)
                {
                    // Roughly calculate the maximum amplitude from the 4-channel
                    // signal, and attack or release the gain control to reach it.
                    auto amplitude = std::abs(temps[i][0]);

                    amplitude = std::max(amplitude + std::abs(temps[i][1]),
                        std::max(amplitude + std::abs(temps[i][2]),
                            amplitude + std::abs(temps[i][3])));

                    if (amplitude > gain_control_)
                    {
                        gain_control_ = std::min(gain_control_ + attack_rate_, amplitude);
                    }
                    else if (amplitude < gain_control_)
                    {
                        gain_control_ = std::max(gain_control_ - release_rate_, amplitude);
                    }

                    // Apply the inverse of the gain control to normalize/compress
                    // the volume.
                    const auto output = 1.0F / Math::clamp(gain_control_, 0.5F, 2.0F);

                    for (int j = 0; j < 4; ++j)
                    {
                        temps[i][j] *= output;
                    }
                }
            }
            else
            {
                for (int i = 0; i < td; ++i)
                {
                    // Same as above, except the amplitude is forced to 1. This
                    // helps ensure smooth gain changes when the compressor is
                    // turned on and off.

                    const auto amplitude = 1.0F;

                    if (amplitude > gain_control_)
                    {
                        gain_control_ = std::min(gain_control_ + attack_rate_, amplitude);
                    }
                    else if (amplitude < gain_control_)
                    {
                        gain_control_ = std::max(gain_control_ - release_rate_, amplitude);
                    }

                    const auto output = 1.0F / Math::clamp(gain_control_, 0.5F, 2.0F);

                    for (int j = 0; j < 4; ++j)
                    {
                        temps[i][j] *= output;
                    }
                }
            }

            // Now mix to the output.
            for (int j = 0; j < 4; ++j)
            {
                for (int k = 0; k < channel_count; ++k)
                {
                    const auto channel_gain = channels_gains_[j][k];

                    if (!(std::abs(channel_gain) > silence_threshold_gain))
                    {
                        continue;
                    }

                    for (int i = 0; i < td; ++i)
                    {
                        dst_samples[k][base + i] += channel_gain * temps[i][j];
                    }
                }
            }

            base += td;
        }
    }


private:
    using ChannelsGains = std::array<Gains, max_effect_channels>;


    // Effect gains for each channel
    ChannelsGains channels_gains_;

    // Effect parameters
    bool is_enabled_;
    float attack_rate_;
    float release_rate_;
    float gain_control_;
}; // CompressorEffectState


EffectState* EffectStateFactory::create_compressor()
{
    return create<CompressorEffectState>();
}


class DedicatedEffectState :
    public EffectState
{
public:
    DedicatedEffectState()
        :
        EffectState{},
        gains_{}
    {
    }

    virtual ~DedicatedEffectState()
    {
    }


protected:
    void do_construct() final
    {
        gains_.fill(0.0F);
    }

    void do_destruct() final
    {
    }

    void do_update_device(
        Device& device) final
    {
        static_cast<void>(device);
    }

    void do_update(
        Device& device,
        const EffectSlot& effect_slot,
        const EffectProps& effect_props)
    {
        gains_.fill(0.0F);

        const auto gain = effect_props.dedicated_.gain_;

        if (effect_slot.effect_.type_ == EffectType::dedicated_low_frequency)
        {
            const auto idx = device.get_channel_index(ChannelId::lfe);

            if (idx != -1)
            {
                dst_buffers_ = &device.sample_buffers_;
                dst_channel_count_ = device.channel_count_;
                gains_[idx] = gain;
            }
        }
        else if (effect_slot.effect_.type_ == EffectType::dedicated_dialog)
        {
            const auto idx = device.get_channel_index(ChannelId::front_center);

            // Dialog goes to the front-center speaker if it exists, otherwise it
            // plays from the front-center location.

            if (idx != -1)
            {
                dst_buffers_ = &device.sample_buffers_;
                dst_channel_count_ = device.channel_count_;
                gains_[idx] = gain;
            }
            else
            {
                AmbiCoeffs coeffs;

                Panning::calc_angle_coeffs(0.0F, 0.0F, 0.0F, coeffs);

                dst_buffers_ = &device.sample_buffers_;
                dst_channel_count_ = device.channel_count_;

                Panning::compute_panning_gains(device.channel_count_, device.dry_, coeffs, gain, gains_);
            }
        }
    }

    void do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final
    {
        for (int c = 0; c < channel_count; ++c)
        {
            const auto gain = gains_[c];

            if (!(std::abs(gain) > silence_threshold_gain))
            {
                continue;
            }

            for (int i = 0; i < sample_count; ++i)
            {
                dst_samples[c][i] += src_samples[0][i] * gain;
            }
        }
    }


private:
    Gains gains_;
}; // DedicatedEffectState


EffectState* EffectStateFactory::create_dedicated()
{
    return create<DedicatedEffectState>();
}


class DistortionEffectState :
    public EffectState
{
public:
    DistortionEffectState()
        :
        EffectState{},
        gains_{},
        low_pass_{},
        band_pass_{},
        attenuation_{},
        edge_coeff_{}
    {
    }

    virtual ~DistortionEffectState()
    {
    }


protected:
    void do_construct() final
    {
        low_pass_.clear();
        band_pass_.clear();
    }

    void do_destruct() final
    {
    }

    void do_update_device(
        Device& device) final
    {
        static_cast<void>(device);
    }

    void do_update(
        Device& device,
        const EffectSlot& effect_slot,
        const EffectProps& effect_props) final
    {
        static_cast<void>(effect_slot);

        const auto frequency = static_cast<float>(device.sampling_rate_);

        // Store distorted signal attenuation settings.
        attenuation_ = effect_props.distortion_.gain_;

        // Store waveshaper edge settings.
        auto edge = std::sin(effect_props.distortion_.edge_ * (Math::pi_2));
        edge = std::min(edge, 0.99F);
        edge_coeff_ = 2.0F * edge / (1.0F - edge);

        auto cutoff = effect_props.distortion_.low_pass_cutoff_;

        // Bandwidth value is constant in octaves.
        auto bandwidth = (cutoff / 2.0F) / (cutoff * 0.67F);

        // Multiply sampling frequency by the amount of oversampling done during
        // processing.
        low_pass_.set_params(
            FilterType::low_pass,
            1.0F,
            cutoff / (frequency * 4.0F),
            FilterState::calc_rcp_q_from_bandwidth(cutoff / (frequency * 4.0F), bandwidth));

        cutoff = effect_props.distortion_.eq_center_;

        // Convert bandwidth in Hz to octaves.
        bandwidth = effect_props.distortion_.eq_bandwidth_ / (cutoff * 0.67F);

        band_pass_.set_params(
            FilterType::band_pass,
            1.0F,
            cutoff / (frequency * 4.0F),
            FilterState::calc_rcp_q_from_bandwidth(cutoff / (frequency * 4.0F), bandwidth));

        Panning::compute_ambient_gains(
            device.channel_count_,
            device.dry_,
            1.0F,
            gains_);
    }

    void do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final
    {
        const auto fc = edge_coeff_;

        for (int base = 0; base < sample_count; )
        {
            float buffer[2][64 * 4];

            const auto td = std::min(64, sample_count - base);

            // Perform 4x oversampling to avoid aliasing. Oversampling greatly
            // improves distortion quality and allows to implement lowpass and
            // bandpass filters using high frequencies, at which classic IIR
            // filters became unstable.

            // Fill oversample buffer using zero stuffing.
            for (int it = 0; it < td; ++it)
            {
                // Multiply the sample by the amount of oversampling to maintain
                // the signal's power.

                buffer[0][(it * 4) + 0] = src_samples[0][it + base] * 4.0F;
                buffer[0][(it * 4) + 1] = 0.0F;
                buffer[0][(it * 4) + 2] = 0.0F;
                buffer[0][(it * 4) + 3] = 0.0F;
            }

            // First step, do lowpass filtering of original signal. Additionally
            // perform buffer interpolation and lowpass cutoff for oversampling
            // (which is fortunately first step of distortion). So combine three
            // operations into the one.
            low_pass_.process(td * 4, buffer[0], buffer[1]);

            // Second step, do distortion using waveshaper function to emulate
            // signal processing during tube overdriving. Three steps of
            // waveshaping are intended to modify waveform without boost/clipping/
            // attenuation process.
            for (int it = 0; it < td * 4; it++)
            {
                auto smp = buffer[1][it];

                smp = (1.0F + fc) * smp / (1.0F + (fc * std::abs(smp)));
                smp = (1.0F + fc) * smp / (1.0F + (fc * std::abs(smp))) * -1.0F;
                smp = (1.0F + fc) * smp / (1.0F + (fc * std::abs(smp)));

                buffer[0][it] = smp;
            }

            // Third step, do bandpass filtering of distorted signal.
            band_pass_.process(td * 4, buffer[0], buffer[1]);

            for (int kt = 0; kt < channel_count; ++kt)
            {
                // Fourth step, final, do attenuation and perform decimation,
                // store only one sample out of 4.

                const auto gain = gains_[kt] * attenuation_;

                if (!(std::abs(gain) > silence_threshold_gain))
                {
                    continue;
                }

                for (int it = 0; it < td; ++it)
                {
                    dst_samples[kt][base + it] += gain * buffer[1][it * 4];
                }
            }

            base += td;
        }
    }


private:
    // Effect gains for each channel
    Gains gains_;

    // Effect parameters
    FilterState low_pass_;
    FilterState band_pass_;
    float attenuation_;
    float edge_coeff_;
}; // DistortionEffectState


EffectState* EffectStateFactory::create_distortion()
{
    return create<DistortionEffectState>();
}


class EchoEffectState :
    public EffectState
{
public:
    struct Tap
    {
        int delay;
    };


    EchoEffectState()
        :
        EffectState{},
        sample_buffer_{},
        buffer_length_{},
        taps_{},
        offset_{},
        taps_gains_{},
        feed_gain_{},
        filter_{}
    {
    }

    virtual ~EchoEffectState()
    {
    }


protected:
    void do_construct() final
    {
        buffer_length_ = 0;
        sample_buffer_ = EffectSampleBuffer{};

        taps_[0].delay = 0;
        taps_[1].delay = 0;
        offset_ = 0;

        filter_.clear();
    }

    void do_destruct() final
    {
        sample_buffer_ = EffectSampleBuffer{};
    }

    void do_update_device(
        Device& device) final
    {
        // Use the next power of 2 for the buffer length, so the tap offsets can be
        // wrapped using a mask instead of a modulo
        auto maxlen = static_cast<int>(EffectProps::Echo::max_delay * device.sampling_rate_) + 1;
        maxlen += static_cast<int>(EffectProps::Echo::max_lr_delay * device.sampling_rate_) + 1;
        maxlen = Math::next_power_of_2(maxlen);

        if (maxlen != buffer_length_)
        {
            sample_buffer_.resize(maxlen);
            buffer_length_ = maxlen;
        }

        std::fill(sample_buffer_.begin(), sample_buffer_.end(), 0.0F);
    }

    void do_update(
        Device& device,
        const EffectSlot& effect_slot,
        const EffectProps& effect_props) final
    {
        static_cast<void>(effect_slot);

        AmbiCoeffs coeffs;
        float effect_gain, lrpan, spread;

        const auto frequency = device.sampling_rate_;

        taps_[0].delay = static_cast<int>(effect_props.echo_.delay_ * frequency) + 1;
        taps_[1].delay = static_cast<int>(effect_props.echo_.lr_delay_ * frequency);
        taps_[1].delay += taps_[0].delay;

        spread = effect_props.echo_.spread_;

        if (spread < 0.0F)
        {
            lrpan = -1.0F;
        }
        else
        {
            lrpan = 1.0F;
        }

        // Convert echo spread (where 0 = omni, +/-1 = directional) to coverage
        // spread (where 0 = point, tau = omni).
        spread = std::asin(1.0F - std::abs(spread)) * 4.0F;

        feed_gain_ = effect_props.echo_.feedback_;

        effect_gain = std::max(1.0F - effect_props.echo_.damping_, 0.0625F); // Limit -24dB

        filter_.set_params(
            FilterType::high_shelf,
            effect_gain,
            SendProps::lp_frequency_reference / frequency,
            FilterState::calc_rcp_q_from_slope(effect_gain, 1.0F));

        effect_gain = 1.0F;

        // First tap panning
        Panning::calc_angle_coeffs(-Math::pi_2 * lrpan, 0.0F, spread, coeffs);
        Panning::compute_panning_gains(device.channel_count_, device.dry_, coeffs, effect_gain, taps_gains_[0]);

        // Second tap panning
        Panning::calc_angle_coeffs(Math::pi_2 * lrpan, 0.0F, spread, coeffs);
        Panning::compute_panning_gains(device.channel_count_, device.dry_, coeffs, effect_gain, taps_gains_[1]);
    }

    void do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final
    {
        const auto mask = buffer_length_ - 1;
        const auto tap1 = taps_[0].delay;
        const auto tap2 = taps_[1].delay;
        float x[2] = {filter_.x_[0], filter_.x_[1],};
        float y[2] = {filter_.y_[0], filter_.y_[1],};

        for (int base = 0; base < sample_count; )
        {
            float temps[128][2];

            const auto td = std::min(128, sample_count - base);

            for (int i = 0; i < td; ++i)
            {
                // First tap
                temps[i][0] = sample_buffer_[(offset_ - tap1) & mask];

                // Second tap
                temps[i][1] = sample_buffer_[(offset_ - tap2) & mask];

                // Apply damping and feedback gain to the second tap, and mix in the
                // new sample
                auto in = temps[i][1] + src_samples[0][i + base];

                auto out = (in * filter_.b0_) +
                    (x[0] * filter_.b1_) + (x[1] * filter_.b2_) -
                    (y[0] * filter_.a1_) - (y[1] * filter_.a2_);

                x[1] = x[0];
                x[0] = in;

                y[1] = y[0];
                y[0] = out;

                sample_buffer_[offset_&mask] = out * feed_gain_;

                offset_ += 1;
            }

            for (int k = 0; k < channel_count; ++k)
            {
                auto channel_gain = taps_gains_[0][k];

                if (std::abs(channel_gain) > silence_threshold_gain)
                {
                    for (int i = 0; i < td; ++i)
                    {
                        dst_samples[k][i + base] += temps[i][0] * channel_gain;
                    }
                }

                channel_gain = taps_gains_[1][k];

                if (std::abs(channel_gain) > silence_threshold_gain)
                {
                    for (int i = 0; i < td; ++i)
                    {
                        dst_samples[k][i + base] += temps[i][1] * channel_gain;
                    }
                }
            }

            base += td;
        }

        filter_.x_[0] = x[0];
        filter_.x_[1] = x[1];
        filter_.y_[0] = y[0];
        filter_.y_[1] = y[1];
    }


private:
    using Taps = std::array<Tap, 2>;
    using TapsGains = std::array<Gains, 2>;


    EffectSampleBuffer sample_buffer_;
    int buffer_length_;

    // The echo is two tap. The delay is the number of samples from before the
    // current offset
    Taps taps_;

    int offset_;

    // The panning gains for the two taps
    TapsGains taps_gains_;

    float feed_gain_;

    FilterState filter_;
}; // EchoEffectState


EffectState* EffectStateFactory::create_echo()
{
    return create<EchoEffectState>();
}


/*  The document  "Effects Extension Guide.pdf"  says that low and high  *
 *  frequencies are cutoff frequencies. This is not fully correct, they  *
 *  are corner frequencies for low and high shelf filters. If they were  *
 *  just cutoff frequencies, there would be no need in cutoff frequency  *
 *  gains, which are present.  Documentation for  "Creative Proteus X2"  *
 *  software describes  4-band equalizer functionality in a much better  *
 *  way.  This equalizer seems  to be a predecessor  of  OpenAL  4-band  *
 *  equalizer.  With low and high  shelf filters  we are able to cutoff  *
 *  frequencies below and/or above corner frequencies using attenuation  *
 *  gains (below 1.0) and amplify all low and/or high frequencies using  *
 *  gains above 1.0.                                                     *
 *                                                                       *
 *     Low-shelf       Low Mid Band      High Mid Band     High-shelf    *
 *      corner            center             center          corner      *
 *     frequency        frequency          frequency       frequency     *
 *    50Hz..800Hz     200Hz..3000Hz      1000Hz..8000Hz  4000Hz..16000Hz *
 *                                                                       *
 *          |               |                  |               |         *
 *          |               |                  |               |         *
 *   B -----+            /--+--\            /--+--\            +-----    *
 *   O      |\          |   |   |          |   |   |          /|         *
 *   O      | \        -    |    -        -    |    -        / |         *
 *   S +    |  \      |     |     |      |     |     |      /  |         *
 *   T      |   |    |      |      |    |      |      |    |   |         *
 * ---------+---------------+------------------+---------------+-------- *
 *   C      |   |    |      |      |    |      |      |    |   |         *
 *   U -    |  /      |     |     |      |     |     |      \  |         *
 *   T      | /        -    |    -        -    |    -        \ |         *
 *   O      |/          |   |   |          |   |   |          \|         *
 *   F -----+            \--+--/            \--+--/            +-----    *
 *   F      |               |                  |               |         *
 *          |               |                  |               |         *
 *                                                                       *
 * Gains vary from 0.126 up to 7.943, which means from -18dB attenuation *
 * up to +18dB amplification. Band width varies from 0.01 up to 1.0 in   *
 * octaves for two mid bands.                                            *
 *                                                                       *
 * Implementation is based on the "Cookbook formulae for audio EQ biquad *
 * filter coefficients" by Robert Bristow-Johnson                        *
 * http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt                   */


class EqualizerEffectState :
    public EffectState
{
public:
    EqualizerEffectState()
        :
        EffectState{},
        channels_gains_{},
        filter_{},
        sample_buffer_{}
    {
    }

    virtual ~EqualizerEffectState()
    {
    }


protected:
    void do_construct()
    {
        // Initialize sample history only on filter creation to avoid
        // sound clicks if filter settings were changed in runtime.
        for (int it = 0; it < 4; ++it)
        {
            for (int ft = 0; ft < max_effect_channels; ++ft)
            {
                filter_[it][ft].clear();
            }
        }
    }

    void do_destruct()
    {
    }

    void do_update_device(
        Device& device)
    {
        static_cast<void>(device);
    }

    void do_update(
        Device& device,
        const EffectSlot& effect_slot,
        const EffectProps& effect_props)
    {
        static_cast<void>(effect_slot);

        const auto frequency = static_cast<float>(device.sampling_rate_);
        float gain;
        float freq_mult;

        dst_buffers_ = &device.sample_buffers_;
        dst_channel_count_ = device.channel_count_;

        for (int i = 0; i < max_effect_channels; ++i)
        {
            Panning::compute_first_order_gains(
                device.channel_count_,
                device.foa_,
                mat4f_identity.m_[i],
                1.0F,
                channels_gains_[i]);
        }

        // Calculate coefficients for the each type of filter. Note that the shelf
        // filters' gain is for the reference frequency, which is the centerpoint
        // of the transition band.
        gain = std::max(std::sqrt(effect_props.equalizer_.low_gain_), 0.0625F); // Limit -24dB
        freq_mult = effect_props.equalizer_.low_cutoff_ / frequency;

        filter_[0][0].set_params(
            FilterType::low_shelf,
            gain,
            freq_mult,
            FilterState::calc_rcp_q_from_slope(gain, 0.75F));

        // Copy the filter coefficients for the other input channels.
        for (int i = 1; i < max_effect_channels; ++i)
        {
            FilterState::copy_params(filter_[0][0], filter_[0][i]);
        }

        gain = std::max(effect_props.equalizer_.mid1_gain_, 0.0625F);
        freq_mult = effect_props.equalizer_.mid1_center_ / frequency;

        filter_[1][0].set_params(
            FilterType::peaking,
            gain,
            freq_mult,
            FilterState::calc_rcp_q_from_bandwidth(freq_mult, effect_props.equalizer_.mid1_width_));

        for (int i = 1; i < max_effect_channels; ++i)
        {
            FilterState::copy_params(filter_[1][0], filter_[1][i]);
        }

        gain = std::max(effect_props.equalizer_.mid2_gain_, 0.0625F);
        freq_mult = effect_props.equalizer_.mid2_center_ / frequency;

        filter_[2][0].set_params(
            FilterType::peaking,
            gain,
            freq_mult,
            FilterState::calc_rcp_q_from_bandwidth(freq_mult, effect_props.equalizer_.mid2_width_));

        for (int i = 1; i < max_effect_channels; ++i)
        {
            FilterState::copy_params(filter_[2][0], filter_[2][i]);
        }

        gain = std::max(std::sqrt(effect_props.equalizer_.high_gain_), 0.0625F);
        freq_mult = effect_props.equalizer_.high_cutoff_ / frequency;

        filter_[3][0].set_params(
            FilterType::high_shelf,
            gain,
            freq_mult,
            FilterState::calc_rcp_q_from_slope(gain, 0.75F));

        for (int i = 1; i < max_effect_channels; ++i)
        {
            FilterState::copy_params(filter_[3][0], filter_[3][i]);
        }
    }

    void do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count)
    {
        auto& samples = sample_buffer_;

        for (int base = 0; base < sample_count; )
        {
            const auto td = std::min(max_update_samples, sample_count - base);

            for (int ft = 0; ft < max_effect_channels; ++ft)
            {
                filter_[0][ft].process(td, &src_samples[ft][base], samples[0][ft].data());
            }

            for (int ft = 0; ft < max_effect_channels; ++ft)
            {
                filter_[1][ft].process(td, samples[0][ft].data(), samples[1][ft].data());
            }

            for (int ft = 0; ft < max_effect_channels; ++ft)
            {
                filter_[2][ft].process(td, samples[1][ft].data(), samples[2][ft].data());
            }

            for (int ft = 0; ft < max_effect_channels; ++ft)
            {
                filter_[3][ft].process(td, samples[2][ft].data(), samples[3][ft].data());
            }

            for (int ft = 0; ft < max_effect_channels; ++ft)
            {
                for (int kt = 0; kt < channel_count; ++kt)
                {
                    const auto gain = channels_gains_[ft][kt];

                    if (!(std::abs(gain) > silence_threshold_gain))
                    {
                        continue;
                    }

                    for (int it = 0; it < td; ++it)
                    {
                        dst_samples[kt][base + it] += gain * samples[3][ft][it];
                    }
                }
            }

            base += td;
        }
    }


private:
    // The maximum number of sample frames per update.
    static constexpr auto max_update_samples = 256;

    using ChannelsGains = std::array<Gains, max_effect_channels>;
    using Filters = MdArray<FilterState, 4, max_effect_channels>;
    using SampleBuffers = MdArray<float, 4, max_effect_channels, max_update_samples>;


    // Effect gains for each channel
    ChannelsGains channels_gains_;

    // Effect parameters
    Filters filter_;

    SampleBuffers sample_buffer_;
}; // EqualizerEffectState


EffectState* EffectStateFactory::create_equalizer()
{
    return create<EqualizerEffectState>();
}


class FlangerEffectState :
    public EffectState
{
public:
    FlangerEffectState()
        :
        EffectState{},
        sample_buffers_{},
        buffer_length_{},
        offset_{},
        lfo_range_{},
        lfo_scale_{},
        lfo_disp_{},
        sides_gains_{},
        waveform_{},
        delay_{},
        depth_{},
        feedback_{}
    {
    }

    virtual ~FlangerEffectState()
    {
    }


protected:
    void do_construct() final
    {
        buffer_length_ = 0;

        for (auto& buffer : sample_buffers_)
        {
            buffer = SampleBuffer{};
        }

        offset_ = 0;
        lfo_range_ = 1;
        waveform_ = Waveform::triangle;
    }

    void do_destruct() final
    {
        for (auto& buffer : sample_buffers_)
        {
            buffer = SampleBuffer{};
        }
    }

    void do_update_device(
        Device& device) final
    {
        auto maxlen = static_cast<int>(EffectProps::Flanger::max_delay * 2.0F * device.sampling_rate_) + 1;
        maxlen = Math::next_power_of_2(maxlen);

        if (maxlen != buffer_length_)
        {
            for (auto& buffer : sample_buffers_)
            {
                buffer.resize(maxlen);
            }

            buffer_length_ = maxlen;
        }

        for (auto& buffer : sample_buffers_)
        {
            std::fill(buffer.begin(), buffer.end(), 0.0F);
        }
    }

    void do_update(
        Device& device,
        const EffectSlot& effect_slot,
        const EffectProps& effect_props) final
    {
        static_cast<void>(effect_slot);

        const auto frequency = static_cast<float>(device.sampling_rate_);
        AmbiCoeffs coeffs;

        switch (effect_props.flanger_.waveform_)
        {
        case EffectProps::Flanger::waveform_triangle:
            waveform_ = Waveform::triangle;
            break;

        case EffectProps::Flanger::waveform_sinusoid:
            waveform_ = Waveform::sinusoid;
            break;
        }

        feedback_ = effect_props.flanger_.feedback_;
        delay_ = static_cast<int>(effect_props.flanger_.delay_ * frequency);

        // The LFO depth is scaled to be relative to the sample delay.
        depth_ = effect_props.flanger_.depth_ * delay_;

        // Gains for left and right sides
        Panning::calc_angle_coeffs(-Math::pi_2, 0.0F, 0.0F, coeffs);
        Panning::compute_panning_gains(device.channel_count_, device.dry_, coeffs, 1.0F, sides_gains_[0]);
        Panning::calc_angle_coeffs(Math::pi_2, 0.0F, 0.0F, coeffs);
        Panning::compute_panning_gains(device.channel_count_, device.dry_, coeffs, 1.0F, sides_gains_[1]);

        const auto phase = effect_props.flanger_.phase_;
        const auto rate = effect_props.flanger_.rate_;

        if (!(rate > 0.0F))
        {
            lfo_scale_ = 0.0F;
            lfo_range_ = 1;
            lfo_disp_ = 0;
        }
        else
        {
            // Calculate LFO coefficient
            lfo_range_ = static_cast<int>(frequency / rate + 0.5F);

            switch (waveform_)
            {
            case Waveform::triangle:
                lfo_scale_ = 4.0F / lfo_range_;
                break;

            case Waveform::sinusoid:
                lfo_scale_ = Math::tau / lfo_range_;
                break;
            }

            // Calculate lfo phase displacement
            if (phase >= 0)
            {
                lfo_disp_ = static_cast<int>(lfo_range_ * (phase / 360.0F));
            }
            else
            {
                lfo_disp_ = static_cast<int>(lfo_range_ * ((360 + phase) / 360.0F));
            }
        }
    }

    void do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final
    {
        auto& left_buf = sample_buffers_[0];
        auto& right_buf = sample_buffers_[1];
        const auto buf_mask = buffer_length_ - 1;

        for (int base = 0; base < sample_count; )
        {
            float temps[128][2];
            int mod_delays[2][128];

            const auto todo = std::min(128, sample_count - base);

            switch (waveform_)
            {
            case Waveform::triangle:
                GetTriangleDelays(
                    mod_delays[0],
                    offset_ % lfo_range_,
                    lfo_range_,
                    lfo_scale_,
                    depth_,
                    delay_,
                    todo);

                GetTriangleDelays(
                    mod_delays[1],
                    (offset_ + lfo_disp_) % lfo_range_,
                    lfo_range_,
                    lfo_scale_,
                    depth_,
                    delay_,
                    todo);

                break;

            case Waveform::sinusoid:
                GetSinusoidDelays(
                    mod_delays[0],
                    offset_ % lfo_range_,
                    lfo_range_,
                    lfo_scale_,
                    depth_,
                    delay_,
                    todo);

                GetSinusoidDelays(
                    mod_delays[1],
                    (offset_ + lfo_disp_) % lfo_range_,
                    lfo_range_,
                    lfo_scale_,
                    depth_,
                    delay_,
                    todo);

                break;
            }

            for (int i = 0; i < todo; ++i)
            {
                left_buf[offset_ & buf_mask] = src_samples[0][base + i];
                temps[i][0] = left_buf[(offset_ - mod_delays[0][i]) & buf_mask] * feedback_;
                left_buf[offset_ & buf_mask] += temps[i][0];

                right_buf[offset_ & buf_mask] = src_samples[0][base + i];
                temps[i][1] = right_buf[(offset_ - mod_delays[1][i]) & buf_mask] * feedback_;
                right_buf[offset_ & buf_mask] += temps[i][1];

                offset_ += 1;
            }

            for (int c = 0; c < channel_count; ++c)
            {
                auto gain = sides_gains_[0][c];

                if (std::abs(gain) > silence_threshold_gain)
                {
                    for (int i = 0; i < todo; i++)
                    {
                        dst_samples[c][i + base] += temps[i][0] * gain;
                    }
                }

                gain = sides_gains_[1][c];

                if (std::abs(gain) > silence_threshold_gain)
                {
                    for (int i = 0; i < todo; ++i)
                    {
                        dst_samples[c][i + base] += temps[i][1] * gain;
                    }
                }
            }

            base += todo;
        }
    }


private:
    enum class Waveform
    {
        triangle = EffectProps::Flanger::waveform_triangle,
        sinusoid = EffectProps::Flanger::waveform_sinusoid,
    }; // Waveform

    using SampleBuffer = EffectSampleBuffer;
    using SampleBuffers = std::array<SampleBuffer, 2>;
    using SidesGains = std::array<Gains, 2>;


    SampleBuffers sample_buffers_;
    int buffer_length_;
    int offset_;
    int lfo_range_;
    float lfo_scale_;
    int lfo_disp_;

    // Gains for left and right sides
    SidesGains sides_gains_;

    // effect parameters
    Waveform waveform_;
    int delay_;
    float depth_;
    float feedback_;


    static void GetTriangleDelays(
        int* delays,
        int offset,
        const int lfo_range,
        const float lfo_scale,
        const float depth,
        const int delay,
        const int todo)
    {
        for (int i = 0; i < todo; ++i)
        {
            delays[i] = static_cast<int>((1.0F - std::abs(2.0F - (lfo_scale * offset))) * depth) + delay;
            offset = (offset + 1) % lfo_range;
        }
    }

    static void GetSinusoidDelays(
        int* delays,
        int offset,
        const int lfo_range,
        const float lfo_scale,
        const float depth,
        const int delay,
        const int todo)
    {
        for (int i = 0; i < todo; ++i)
        {
            delays[i] = static_cast<int>(std::sin(lfo_scale * offset) * depth) + delay;
            offset = (offset + 1) % lfo_range;
        }
    }
}; // FlangerEffectState


EffectState* EffectStateFactory::create_flanger()
{
    return create<FlangerEffectState>();
}


class RingModulatorEffectState :
    public EffectState
{
public:
    RingModulatorEffectState()
        :
        EffectState{},
        process_func_{},
        index_{},
        step_{},
        channels_gains_{},
        filters_{}
    {
    }

    virtual ~RingModulatorEffectState()
    {
    }


protected:
    void do_construct() final
    {
        index_ = 0;
        step_ = 1;

        for (int i = 0; i < max_effect_channels; ++i)
        {
            filters_[i].clear();
        }
    }

    void do_destruct() final
    {
    }

    void do_update_device(
        Device& device) final
    {
        static_cast<void>(device);
    }

    void do_update(
        Device& device,
        const EffectSlot& effect_slot,
        const EffectProps& effect_props) final
    {
        static_cast<void>(effect_slot);

        if (effect_props.ring_modulator_.waveform_ == EffectProps::RingModulator::waveform_sinusoid)
        {
            process_func_ = modulate_sin;
        }
        else if (effect_props.ring_modulator_.waveform_ == EffectProps::RingModulator::waveform_sawtooth)
        {
            process_func_ = modulate_saw;
        }
        else
        {
            process_func_ = modulate_square;
        }

        step_ = static_cast<int>(effect_props.ring_modulator_.frequency_ * waveform_frac_one / device.sampling_rate_);

        if (step_ == 0)
        {
            step_ = 1;
        }

        // Custom filter coeffs, which match the old version instead of a low-shelf.
        const auto cw = std::cos(Math::tau * effect_props.ring_modulator_.high_pass_cutoff_ / device.sampling_rate_);
        const auto a = (2.0F - cw) - std::sqrt(std::pow(2.0F - cw, 2.0F) - 1.0F);

        for (int i = 0; i < max_effect_channels; ++i)
        {
            filters_[i].b0_ = a;
            filters_[i].b1_ = -a;
            filters_[i].b2_ = 0.0F;
            filters_[i].a1_ = -a;
            filters_[i].a2_ = 0.0F;
        }

        dst_buffers_ = &device.sample_buffers_;
        dst_channel_count_ = device.channel_count_;

        for (int i = 0; i < max_effect_channels; ++i)
        {
            Panning::compute_first_order_gains(
                device.channel_count_,
                device.foa_,
                mat4f_identity.m_[i],
                1.0F,
                channels_gains_[i]);
        }
    }

    void do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final
    {
        for (int base = 0; base < sample_count; )
        {
            float temps[2][128];
            const auto td = std::min(128, sample_count - base);

            for (int j = 0; j < max_effect_channels; ++j)
            {
                filters_[j].process(td, &src_samples[j][base], temps[0]);
                process_func_(temps[1], temps[0], index_, step_, td);

                for (int k = 0; k < channel_count; ++k)
                {
                    const auto gain = channels_gains_[j][k];

                    if (!(std::abs(gain) > silence_threshold_gain))
                    {
                        continue;
                    }

                    for (int i = 0; i < td; ++i)
                    {
                        dst_samples[k][base + i] += gain * temps[1][i];
                    }
                }
            }

            for (int i = 0; i < td; ++i)
            {
                index_ += step_;
                index_ &= waveform_frac_mask;
            }

            base += td;
        }
    }


private:
    static constexpr auto waveform_frac_bits = 24;
    static constexpr auto waveform_frac_one = 1 << waveform_frac_bits;
    static constexpr auto waveform_frac_mask = waveform_frac_one - 1;


    using ChannelsGains = std::array<Gains, max_effect_channels>;
    using Filters = std::array<FilterState, max_effect_channels>;

    using ModulateFunc = float (*)(
        const int index);

    using ProcessFunc = void (*)(
        float* const dst,
        const float* const src,
        int index,
        const int step,
        const int todo);


    ProcessFunc process_func_;
    int index_;
    int step_;
    ChannelsGains channels_gains_;
    Filters filters_;


    static float sin_func(
        const int index)
    {
        return std::sin(index * (Math::tau / waveform_frac_one) - Math::pi) * 0.5F + 0.5F;
    }

    static float saw_func(
        const int index)
    {
        return static_cast<float>(index) / waveform_frac_one;
    }

    static float square_func(
        const int index)
    {
        return static_cast<float>((index >> (waveform_frac_bits - 1)) & 1);
    }

    static void modulate(
        const ModulateFunc func,
        float* const dst,
        const float* const src,
        int index,
        const int step,
        const int todo)
    {
        for (int i = 0; i < todo; ++i)
        {
            index += step;
            index &= waveform_frac_mask;
            dst[i] = src[i] * func(index);
        }
    }

    static void modulate_sin(
        float* const dst,
        const float* const src,
        int index,
        const int step,
        const int todo)
    {
        modulate(sin_func, dst, src, index, step, todo);
    }

    static void modulate_saw(
        float* const dst,
        const float* const src,
        int index,
        const int step,
        const int todo)
    {
        modulate(saw_func, dst, src, index, step, todo);
    }

    static void modulate_square(
        float* const dst,
        const float* const src,
        int index,
        const int step,
        const int todo)
    {
        modulate(square_func, dst, src, index, step, todo);
    }
}; // ModulatorEffectState


EffectState* EffectStateFactory::create_ring_modulator()
{
    return create<RingModulatorEffectState>();
}


class ReverbEffectState :
    public EffectState
{
public:
    ReverbEffectState()
        :
        EffectState{},
        is_eax_{},
        filters_{},
        delay_{},
        early_delay_taps_{},
        early_delay_coeffs_{},
        late_feed_tap_{},
        late_delay_taps_{},
        ap_feed_coeff_{},
        mix_x_{},
        mix_y_{},
        early_{},
        mod_{},
        late_{},
        fade_count_{},
        offset_{},
        a_format_samples_{},
        reverb_samples_{},
        early_samples_{}
    {
    }

    virtual ~ReverbEffectState()
    {
    }


protected:
    void do_construct() final
    {
        is_eax_ = false;

        for (int i = 0; i < 4; ++i)
        {
            filters_[i].lp.clear();
            filters_[i].hp.clear();
        }

        delay_.reset();

        for (int i = 0; i < 4; ++i)
        {
            early_delay_taps_[i][0] = 0;
            early_delay_taps_[i][1] = 0;
            early_delay_coeffs_[i] = 0.0F;
        }

        late_feed_tap_ = 0;

        for (int i = 0; i < 4; ++i)
        {
            late_delay_taps_[i][0] = 0;
            late_delay_taps_[i][1] = 0;
        }

        ap_feed_coeff_ = 0.0F;
        mix_x_ = 0.0F;
        mix_y_ = 0.0F;

        early_.vec_ap.delay.reset();
        early_.delay.reset();

        for (int i = 0; i < 4; ++i)
        {
            early_.vec_ap.offsets[i][0] = 0;
            early_.vec_ap.offsets[i][1] = 0;
            early_.offsets[i][0] = 0;
            early_.offsets[i][1] = 0;
            early_.coeffs[i] = 0.0F;
        }

        mod_.index = 0;
        mod_.range = 1;
        mod_.depth = 0.0F;
        mod_.coeff = 0.0F;
        mod_.filter = 0.0F;

        late_.density_gain = 0.0F;

        late_.delay.reset();
        late_.vec_ap.delay.reset();

        for (int i = 0; i < 4; ++i)
        {
            late_.offsets[i][0] = 0;
            late_.offsets[i][1] = 0;

            late_.vec_ap.offsets[i][0] = 0;
            late_.vec_ap.offsets[i][1] = 0;

            for (int j = 0; j < 3; ++j)
            {
                late_.filters[i].lf_coeffs[j] = 0.0F;
                late_.filters[i].hf_coeffs[j] = 0.0F;
            }

            late_.filters[i].mid_coeff = 0.0F;

            late_.filters[i].states[0][0] = 0.0F;
            late_.filters[i].states[0][1] = 0.0F;
            late_.filters[i].states[1][0] = 0.0F;
            late_.filters[i].states[1][1] = 0.0F;
        }

        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < max_channels; ++j)
            {
                early_.current_gains[i][j] = 0.0F;
                early_.pan_gains[i][j] = 0.0F;
                late_.current_gains[i][j] = 0.0F;
                late_.pan_gains[i][j] = 0.0F;
            }
        }

        fade_count_ = 0;
        offset_ = 0;
    }

    void do_destruct() final
    {
    }

    void do_update_device(
        Device& device) final
    {
        const auto frequency = device.sampling_rate_;

        // Allocate the delay lines.
        alloc_lines(frequency);

        // Calculate the modulation filter coefficient.  Notice that the exponent
        // is calculated given the current sample rate.  This ensures that the
        // resulting filter response over time is consistent across all sample
        // rates.
        mod_.coeff = std::pow(modulation_filter_coeff, modulation_filter_const / frequency);

        const auto multiplier = 1.0F + line_multiplier;

        // The late feed taps are set a fixed position past the latest delay tap.
        for (int i = 0; i < 4; ++i)
        {
            late_feed_tap_ = static_cast<int>(
                (EffectProps::Reverb::max_reflections_delay + (early_tap_lengths[3] * multiplier)) * frequency);
        }
    }

    void do_update(
        Device& device,
        const EffectSlot& effect_slot,
        const EffectProps& effect_props) final
    {
        if (effect_slot.effect_.type_ == EffectType::eax_reverb)
        {
            is_eax_ = true;
        }
        else if (effect_slot.effect_.type_ == EffectType::reverb)
        {
            is_eax_ = false;
        }

        const auto frequency = device.sampling_rate_;

        // Calculate the master filters
        const auto hf_scale = effect_props.reverb_.hf_reference_ / frequency;

        // Restrict the filter gains from going below -60dB to keep the filter from
        // killing most of the signal.
        const auto gain_hf = std::max(effect_props.reverb_.gain_hf_, 0.001F);

        filters_[0].lp.set_params(
            FilterType::high_shelf,
            gain_hf,
            hf_scale,
            FilterState::calc_rcp_q_from_slope(gain_hf, 1.0F));

        const auto lf_scale = effect_props.reverb_.lf_reference_ / frequency;

        const auto gain_lf = std::max(effect_props.reverb_.gain_lf_, 0.001F);

        filters_[0].hp.set_params(
            FilterType::low_shelf,
            gain_lf,
            lf_scale,
            FilterState::calc_rcp_q_from_slope(gain_lf, 1.0F));

        for (int i = 1; i < 4; ++i)
        {
            FilterState::copy_params(filters_[0].lp, filters_[i].lp);
            FilterState::copy_params(filters_[0].hp, filters_[i].hp);
        }

        // Update the main effect delay and associated taps.
        update_delay_line(
            effect_props.reverb_.reflections_delay_,
            effect_props.reverb_.late_reverb_delay_,
            effect_props.reverb_.density_,
            effect_props.reverb_.decay_time_,
            frequency);

        // Calculate the all-pass feed-back/forward coefficient.
        ap_feed_coeff_ = std::sqrt(0.5F) * std::pow(effect_props.reverb_.diffusion_, 2.0F);

        // Update the early lines.
        update_early_lines(effect_props.reverb_.density_, effect_props.reverb_.decay_time_, frequency);

        // Get the mixing matrix coefficients.
        calc_matrix_coeffs(effect_props.reverb_.diffusion_, &mix_x_, &mix_y_);

        // If the HF limit parameter is flagged, calculate an appropriate limit
        // based on the air absorption parameter.
        auto hf_ratio = effect_props.reverb_.decay_hf_ratio_;

        if (effect_props.reverb_.decay_hf_limit_ && effect_props.reverb_.air_absorption_gain_hf_ < 1.0F)
        {
            hf_ratio = calc_limited_hf_ratio(
                hf_ratio,
                effect_props.reverb_.air_absorption_gain_hf_,
                effect_props.reverb_.decay_time_);
        }

        // Calculate the LF/HF decay times.
        const auto lf_decay_time = Math::clamp(
            effect_props.reverb_.decay_time_ * effect_props.reverb_.decay_lf_ratio_,
            EffectProps::Reverb::min_decay_time,
            EffectProps::Reverb::max_decay_time);

        const auto hf_decay_time = Math::clamp(
            effect_props.reverb_.decay_time_ * hf_ratio,
            EffectProps::Reverb::min_decay_time,
            EffectProps::Reverb::max_decay_time);

        // Update the modulator line.
        update_modulator(effect_props.reverb_.modulation_time_, effect_props.reverb_.modulation_depth_, frequency);

        // Update the late lines.
        update_late_lines(
            effect_props.reverb_.density_,
            effect_props.reverb_.diffusion_,
            lf_decay_time,
            effect_props.reverb_.decay_time_,
            hf_decay_time,
            Math::tau * lf_scale,
            Math::tau * hf_scale,
            effect_props.reverb_.echo_time_,
            effect_props.reverb_.echo_depth_,
            frequency);

        // Update early and late 3D panning.
        update_3d_panning(
            device,
            effect_props.reverb_.reflections_pan_.data(),
            effect_props.reverb_.late_reverb_pan_.data(),
            effect_props.reverb_.gain_,
            effect_props.reverb_.reflections_gain_,
            effect_props.reverb_.late_reverb_gain_);

        // Determine if delay-line cross-fading is required.
        for (int i = 0; i < 4; ++i)
        {
            if (early_delay_taps_[i][1] != early_delay_taps_[i][0] ||
                early_.vec_ap.offsets[i][1] != early_.vec_ap.offsets[i][0] ||
                early_.offsets[i][1] != early_.offsets[i][0] ||
                late_delay_taps_[i][1] != late_delay_taps_[i][0] ||
                late_.vec_ap.offsets[i][1] != late_.vec_ap.offsets[i][0] ||
                late_.offsets[i][1] != late_.offsets[i][0])
            {
                fade_count_ = 0;
                break;
            }
        }
    }

    void do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final
    {
        const auto reverb_func = (is_eax_ ? &ReverbEffectState::eax_verb_pass : &ReverbEffectState::verb_pass);
        auto fade = static_cast<float>(fade_count_) / fade_samples;

        // Process reverb for these samples.
        for (int base = 0; base < sample_count; )
        {
            auto todo = std::min(sample_count - base, max_update_samples);

            // If cross-fading, don't do more samples than there are to fade.
            if (fade_samples - fade_count_ > 0)
            {
                todo = std::min(todo, fade_samples - fade_count_);
            }

            // Convert B-Format to A-Format for processing.
            for (auto& samples : a_format_samples_)
            {
                samples.fill(0.0F);
            }

            for (int c = 0; c < 4; ++c)
            {
                MixHelpers::mix_row(
                    a_format_samples_[c].data(),
                    b2a.m_[c],
                    src_samples,
                    max_effect_channels,
                    base,
                    todo);
            }

            // Process the samples for reverb.
            fade = (this->*reverb_func)(todo, fade, a_format_samples_, early_samples_, reverb_samples_);

            if (fade_count_ < fade_samples)
            {
                fade_count_ += todo;

                if (fade_count_ >= fade_samples)
                {
                    // Update the cross-fading delay line taps.
                    fade_count_ = fade_samples;
                    fade = 1.0F;

                    for (int c = 0; c < 4; ++c)
                    {
                        early_delay_taps_[c][0] = early_delay_taps_[c][1];
                        early_.vec_ap.offsets[c][0] = early_.vec_ap.offsets[c][1];
                        early_.offsets[c][0] = early_.offsets[c][1];
                        late_delay_taps_[c][0] = late_delay_taps_[c][1];
                        late_.vec_ap.offsets[c][0] = late_.vec_ap.offsets[c][1];
                        late_.offsets[c][0] = late_.offsets[c][1];
                    }
                }
            }

            // Mix the A-Format results to output, implicitly converting back to
            // B-Format.
            for (int c = 0; c < 4; c++)
            {
                MixHelpers::mix(
                    early_samples_[c].data(),
                    channel_count,
                    dst_samples,
                    early_.current_gains[c].data(),
                    early_.pan_gains[c].data(),
                    sample_count - base,
                    base,
                    todo);
            }

            for (int c = 0; c < 4; c++)
            {
                MixHelpers::mix(
                    reverb_samples_[c].data(),
                    channel_count,
                    dst_samples,
                    late_.current_gains[c].data(),
                    late_.pan_gains[c].data(),
                    sample_count - base,
                    base,
                    todo);
            }

            base += todo;
        }
    }


private:
    static constexpr auto speed_of_sound_mps = 343.3F;

    // Target gain for the reverb decay feedback reaching the decay time.
    static constexpr auto reverb_decay_gain = 0.001F; // -60 dB

    // This is the maximum number of samples processed for each inner loop
    // iteration.
    static constexpr auto max_update_samples = 256;

    // The number of samples used for cross-faded delay lines.  This can be used
    // to balance the compensation for abrupt line changes and attenuation due to
    // minimally lengthed recursive lines.  Try to keep this below the device
    // update size.
    static constexpr auto fade_samples = 128;


    using ChannelsGains = std::array<Gains, 4>;


    struct DelayLineI
    {
        using Line = std::array<float, 4>;
        using Lines = std::vector<Line>;

        // The delay lines use interleaved samples, with the lengths being powers
        // of 2 to allow the use of bit-masking instead of a modulus for wrapping.
        int mask;
        Lines lines;


        int get_sample_count() const
        {
            return (mask > 0 ? mask + 1 : 0);
        }

        void reset()
        {
            mask = 0;
            lines = Lines{};
        }

        void initialize(
            const int sample_count)
        {
            if (sample_count == get_sample_count())
            {
                lines.clear();
                lines.resize(sample_count);
                return;
            }

            reset();

            mask = sample_count - 1;
            lines.resize(sample_count);
        }
    }; // DelayLineI

    struct VecAllpass
    {
        using Offsets = MdArray<int, 4, 2>;

        DelayLineI delay;
        Offsets offsets;
    }; // VecAllpass

    struct Filter
    {
        FilterState lp;
        FilterState hp; // EAX only
    }; // FilterProps

    using Filters = std::array<Filter, 4>;

    struct Early
    {
        using Offsets = MdArray<int, 4, 2>;
        using Coeffs = std::array<float, 4>;

        // A Gerzon vector all-pass filter is used to simulate initial
        // diffusion.  The spread from this filter also helps smooth out the
        // reverb tail.
        //
        VecAllpass vec_ap;

        // An echo line is used to complete the second half of the early
        // reflections.
        //
        DelayLineI delay;
        Offsets offsets;
        Coeffs coeffs;

        // The gain for each output channel based on 3D panning.
        ChannelsGains current_gains;
        ChannelsGains pan_gains;
    }; // Early

    struct Mod
    {
        // The vibrato time is tracked with an index over a modulus-wrapped
        // range (in samples).
        //
        int index;
        int range;

        // The depth of frequency change (also in samples) and its filter.
        float depth;
        float coeff;
        float filter;
    }; // Mod

    struct Late
    {
        struct Filter
        {
            using Coeffs = std::array<float, 3>;
            using States = MdArray<float, 2, 2>;

            Coeffs lf_coeffs;
            Coeffs hf_coeffs;
            float mid_coeff;

            // The LF and HF filters keep a state of the last input and last
            // output sample.
            States states;
        }; // FilterProps

        using Filters = std::array<Filter, 4>;
        using Offsets = MdArray<int, 4, 2>;


        // Attenuation to compensate for the modal density and decay rate of
        // the late lines.
        //
        float density_gain;

        // A recursive delay line is used fill in the reverb tail.
        DelayLineI delay;
        Offsets offsets;

        // T60 decay filters are used to simulate absorption.
        Filters filters;

        // A Gerzon vector all-pass filter is used to simulate diffusion.
        VecAllpass vec_ap;

        // The gain for each output channel based on 3D panning.
        ChannelsGains current_gains;
        ChannelsGains pan_gains;
    }; // Late

    using Taps = MdArray<int, 4, 2>;

    using SamplesPerChannel = std::array<float, max_update_samples>;
    using Samples = std::array<SamplesPerChannel, 4>;
    using Coeffs = std::array<float, 4>;


    bool is_eax_;

    // Master effect filters
    Filters filters_;

    // Core delay line (early reflections and late reverb tap from this).
    DelayLineI delay_;

    // Tap points for early reflection delay.
    Taps early_delay_taps_;
    Coeffs early_delay_coeffs_;

    // Tap points for late reverb feed and delay.
    int late_feed_tap_;
    Taps late_delay_taps_;

    // The feed-back and feed-forward all-pass coefficient.
    float ap_feed_coeff_;

    // Coefficients for the all-pass and line scattering matrices.
    float mix_x_;
    float mix_y_;

    Early early_;
    Mod mod_; // EAX only
    Late late_;

    // Indicates the cross-fade point for delay line reads [0,FADE_SAMPLES].
    int fade_count_;

    // The current write offset for all delay lines.
    int offset_;

    // Temporary storage used when processing.
    Samples a_format_samples_;
    Samples reverb_samples_;
    Samples early_samples_;


    // The B-Format to A-Format conversion matrix. The arrangement of rows is
    // deliberately chosen to align the resulting lines to their spatial opposites
    // (0:above front left <-> 3:above back right, 1:below front right <-> 2:below
    // back left). It's not quite opposite, since the A-Format results in a
    // tetrahedron, but it's close enough. Should the model be extended to 8-lines
    // in the future, true opposites can be used.
    static constexpr Mat4F b2a = {{
        { 0.288675134595F,  0.288675134595F,  0.288675134595F,  0.288675134595F },
        { 0.288675134595F, -0.288675134595F, -0.288675134595F,  0.288675134595F },
        { 0.288675134595F,  0.288675134595F, -0.288675134595F, -0.288675134595F },
        { 0.288675134595F, -0.288675134595F,  0.288675134595F, -0.288675134595F },
    }};

    // Converts A-Format to B-Format.
    static constexpr Mat4F a2b = {{
        { 0.866025403785F,  0.866025403785F,  0.866025403785F,  0.866025403785F },
        { 0.866025403785F, -0.866025403785F,  0.866025403785F, -0.866025403785F },
        { 0.866025403785F, -0.866025403785F, -0.866025403785F,  0.866025403785F },
        { 0.866025403785F,  0.866025403785F, -0.866025403785F, -0.866025403785F },
    }};

    static constexpr auto fade_step = 1.0F / fade_samples;

    // The all-pass and delay lines have a variable length dependent on the
    // effect's density parameter.  The resulting density multiplier is:
    //
    //     multiplier = 1 + (density * LINE_MULTIPLIER)
    //
    // Thus the line multiplier below will result in a maximum density multiplier
    // of 10.
    static constexpr auto line_multiplier = 9.0F;

    // All delay line lengths are specified in seconds.
    //
    // To approximate early reflections, we break them up into primary (those
    // arriving from the same direction as the source) and secondary (those
    // arriving from the opposite direction).
    //
    // The early taps decorrelate the 4-channel signal to approximate an average
    // room response for the primary reflections after the initial early delay.
    //
    // Given an average room dimension (d_a) and the speed of sound (c) we can
    // calculate the average reflection delay (r_a) regardless of listener and
    // source positions as:
    //
    //     r_a = d_a / c
    //     c   = 343.3
    //
    // This can extended to finding the average difference (r_d) between the
    // maximum (r_1) and minimum (r_0) reflection delays:
    //
    //     r_0 = 2 / 3 r_a
    //         = r_a - r_d / 2
    //         = r_d
    //     r_1 = 4 / 3 r_a
    //         = r_a + r_d / 2
    //         = 2 r_d
    //     r_d = 2 / 3 r_a
    //         = r_1 - r_0
    //
    // As can be determined by integrating the 1D model with a source (s) and
    // listener (l) positioned across the dimension of length (d_a):
    //
    //     r_d = int_(l=0)^d_a (int_(s=0)^d_a |2 d_a - 2 (l + s)| ds) dl / c
    //
    // The initial taps (T_(i=0)^N) are then specified by taking a power series
    // that ranges between r_0 and half of r_1 less r_0:
    //
    //     R_i = 2^(i / (2 N - 1)) r_d
    //         = r_0 + (2^(i / (2 N - 1)) - 1) r_d
    //         = r_0 + T_i
    //     T_i = R_i - r_0
    //         = (2^(i / (2 N - 1)) - 1) r_d
    //
    // Assuming an average of 5m (up to 50m with the density multiplier), we get
    // the following taps:
    static constexpr float early_tap_lengths[4] =
    {
        0.000000e+0F, 1.010676E-3F, 2.126553E-3F, 3.358580E-3F,
    };

    // The early all-pass filter lengths are based on the early tap lengths:
    //
    //     A_i = R_i / a
    //
    // Where a is the approximate maximum all-pass cycle limit (20).
    //
    static constexpr float early_allpass_lengths[4] =
    {
        4.854840E-4F, 5.360178E-4F, 5.918117E-4F, 6.534130E-4F,
    };

    // The early delay lines are used to transform the primary reflections into
    // the secondary reflections.  The A-format is arranged in such a way that
    // the channels/lines are spatially opposite:
    //
    //     C_i is opposite C_(N-i-1)
    //
    // The delays of the two opposing reflections (R_i and O_i) from a source
    // anywhere along a particular dimension always sum to twice its full delay:
    //
    //     2 r_a = R_i + O_i
    //
    // With that in mind we can determine the delay between the two reflections
    // and thus specify our early line lengths (L_(i=0)^N) using:
    //
    //     O_i = 2 r_a - R_(N-i-1)
    //     L_i = O_i - R_(N-i-1)
    //         = 2 (r_a - R_(N-i-1))
    //         = 2 (r_a - T_(N-i-1) - r_0)
    //         = 2 r_a (1 - (2 / 3) 2^((N - i - 1) / (2 N - 1)))
    //
    // Using an average dimension of 5m, we get:
    static constexpr float early_line_lengths[4] =
    {
        2.992520E-3F, 5.456575E-3F, 7.688329E-3F, 9.709681E-3F,
    };

    // The late all-pass filter lengths are based on the late line lengths:
    //
    //     A_i = (5 / 3) L_i / r_1
    //
    static constexpr float late_allpass_lengths[4] =
    {
        8.091400E-4F, 1.019453E-3F, 1.407968E-3F, 1.618280E-3F,
    };

    // The late lines are used to approximate the decaying cycle of recursive
    // late reflections.
    //
    // Splitting the lines in half, we start with the shortest reflection paths
    // (L_(i=0)^(N/2)):
    //
    //     L_i = 2^(i / (N - 1)) r_d
    //
    // Then for the opposite (longest) reflection paths (L_(i=N/2)^N):
    //
    //     L_i = 2 r_a - L_(i-N/2)
    //         = 2 r_a - 2^((i - N / 2) / (N - 1)) r_d
    //
    // For our 5m average room, we get:
    static constexpr float late_line_lengths[4] =
    {
        9.709681E-3F, 1.223343E-2F, 1.689561E-2F, 1.941936E-2F,
    };

    // This coefficient is used to define the sinus depth according to the
    // modulation depth property. This value must be below half the shortest late
    // line length (0.0097/2 = ~0.0048), otherwise with certain parameters (high
    // mod time, low density) the downswing can sample before the input.
    static constexpr float modulation_depth_coeff = 1.0F / 4096.0F;

    // A filter is used to avoid the terrible distortion caused by changing
    // modulation time and/or depth.  To be consistent across different sample
    // rates, the coefficient must be raised to a constant divided by the sample
    // rate:  coeff^(constant / rate).
    static constexpr float modulation_filter_coeff = 0.048F;
    static constexpr float modulation_filter_const = 100000.0F;


    //
    // Device Update
    //

    // Calculate the length of a delay line and store its mask and offset.
    static void initialize_delay_line(
        const float length,
        const int frequency,
        const int extra,
        DelayLineI& delay)
    {
        auto sample_count = int{};

        // All line lengths are powers of 2, calculated from their lengths in
        // seconds, rounded up.
        sample_count = static_cast<int>(std::ceil(length * frequency));
        sample_count = Math::next_power_of_2(sample_count + extra);

        delay.initialize(sample_count);
    }

    // Calculates the delay line metrics and allocates the lines for given
    // the sample rate (frequency).
    void alloc_lines(
        const int frequency)
    {
        // Multiplier for the maximum density value, i.e. density=1, which is
        // actually the least density...
        //
        const auto multiplier = 1.0F + line_multiplier;

        // The main delay length includes the maximum early reflection delay, the
        // largest early tap width, the maximum late reverb delay, and the
        // largest late tap width.  Finally, it must also be extended by the
        // update size (MAX_UPDATE_SAMPLES) for block processing.
        auto length = EffectProps::Reverb::max_reflections_delay +
                 (early_tap_lengths[3] * multiplier) +
                 EffectProps::Reverb::max_late_reverb_delay +
                 ((late_line_lengths[3] - late_line_lengths[0]) * 0.25F * multiplier);

        initialize_delay_line(length, frequency, max_update_samples, delay_);

        // The early vector all-pass line.
        length = early_allpass_lengths[3] * multiplier;
        initialize_delay_line(length, frequency, 0, early_.vec_ap.delay);

        // The early reflection line.
        length = early_line_lengths[3] * multiplier;
        initialize_delay_line(length, frequency, 0, early_.delay);

        // The late vector all-pass line.
        length = late_allpass_lengths[3] * multiplier;
        initialize_delay_line(length, frequency, 0, late_.vec_ap.delay);

        // The late delay lines are calculated from the larger of the maximum
        // density line length or the maximum echo time, and includes the maximum
        // modulation-related delay. The modulator's delay is calculated from the
        // maximum modulation time and depth coefficient, and halved for the low-
        // to-high frequency swing.
        length = std::max(
            EffectProps::Reverb::max_echo_time,
            late_line_lengths[3] * multiplier) +
                (EffectProps::Reverb::max_modulation_time * modulation_depth_coeff / 2.0F);

        initialize_delay_line(length, frequency, 0, late_.delay);
    }


    //
    // Effect Update
    //

    // Calculate a decay coefficient given the length of each cycle and the time
    // until the decay reaches -60 dB.
    static float calc_decay_coeff(
        const float length,
        const float decayTime)
    {
        return std::pow(reverb_decay_gain, length / decayTime);
    }

    // Calculate a decay length from a coefficient and the time until the decay
    // reaches -60 dB.
    static float calc_decay_length(
        const float coeff,
        const float decay_time)
    {
        return std::log10(coeff) * decay_time / std::log10(reverb_decay_gain);
    }

    // Calculate an attenuation to be applied to the input of any echo models to
    // compensate for modal density and decay time.
    static float calc_density_gain(
        const float a)
    {
        // The energy of a signal can be obtained by finding the area under the
        // squared signal.  This takes the form of Sum(x_n^2), where x is the
        // amplitude for the sample n.
        //
        // Decaying feedback matches exponential decay of the form Sum(a^n),
        // where a is the attenuation coefficient, and n is the sample.  The area
        // under this decay curve can be calculated as:  1 / (1 - a).
        //
        // Modifying the above equation to find the area under the squared curve
        // (for energy) yields:  1 / (1 - a^2).  Input attenuation can then be
        // calculated by inverting the square root of this approximation,
        // yielding:  1 / sqrt(1 / (1 - a^2)), simplified to: sqrt(1 - a^2).
        //
        return std::sqrt(1.0F - (a * a));
    }

    // Calculate the scattering matrix coefficients given a diffusion factor.
    static void calc_matrix_coeffs(
        const float diffusion,
        float* x,
        float* y)
    {
        // The matrix is of order 4, so n is sqrt(4 - 1).
        const auto n = std::sqrt(3.0F);
        const auto t = diffusion * std::atan(n);

        // Calculate the first mixing matrix coefficient.
        *x = std::cos(t);

        // Calculate the second mixing matrix coefficient.
        *y = std::sin(t) / n;
    }

    // Calculate the limited HF ratio for use with the late reverb low-pass
    // filters.
    static float calc_limited_hf_ratio(
        const float hf_ratio,
        const float air_absorption_gain_hf,
        const float decay_time)
    {
        // Find the attenuation due to air absorption in dB (converting delay
        // time to meters using the speed of sound).  Then reversing the decay
        // equation, solve for HF ratio.  The delay length is cancelled out of
        // the equation, so it can be calculated once for all lines.
        const auto limit_ratio = 1.0F / (calc_decay_length(air_absorption_gain_hf, decay_time) *
            speed_of_sound_mps);

        // Using the limit calculated above, apply the upper bound to the HF
        // ratio. Also need to limit the result to a minimum of 0.1, just like
        // the HF ratio parameter.
        return Math::clamp(limit_ratio, 0.1F, hf_ratio);
    }

    // Calculates the first-order high-pass coefficients following the I3DL2
    // reference model.  This is the transfer function:
    //
    //                1 - z^-1
    //     H(z) = p ------------
    //               1 - p z^-1
    //
    // And this is the I3DL2 coefficient calculation given gain (g) and reference
    // angular frequency (w):
    //
    //                                    g
    //      p = ------------------------------------------------------
    //          g cos(w) + sqrt((cos(w) - 1) (g^2 cos(w) + g^2 - 2))
    //
    // The coefficient is applied to the partial differential filter equation as:
    //
    //     c_0 = p
    //     c_1 = -p
    //     c_2 = p
    //     y_i = c_0 x_i + c_1 x_(i-1) + c_2 y_(i-1)
    //
    static void calc_highpass_coeffs(
        const float gain,
        const float w,
        float coeffs[3])
    {
        if (gain >= 1.0F)
        {
            coeffs[0] = 1.0F;
            coeffs[1] = 0.0F;
            coeffs[2] = 0.0F;

            return;
        }

        const auto g = std::max(0.001F, gain);
        const auto g2 = g * g;
        const auto cw = std::cos(w);
        const auto p = g / ((g * cw) + std::sqrt((cw - 1.0F) * ((g2 * cw) + g2 - 2.0F)));

        coeffs[0] = p;
        coeffs[1] = -p;
        coeffs[2] = p;
    }

    // Calculates the first-order low-pass coefficients following the I3DL2
    // reference model.  This is the transfer function:
    //
    //              (1 - a) z^0
    //     H(z) = ----------------
    //             1 z^0 - a z^-1
    //
    // And this is the I3DL2 coefficient calculation given gain (g) and reference
    // angular frequency (w):
    //
    //          1 - g^2 cos(w) - sqrt(2 g^2 (1 - cos(w)) - g^4 (1 - cos(w)^2))
    //     a = ----------------------------------------------------------------
    //                                    1 - g^2
    //
    // The coefficient is applied to the partial differential filter equation as:
    //
    //     c_0 = 1 - a
    //     c_1 = 0
    //     c_2 = a
    //     y_i = c_0 x_i + c_1 x_(i-1) + c_2 y_(i-1)
    //
    static void calc_lowpass_coeffs(
        const float gain,
        const float w,
        float coeffs[3])
    {
        if (gain >= 1.0F)
        {
            coeffs[0] = 1.0F;
            coeffs[1] = 0.0F;
            coeffs[2] = 0.0F;

            return;
        }

        // Be careful with gains < 0.001, as that causes the coefficient
        // to head towards 1, which will flatten the signal.
        const auto g = std::max(0.001F, gain);
        const auto g2 = g * g;
        const auto cw = std::cos(w);

        const auto a = (1.0F - (g2 * cw) - std::sqrt((2.0F * g2 * (1.0F - cw)) - (g2 * g2 * (1.0F - (cw * cw))))) /
            (1.0F - g2);

        coeffs[0] = 1.0F - a;
        coeffs[1] = 0.0F;
        coeffs[2] = a;
    }

    // Calculates the first-order low-shelf coefficients.  The shelf filters are
    // used in place of low/high-pass filters to preserve the mid-band.  This is
    // the transfer function:
    //
    //             a_0 + a_1 z^-1
    //     H(z) = ----------------
    //              1 + b_1 z^-1
    //
    // And these are the coefficient calculations given cut gain (g) and a center
    // angular frequency (w):
    //
    //          sin(0.5 (pi - w) - 0.25 pi)
    //     p = -----------------------------
    //          sin(0.5 (pi - w) + 0.25 pi)
    //
    //          g + 1           g + 1
    //     a = ------- + sqrt((-------)^2 - 1)
    //          g - 1           g - 1
    //
    //            1 + g + (1 - g) a
    //     b_0 = -------------------
    //                    2
    //
    //            1 - g + (1 + g) a
    //     b_1 = -------------------
    //                    2
    //
    // The coefficients are applied to the partial differential filter equation
    // as:
    //
    //            b_0 + p b_1
    //     c_0 = -------------
    //              1 + p a
    //
    //            -(b_1 + p b_0)
    //     c_1 = ----------------
    //               1 + p a
    //
    //             p + a
    //     c_2 = ---------
    //            1 + p a
    //
    //     y_i = c_0 x_i + c_1 x_(i-1) + c_2 y_(i-1)
    //
    static void calc_low_shelf_coeffs(
        const float gain,
        const float w,
        float coeffs[3])
    {
        if (gain >= 1.0F)
        {
            coeffs[0] = 1.0F;
            coeffs[1] = 0.0F;
            coeffs[2] = 0.0F;

            return;
        }

        const auto g = std::max(0.001F, gain);
        const auto rw = Math::pi - w;
        const auto p = std::sin((0.5F * rw) - (0.25F * Math::pi)) / std::sin((0.5F * rw) + (0.25F * Math::pi));
        const auto n = (g + 1.0F) / (g - 1.0F);
        const auto alpha = n + std::sqrt((n * n) - 1.0F);
        const auto beta0 = (1.0F + g + (1.0F - g) * alpha) / 2.0F;
        const auto beta1 = (1.0F - g + (1.0F + g) * alpha) / 2.0F;

        coeffs[0] = (beta0 + (p * beta1)) / (1.0F + (p * alpha));
        coeffs[1] = -(beta1 + (p * beta0)) / (1.0F + (p * alpha));
        coeffs[2] = (p + alpha) / (1.0F + (p * alpha));
    }

    // Calculates the first-order high-shelf coefficients.  The shelf filters are
    // used in place of low/high-pass filters to preserve the mid-band.  This is
    // the transfer function:
    //
    //             a_0 + a_1 z^-1
    //     H(z) = ----------------
    //              1 + b_1 z^-1
    //
    // And these are the coefficient calculations given cut gain (g) and a center
    // angular frequency (w):
    //
    //          sin(0.5 w - 0.25 pi)
    //     p = ----------------------
    //          sin(0.5 w + 0.25 pi)
    //
    //          g + 1           g + 1
    //     a = ------- + sqrt((-------)^2 - 1)
    //          g - 1           g - 1
    //
    //            1 + g + (1 - g) a
    //     b_0 = -------------------
    //                    2
    //
    //            1 - g + (1 + g) a
    //     b_1 = -------------------
    //                    2
    //
    // The coefficients are applied to the partial differential filter equation
    // as:
    //
    //            b_0 + p b_1
    //     c_0 = -------------
    //              1 + p a
    //
    //            b_1 + p b_0
    //     c_1 = -------------
    //              1 + p a
    //
    //            -(p + a)
    //     c_2 = ----------
    //            1 + p a
    //
    //     y_i = c_0 x_i + c_1 x_(i-1) + c_2 y_(i-1)
    //
    //
    static void calc_high_shelf_coeffs(
        const float gain,
        const float w,
        float coeffs[3])
    {
        if (gain >= 1.0F)
        {
            coeffs[0] = 1.0F;
            coeffs[1] = 0.0F;
            coeffs[2] = 0.0F;

            return;
        }

        const auto g = std::max(0.001F, gain);
        const auto p = std::sin((0.5F * w) - (0.25F * Math::pi)) / std::sin((0.5F * w) + (0.25F * Math::pi));
        const auto n = (g + 1.0F) / (g - 1.0F);
        const auto alpha = n + std::sqrt((n * n) - 1.0F);
        const auto beta0 = (1.0F + g + (1.0F - g) * alpha) / 2.0F;
        const auto beta1 = (1.0F - g + (1.0F + g) * alpha) / 2.0F;

        coeffs[0] = (beta0 + (p * beta1)) / (1.0F + (p * alpha));
        coeffs[1] = (beta1 + (p * beta0)) / (1.0F + (p * alpha));
        coeffs[2] = -(p + alpha) / (1.0F + (p * alpha));
    }

    // Calculates the 3-band T60 damping coefficients for a particular delay line
    // of specified length using a combination of two low/high-pass/shelf or
    // pass-through filter sections (producing 3 coefficients each) and a general
    // gain (7th coefficient) given decay times for each band split at two (LF/
    // HF) reference frequencies (w).
    static void calc_t60_damping_coeffs(
        const float length,
        const float lf_decay_time,
        const float mf_decay_time,
        const float hf_decay_time,
        const float lf_w,
        const float hf_w,
        float lfcoeffs[3],
        float hfcoeffs[3],
        float *midcoeff)
    {
        const auto lf_gain = calc_decay_coeff(length, lf_decay_time);
        const auto mf_gain = calc_decay_coeff(length, mf_decay_time);
        const auto hf_gain = calc_decay_coeff(length, hf_decay_time);

        if (lf_gain < mf_gain)
        {
            if (mf_gain < hf_gain)
            {
                calc_low_shelf_coeffs(mf_gain / hf_gain, hf_w, lfcoeffs);
                calc_highpass_coeffs(lf_gain / mf_gain, lf_w, hfcoeffs);
                *midcoeff = hf_gain;
            }
            else if (mf_gain > hf_gain)
            {
                calc_highpass_coeffs(lf_gain / mf_gain, lf_w, lfcoeffs);
                calc_lowpass_coeffs(hf_gain / mf_gain, hf_w, hfcoeffs);
                *midcoeff = mf_gain;
            }
            else
            {
                lfcoeffs[0] = 1.0F;
                lfcoeffs[1] = 0.0F;
                lfcoeffs[2] = 0.0F;
                calc_highpass_coeffs(lf_gain / mf_gain, lf_w, hfcoeffs);
                *midcoeff = mf_gain;
            }
        }
        else if (lf_gain > mf_gain)
        {
            if (mf_gain < hf_gain)
            {
                const auto hg = mf_gain / lf_gain;
                const auto lg = mf_gain / hf_gain;

                calc_high_shelf_coeffs(hg, lf_w, lfcoeffs);
                calc_low_shelf_coeffs(lg, hf_w, hfcoeffs);
                *midcoeff = std::max(lf_gain, hf_gain) / std::max(hg, lg);
            }
            else if (mf_gain > hf_gain)
            {
                calc_high_shelf_coeffs(mf_gain / lf_gain, lf_w, lfcoeffs);
                calc_lowpass_coeffs(hf_gain / mf_gain, hf_w, hfcoeffs);
                *midcoeff = lf_gain;
            }
            else
            {
                lfcoeffs[0] = 1.0F;
                lfcoeffs[1] = 0.0F;
                lfcoeffs[2] = 0.0F;
                calc_high_shelf_coeffs(mf_gain / lf_gain, lf_w, hfcoeffs);
                *midcoeff = lf_gain;
            }
        }
        else
        {
            lfcoeffs[0] = 1.0F;
            lfcoeffs[1] = 0.0F;
            lfcoeffs[2] = 0.0F;

            if (mf_gain < hf_gain)
            {
                calc_low_shelf_coeffs(mf_gain / hf_gain, hf_w, hfcoeffs);
                *midcoeff = hf_gain;
            }
            else if (mf_gain > hf_gain)
            {
                calc_lowpass_coeffs(hf_gain / mf_gain, hf_w, hfcoeffs);
                *midcoeff = mf_gain;
            }
            else
            {
                hfcoeffs[0] = 1.0F;
                hfcoeffs[1] = 0.0F;
                hfcoeffs[2] = 0.0F;
                *midcoeff = mf_gain;
            }
        }
    }

    // Update the EAX modulation index, range, and depth.  Keep in mind that this
    // kind of vibrato is additive and not multiplicative as one may expect.  The
    // downswing will sound stronger than the upswing.
    void update_modulator(
        const float mod_time,
        const float mod_depth,
        const int frequency)
    {
        // Modulation is calculated in two parts.
        //
        // The modulation time effects the speed of the sinus. An index out of the
        // current range (both in samples) is incremented each sample, so a longer
        // time implies a larger range. The range is bound to a reasonable minimum
        // (1 sample) and when the timing changes, the index is rescaled to the new
        // range to keep the sinus consistent.
        //
        const auto range = std::max(static_cast<int>(mod_time * frequency), 1);

        mod_.index = static_cast<int>(mod_.index * static_cast<int64_t>(range) / mod_.range);
        mod_.range = range;

        // The modulation depth effects the scale of the sinus, which changes how
        // much extra delay is added to the delay line. This delay changing over
        // time changes the pitch, creating the modulation effect. The scale needs
        // to be multiplied by the modulation time so that a given depth produces a
        // consistent shift in frequency over all ranges of time. Since the depth
        // is applied to a sinus value, it needs to be halved for the sinus swing
        // in time (half of it is spent decreasing the frequency, half is spent
        // increasing it).
        mod_.depth = mod_depth * modulation_depth_coeff * mod_time / 2.0F * frequency;
    }

    // Update the offsets for the main effect delay line.
    void update_delay_line(
        const float early_delay,
        const float late_delay,
        const float density,
        const float decay_time,
        const int frequency)
    {
        const auto multiplier = 1.0F + (density * line_multiplier);

        // Early reflection taps are decorrelated by means of an average room
        // reflection approximation described above the definition of the taps.
        // This approximation is linear and so the above density multiplier can
        // be applied to adjust the width of the taps.  A single-band decay
        // coefficient is applied to simulate initial attenuation and absorption.
        //
        // Late reverb taps are based on the late line lengths to allow a zero-
        // delay path and offsets that would continue the propagation naturally
        // into the late lines.

        for (int i = 0; i < 4; ++i)
        {
            auto length = float{};

            length = early_delay + (early_tap_lengths[i] * multiplier);
            early_delay_taps_[i][1] = static_cast<int>(length * frequency);

            length = early_tap_lengths[i] * multiplier;
            early_delay_coeffs_[i] = calc_decay_coeff(length, decay_time);

            length = late_delay + (late_line_lengths[i] - late_line_lengths[0]) * 0.25F * multiplier;
            late_delay_taps_[i][1] = late_feed_tap_ + static_cast<int>(length * frequency);
        }
    }

    // Update the early reflection line lengths and gain coefficients.
    void update_early_lines(
        const float density,
        const float decay_time,
        const int frequency)
    {
        const auto multiplier = 1.0F + density*line_multiplier;

        for(int i = 0; i < 4; ++i)
        {
            auto length = float{};

            // Calculate the length (in seconds) of each all-pass line.
            length = early_allpass_lengths[i] * multiplier;

            // Calculate the delay offset for each all-pass line.
            early_.vec_ap.offsets[i][1] = static_cast<int>(length * frequency);

            // Calculate the length (in seconds) of each delay line.
            length = early_line_lengths[i] * multiplier;

            // Calculate the delay offset for each delay line.
            early_.offsets[i][1] = static_cast<int>(length * frequency);

            /* Calculate the gain (coefficient) for each line. */
            early_.coeffs[i] = calc_decay_coeff(length, decay_time);
        }
    }

    // Update the late reverb line lengths and T60 coefficients.
    void update_late_lines(
        const float density,
        const float diffusion,
        const float lf_decay_time,
        const float mf_decay_time,
        const float hf_decay_time,
        const float lf_w,
        const float hf_w,
        const float echo_time,
        const float echo_depth,
        const int frequency)
    {
        float band_weights[3];

        // To compensate for changes in modal density and decay time of the late
        // reverb signal, the input is attenuated based on the maximal energy of
        // the outgoing signal.  This approximation is used to keep the apparent
        // energy of the signal equal for all ranges of density and decay time.
        //
        // The average length of the delay lines is used to calculate the
        // attenuation coefficient.

        const auto multiplier = 1.0F + (density * line_multiplier);

        auto length = (late_line_lengths[0] + late_line_lengths[1] +
                  late_line_lengths[2] + late_line_lengths[3]) / 4.0F * multiplier;

        // Include the echo transformation (see below).
        length = Math::lerp(length, echo_time, echo_depth);

        length += (late_allpass_lengths[0] + late_allpass_lengths[1] +
                   late_allpass_lengths[2] + late_allpass_lengths[3]) / 4.0F * multiplier;

        // The density gain calculation uses an average decay time weighted by
        // approximate bandwidth.  This attempts to compensate for losses of
        // energy that reduce decay time due to scattering into highly attenuated
        // bands.

        band_weights[0] = lf_w;
        band_weights[1] = hf_w - lf_w;
        band_weights[2] = Math::tau - hf_w;

        late_.density_gain = calc_density_gain(
            calc_decay_coeff(
                length,
                ((band_weights[0] * lf_decay_time) +
                    (band_weights[1] * mf_decay_time) +
                    (band_weights[2] * hf_decay_time)) / Math::tau)
        );

        for (int i = 0; i < 4; ++i)
        {
            // Calculate the length (in seconds) of each all-pass line.
            length = late_allpass_lengths[i] * multiplier;

            // Calculate the delay offset for each all-pass line.
            late_.vec_ap.offsets[i][1] = static_cast<int>(length * frequency);

            // Calculate the length (in seconds) of each delay line.  This also
            // applies the echo transformation.  As the EAX echo depth approaches
            // 1, the line lengths approach a length equal to the echoTime.  This
            // helps to produce distinct echoes along the tail.
            length = Math::lerp(late_line_lengths[i] * multiplier, echo_time, echo_depth);

            // Calculate the delay offset for each delay line.
            late_.offsets[i][1] = static_cast<int>(length * frequency);

            // Approximate the absorption that the vector all-pass would exhibit
            // given the current diffusion so we don't have to process a full T60
            // filter for each of its four lines.
            length += Math::lerp(late_allpass_lengths[i],
                (late_allpass_lengths[0] + late_allpass_lengths[1] +
                    late_allpass_lengths[2] + late_allpass_lengths[3]) / 4.0F,
                diffusion) * multiplier;

            // Calculate the T60 damping coefficients for each line.
            calc_t60_damping_coeffs(
                length,
                lf_decay_time,
                mf_decay_time,
                hf_decay_time,
                lf_w,
                hf_w,
                late_.filters[i].lf_coeffs.data(),
                late_.filters[i].hf_coeffs.data(),
                &late_.filters[i].mid_coeff);
        }
    }

    static Mat4F matrix_mult(
        const Mat4F& a,
        const Mat4F& b)
    {
        Mat4F result;

        for (int col = 0; col < 4; ++col)
        {
            for (int row = 0;row < 4; ++row)
            {
                result(row, col) =
                    (a(row, 0) * b(0, col)) +
                    (a(row, 1) * b(1, col)) +
                    (a(row, 2) * b(2, col)) +
                    (a(row, 3) * b(3, col));
            }
        }

        return result;
    }

    static void clear_gains(
        ChannelsGains& gains)
    {
        for (auto& gain : gains)
        {
            gain.fill(0.0F);
        }
    }

    // Creates a transform matrix given a reverb vector. This works by creating a
    // Z-focus transform, then a rotate transform around X, then Y, to place the
    // focal point in the direction of the vector, using the vector length as a
    // focus strength.
    //
    // This isn't technically correct since the vector is supposed to define the
    // aperture and not rotate the perceived soundfield, but in practice it's
    // probably good enough.
    static Mat4F get_transform_from_vector(
        const float* vec)
    {
        const auto length = std::sqrt((vec[0] * vec[0]) + (vec[1] * vec[1]) + (vec[2] * vec[2]));

        // Define a Z-focus (X in Ambisonics) transform, given the panning vector
        // length.
        const auto sa = std::sin(std::min(length, 1.0F) * (Math::pi / 4.0F));

        const auto zfocus = Mat4F{{
            {1.0F / (1.0F + sa), 0.0F, 0.0F, (sa / (1.0F + sa)) / 1.732050808F,},
            {0.0F, std::sqrt((1.0F - sa) / (1.0F + sa)), 0.0F, 0.0F,},
            {0.0F, 0.0F, std::sqrt((1.0F - sa) / (1.0F + sa)), 0.0F,},
            {(sa / (1.0F + sa)) * 1.732050808F, 0.0F, 0.0F, 1.0F / (1.0F + sa),}
        }};

        // Define rotation around X (Y in Ambisonics)
        auto a = std::atan2(vec[1], std::sqrt((vec[0] * vec[0]) + (vec[2] * vec[2])));

        const auto xrot = Mat4F{{
            {1.0F, 0.0F, 0.0F, 0.0F,},
            {0.0F, 1.0F, 0.0F, 0.0F,},
            {0.0F, 0.0F,  std::cos(a), std::sin(a),},
            {0.0F, 0.0F, -std::sin(a), std::cos(a),},
        }};

        // Define rotation around Y (Z in Ambisonics). NOTE: EFX's reverb vectors
        // use a right-handled coordinate system, compared to the rest of OpenAL
        // which uses left-handed. This is fixed by negating Z, however it would
        // need to also be negated to get a proper Ambisonics angle, thus
        // cancelling it out.
        a = std::atan2(-vec[0], vec[2]);

        const auto yrot = Mat4F{{
            {1.0F, 0.0F, 0.0F, 0.0F,},
            {0.0F, std::cos(a), 0.0F, std::sin(a),},
            {0.0F, 0.0F, 1.0F, 0.0F,},
            {0.0F, -std::sin(a), 0.0F, std::cos(a),},
        }};

        // Define a matrix that first focuses on Z, then rotates around X then Y to
        // focus the output in the direction of the vector.
        return matrix_mult(yrot, matrix_mult(xrot, zfocus));
    }

    // Note: res is transposed.
    static Mat4F matrix_mult_t(
        const Mat4F& a,
        const Mat4F& b)
    {
        Mat4F result;

        for (int col = 0; col < 4; ++col)
        {
            for (int row = 0; row < 4; ++row)
            {
                result(col, row) =
                    (a(row, 0) * b(0, col)) +
                    (a(row, 1) * b(1, col)) +
                    (a(row, 2) * b(2, col)) +
                    (a(row, 3) * b(3, col));
            }
        }

        return result;
    }

    // Update the early and late 3D panning gains.
    void update_3d_panning(
        Device& device,
        const float* reflections_pan,
        const float* late_reverb_pan,
        const float gain,
        const float early_gain,
        const float late_gain)
    {
        Mat4F transform;
        Mat4F rot;

        dst_buffers_ = &device.sample_buffers_;
        dst_channel_count_ = device.channel_count_;

        // Create a matrix that first converts A-Format to B-Format, then rotates
        // the B-Format soundfield according to the panning vector.
        rot = get_transform_from_vector(reflections_pan);
        transform = matrix_mult_t(rot, a2b);
        clear_gains(early_.pan_gains);

        for (int i = 0; i < max_effect_channels; ++i)
        {
            Panning::compute_first_order_gains(
                device.channel_count_,
                device.foa_,
                transform.m_[i],
                gain * early_gain,
                early_.pan_gains[i]);
        }

        rot = get_transform_from_vector(late_reverb_pan);
        transform = matrix_mult_t(rot, a2b);
        clear_gains(late_.pan_gains);

        for (int i = 0; i < max_effect_channels; ++i)
        {
            Panning::compute_first_order_gains(
                device.channel_count_,
                device.foa_,
                transform.m_[i],
                gain * late_gain,
                late_.pan_gains[i]);
        }
    }


    //
    // Effect Processing
    //

    // Basic delay line input/output routines.
    static float delay_line_out(
        const DelayLineI* delay,
        const int offset,
        const int c)
    {
        return delay->lines[offset & delay->mask][c];
    }

    // Cross-faded delay line output routine.  Instead of interpolating the
    // offsets, this interpolates (cross-fades) the outputs at each offset.
    static float faded_delay_line_out(
        const DelayLineI* delay,
        const int off0,
        const int off1,
        const int c,
        const float mu)
    {
        return Math::lerp(delay->lines[off0 & delay->mask][c], delay->lines[off1 & delay->mask][c], mu);
    }

    static float delay_out_faded(
        const DelayLineI* delay,
        const int off0,
        const int off1,
        const int c,
        const float mu)
    {
        return faded_delay_line_out(delay, off0, off1, c, mu);
    }

    static float delay_out_unfaded(
        const DelayLineI* delay,
        const int off0,
        const int off1,
        const int c,
        const float mu)
    {
        static_cast<void>(off1);
        static_cast<void>(mu);

        return delay_line_out(delay, off0, c);
    }

    using DelayOutFunc = float (*)(
        const DelayLineI* delay,
        const int off0,
        const int off1,
        const int c,
        const float mu);

    static void delay_line_in(
        DelayLineI* delay,
        const int offset,
        const int c,
        const float in)
    {
        delay->lines[offset & delay->mask][c] = in;
    }

    static void delay_line_in4(
        DelayLineI* delay,
        int offset,
        const float in[4])
    {
        offset &= delay->mask;

        for (int i = 0; i < 4; ++i)
        {
            delay->lines[offset][i] = in[i];
        }
    }

    static void delay_line_in4_rev(
        DelayLineI* delay,
        int offset,
        const float in[4])
    {
        offset &= delay->mask;

        for (int i = 0; i < 4; ++i)
        {
            delay->lines[offset][i] = in[3 - i];
        }
    }

    void calc_modulation_delays(
        int* delays,
        const int todo)
    {
        auto index = mod_.index;
        auto range = mod_.filter;

        for (int i = 0; i < todo; ++i)
        {
            // Calculate the sinus rhythm (dependent on modulation time and the
            // sampling rate).
            const auto sinus = std::sin(Math::tau * index / mod_.range);

            // Step the modulation index forward, keeping it bound to its range.
            index = (index + 1) % mod_.range;

            // The depth determines the range over which to read the input samples
            // from, so it must be filtered to reduce the distortion caused by even
            // small parameter changes.
            range = Math::lerp(range, mod_.depth, mod_.coeff);

            // Calculate the read offset.
            delays[i] = std::lround(range * sinus);
        }

        mod_.index = index;
        mod_.filter = range;
    }

    // Applies a scattering matrix to the 4-line (vector) input.  This is used
    // for both the below vector all-pass model and to perform modal feed-back
    // delay network (FDN) mixing.
    //
    // The matrix is derived from a skew-symmetric matrix to form a 4D rotation
    // matrix with a single unitary rotational parameter:
    //
    //     [  d,  a,  b,  c ]          1 = a^2 + b^2 + c^2 + d^2
    //     [ -a,  d,  c, -b ]
    //     [ -b, -c,  d,  a ]
    //     [ -c,  b, -a,  d ]
    //
    // The rotation is constructed from the effect's diffusion parameter,
    // yielding:
    //
    //     1 = x^2 + 3 y^2
    //
    // Where a, b, and c are the coefficient y with differing signs, and d is the
    // coefficient x.  The final matrix is thus:
    //
    //     [  x,  y, -y,  y ]          n = sqrt(matrix_order - 1)
    //     [ -y,  x,  y,  y ]          t = diffusion_parameter * atan(n)
    //     [  y, -y,  x,  y ]          x = cos(t)
    //     [ -y, -y, -y,  x ]          y = sin(t) / n
    //
    // Any square orthogonal matrix with an order that is a power of two will
    // work (where ^T is transpose, ^-1 is inverse):
    //
    //     M^T = M^-1
    //
    // Using that knowledge, finding an appropriate matrix can be accomplished
    // naively by searching all combinations of:
    //
    //     M = D + S - S^T
    //
    // Where D is a diagonal matrix (of x), and S is a triangular matrix (of y)
    // whose combination of signs are being iterated.
    //
    static void vector_partial_scatter(
        float* vec,
        const float x_coeff,
        const float y_coeff)
    {
        const float f[] = {vec[0], vec[1], vec[2], vec[3],};

        vec[0] = (x_coeff * f[0]) + (y_coeff * (f[1] + -f[2] + f[3]));
        vec[1] = (x_coeff * f[1]) + (y_coeff * (-f[0] + f[2] + f[3]));
        vec[2] = (x_coeff * f[2]) + (y_coeff * (f[0] + -f[1] + f[3]));
        vec[3] = (x_coeff * f[3]) + (y_coeff * (-f[0] + -f[1] + -f[2]));
    }

    // This applies a Gerzon multiple-in/multiple-out (MIMO) vector all-pass
    // filter to the 4-line input.
    //
    // It works by vectorizing a regular all-pass filter and replacing the delay
    // element with a scattering matrix (like the one above) and a diagonal
    // matrix of delay elements.
    //
    // Two static specializations are used for transitional (cross-faded) delay
    // line processing and non-transitional processing.
    static void vector_allpass_x(
        DelayOutFunc delay_out_func,
        float* vec,
        const int offset,
        const float feed_coeff,
        const float x_coeff,
        const float y_coeff,
        const float mu,
        VecAllpass *vap)
    {
        float f[4];

        for (int i = 0; i < 4; i++)
        {
            auto input = vec[i];

            vec[i] = delay_out_func(
                &vap->delay,
                offset - vap->offsets[i][0],
                offset - vap->offsets[i][1],
                i,
                mu) - (feed_coeff * input);

            f[i] = input + (feed_coeff * vec[i]);
        }

        vector_partial_scatter(f, x_coeff, y_coeff);

        delay_line_in4(&vap->delay, offset, f);
    }

    static void vector_allpass_unfaded(
        float* vec,
        const int offset,
        const float feed_coeff,
        const float x_coeff,
        const float y_coeff,
        const float mu,
        VecAllpass* vap)
    {
        vector_allpass_x(delay_out_unfaded, vec, offset, feed_coeff, x_coeff, y_coeff, mu, vap);
    }

    static void vector_allpass_faded(
        float* vec,
        const int offset,
        const float feed_coeff,
        const float x_coeff,
        const float y_coeff,
        const float mu,
        VecAllpass* vap)
    {
        vector_allpass_x(delay_out_faded, vec, offset, feed_coeff, x_coeff, y_coeff, mu, vap);
    }

    // A helper to reverse vector components.
    static void vector_reverse(
        float vec[4])
    {
        std::swap(vec[0], vec[3]);
        std::swap(vec[1], vec[2]);
    }


    using VectorAllpassFunc = void (*)(
        float* vec,
        const int offset,
        const float feed_coeff,
        const float x_coeff,
        const float y_coeff,
        const float mu,
        VecAllpass* vap);

    // This generates early reflections.
    //
    // This is done by obtaining the primary reflections (those arriving from the
    // same direction as the source) from the main delay line.  These are
    // attenuated and all-pass filtered (based on the diffusion parameter).
    //
    // The early lines are then fed in reverse (according to the approximately
    // opposite spatial location of the A-Format lines) to create the secondary
    // reflections (those arriving from the opposite direction as the source).
    //
    // The early response is then completed by combining the primary reflections
    // with the delayed and attenuated output from the early lines.
    //
    // Finally, the early response is reversed, scattered (based on diffusion),
    // and fed into the late reverb section of the main delay line.
    //
    // Two static specializations are used for transitional (cross-faded) delay
    // line processing and non-transitional processing.
    //
    void early_reflection_x(
        VectorAllpassFunc vector_allpass_func,
        DelayOutFunc delay_out_func,
        const int todo,
        float fade,
        Samples& out)
    {
        float f[4];
        auto current_offset = offset_;

        for (int i = 0; i < todo; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                f[j] = delay_out_func(
                    &delay_,
                    current_offset - early_delay_taps_[j][0],
                    current_offset - early_delay_taps_[j][1],
                    j, fade) * early_delay_coeffs_[j];
            }

            vector_allpass_func(f, current_offset, ap_feed_coeff_, mix_x_, mix_y_, fade, &early_.vec_ap);

            delay_line_in4_rev(&early_.delay, current_offset, f);

            for (int j = 0; j < 4; ++j)
            {
                f[j] += delay_out_func(&early_.delay,
                    current_offset - early_.offsets[j][0],
                    current_offset - early_.offsets[j][1], j, fade
                ) * early_.coeffs[j];
            }

            for (int j = 0; j < 4; ++j)
            {
                out[j][i] = f[j];
            }

            vector_reverse(f);

            vector_partial_scatter(f, mix_x_, mix_y_);

            delay_line_in4(&delay_, current_offset - late_feed_tap_, f);

            current_offset += 1;
            fade += fade_step;
        }
    }

    void early_reflection_unfaded(
        const int todo,
        float fade,
        Samples& out)
    {
        early_reflection_x(vector_allpass_unfaded, delay_out_unfaded, todo, fade, out);
    }

    void early_reflection_faded(
        const int todo,
        float fade,
        Samples& out)
    {
        early_reflection_x(vector_allpass_faded, delay_out_faded, todo, fade, out);
    }

    // Applies a first order filter section.
    static float first_order_filter(
        const float in,
        const float coeffs[3],
        float state[2])
    {
        const auto out = (coeffs[0] * in) + (coeffs[1] * state[0]) + (coeffs[2] * state[1]);

        state[0] = in;
        state[1] = out;

        return out;
    }

    // Applies the two T60 damping filter sections.
    float late_t60_filter(
        const int index,
        const float in)
    {
        const auto out = first_order_filter(
            in,
            late_.filters[index].lf_coeffs.data(),
            late_.filters[index].states[0].data());

        return late_.filters[index].mid_coeff *
            first_order_filter(
                out,
                late_.filters[index].hf_coeffs.data(),
                late_.filters[index].states[1].data());
    }

    // This generates the reverb tail using a modified feed-back delay network
    // (FDN).
    //
    // Results from the early reflections are attenuated by the density gain and
    // mixed with the output from the late delay lines.
    //
    // The late response is then completed by T60 and all-pass filtering the mix.
    //
    // Finally, the lines are reversed (so they feed their opposite directions)
    // and scattered with the FDN matrix before re-feeding the delay lines.
    //
    // Two static specializations are used for transitional (cross-faded) delay
    // line processing and non-transitional processing.
    //
    void late_reverb_x(
        VectorAllpassFunc vector_allpass_func,
        DelayOutFunc delay_out_func,
        const int todo,
        float fade,
        Samples& out)
    {
        float f[4];
        int moddelay[max_update_samples];

        calc_modulation_delays(moddelay, todo);

        auto current_offset = offset_;

        for (int i = 0; i < todo; i++)
        {
            for (int j = 0; j < 4; ++j)
            {
                f[j] = delay_out_func(
                    &delay_,
                    current_offset - late_delay_taps_[j][0],
                    current_offset - late_delay_taps_[j][1],
                    j,
                    fade
                ) * late_.density_gain;
            }

            const auto current_delay = current_offset - moddelay[i];

            for (int j = 0; j < 4; ++j)
            {
                f[j] += delay_out_func(&late_.delay,
                    current_delay - late_.offsets[j][0],
                    current_delay - late_.offsets[j][1],
                    j,
                    fade);
            }

            for (int j = 0; j < 4; ++j)
            {
                f[j] = late_t60_filter(j, f[j]);
            }

            vector_allpass_func(f, current_offset, ap_feed_coeff_, mix_x_, mix_y_, fade, &late_.vec_ap);

            for (int j = 0; j < 4; ++j)
            {
                out[j][i] = f[j];
            }

            vector_reverse(f);

            vector_partial_scatter(f, mix_x_, mix_y_);

            delay_line_in4(&late_.delay, current_offset, f);

            current_offset += 1;
            fade += fade_step;
        }
    }

    void late_reverb_unfaded(
        const int todo,
        float fade,
        Samples& out)
    {
        late_reverb_x(vector_allpass_unfaded, delay_out_unfaded, todo, fade, out);
    }

    void late_reverb_faded(
        const int todo,
        float fade,
        Samples& out)
    {
        late_reverb_x(vector_allpass_faded, delay_out_faded, todo, fade, out);
    }

    // Perform the non-EAX reverb pass on a given input sample, resulting in
    // four-channel output.
    float verb_pass(
        const int todo,
        float fade,
        const Samples& input,
        Samples& early,
        Samples& late)
    {
        for (int c = 0; c < 4; ++c)
        {
            // Low-pass filter the incoming samples (use the early buffer as temp
            // storage).
            filters_[c].lp.process(todo, input[c].data(), early[0].data());

            // Feed the initial delay line.
            for (int i = 0; i < todo; ++i)
            {
                delay_line_in(&delay_, offset_ + i, c, early[0][i]);
            }
        }

        if (fade < 1.0F)
        {
            // Generate early reflections.
            early_reflection_faded(todo, fade, early);

            // Generate late reverb.
            late_reverb_faded(todo, fade, late);
            fade = std::min(1.0F, fade + (todo * fade_step));
        }
        else
        {
            // Generate early reflections.
            early_reflection_unfaded(todo, fade, early);

            // Generate late reverb.
            late_reverb_unfaded(todo, fade, late);
        }

        // Step all delays forward one sample.
        offset_ += todo;

        return fade;
    }

    // Perform the EAX reverb pass on a given input sample, resulting in four-
    // channel output.
    float eax_verb_pass(
        const int todo,
        float fade,
        const Samples& input,
        Samples& early,
        Samples& late)
    {
        for (int c = 0; c < 4; ++c)
        {
            // Band-pass the incoming samples. Use the early output lines for temp
            // storage.
            filters_[c].lp.process(todo, input[c].data(), early[0].data());
            filters_[c].hp.process(todo, early[0].data(), early[1].data());

            // Feed the initial delay line.
            for (int i = 0; i < todo; i++)
            {
                delay_line_in(&delay_, offset_ + i, c, early[1][i]);
            }
        }

        if (fade < 1.0F)
        {
            // Generate early reflections.
            early_reflection_faded(todo, fade, early);

            // Generate late reverb.
            late_reverb_faded(todo, fade, late);
            fade = std::min(1.0F, fade + (todo * fade_step));
        }
        else
        {
            // Generate early reflections.
            early_reflection_unfaded(todo, fade, early);

            // Generate late reverb.
            late_reverb_unfaded(todo, fade, late);
        }

        // Step all delays forward.
        offset_ += todo;

        return fade;
    }
}; // ReverbEffectState


EffectState* EffectStateFactory::create_reverb()
{
    return create<ReverbEffectState>();
}

// Effects
// ==========================================================================
