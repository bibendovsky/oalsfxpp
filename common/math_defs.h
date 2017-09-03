#ifndef AL_MATH_DEFS_H
#define AL_MATH_DEFS_H


constexpr auto F_PI = 3.14159265358979323846F;
constexpr auto F_PI_2 = 1.57079632679489661923F;
constexpr auto F_TAU = 6.28318530717958647692F;


constexpr float DEG2RAD(
    const float x)
{
    return x * (F_PI / 180.0F);
}


#endif /* AL_MATH_DEFS_H */
