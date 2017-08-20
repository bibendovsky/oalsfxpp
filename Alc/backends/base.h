#ifndef AL_BACKENDS_BASE_H
#define AL_BACKENDS_BASE_H

#include "alMain.h"


typedef struct ClockLatency {
    ALint64 ClockTime;
    ALint64 Latency;
} ClockLatency;

/* Helper to get the current clock time from the device's ClockBase, and
 * SamplesDone converted from the sample rate.
 */
inline ALuint64 GetDeviceClockTime(ALCdevice *device)
{
    return device->ClockBase + (device->SamplesDone * DEVICE_CLOCK_RES /
                                device->Frequency);
}


#endif /* AL_BACKENDS_BASE_H */
