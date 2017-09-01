#ifndef AL_MATH_DEFS_H
#define AL_MATH_DEFS_H

constexpr auto F_PI = 3.14159265358979323846F;
constexpr auto F_PI_2 = 1.57079632679489661923F;
constexpr auto F_TAU = 6.28318530717958647692F;

#define DEG2RAD(x)  ((float)(x) * (F_PI/180.0f))
#define RAD2DEG(x)  ((float)(x) * (180.0f/F_PI))

#endif /* AL_MATH_DEFS_H */
