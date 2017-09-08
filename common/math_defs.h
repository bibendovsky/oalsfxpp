#ifndef AL_MATH_DEFS_H
#define AL_MATH_DEFS_H


constexpr auto pi = 3.14159265358979323846F;
constexpr auto pi_2 = 1.57079632679489661923F;
constexpr auto tau = 6.28318530717958647692F;


constexpr float deg_to_rad(
    const float x)
{
    return x * (pi / 180.0F);
}


#endif /* AL_MATH_DEFS_H */
