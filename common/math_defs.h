#ifndef AL_MATH_DEFS_H
#define AL_MATH_DEFS_H

#include <math.h>
#include <float.h>

#define F_PI    (3.14159265358979323846f)
#define F_PI_2  (1.57079632679489661923f)
#define F_TAU   (6.28318530717958647692f)

#define DEG2RAD(x)  ((float)(x) * (F_PI/180.0f))
#define RAD2DEG(x)  ((float)(x) * (180.0f/F_PI))

#endif /* AL_MATH_DEFS_H */
