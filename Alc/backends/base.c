
#include "config.h"

#include <stdlib.h>

#include "alMain.h"
#include "alu.h"

#include "backends/base.h"


extern inline ALuint64 GetDeviceClockTime(ALCdevice *device);

/* Base ALCbackend method implementations. */
void ALCbackend_Construct(ALCbackend *self, ALCdevice *device)
{
    self->mDevice = device;
}

void ALCbackend_Destruct(ALCbackend *self)
{
}

ALCboolean ALCbackend_reset(ALCbackend* UNUSED(self))
{
    return ALC_FALSE;
}

ALCenum ALCbackend_captureSamples(ALCbackend* UNUSED(self), void* UNUSED(buffer), ALCuint UNUSED(samples))
{
    return ALC_INVALID_DEVICE;
}

ALCuint ALCbackend_availableSamples(ALCbackend* UNUSED(self))
{
    return 0;
}

ClockLatency ALCbackend_getClockLatency(ALCbackend *self)
{
    ALCdevice *device = self->mDevice;
    ClockLatency ret;

    ret.ClockTime = GetDeviceClockTime(device);
    ATOMIC_THREAD_FENCE(almemory_order_acquire);

    /* NOTE: The device will generally have about all but one periods filled at
     * any given time during playback. Without a more accurate measurement from
     * the output, this is an okay approximation.
     */
    ret.Latency = device->UpdateSize * DEVICE_CLOCK_RES / device->Frequency *
                  maxu(device->NumUpdates-1, 1);

    return ret;
}

void ALCbackend_lock(ALCbackend *self)
{
}

void ALCbackend_unlock(ALCbackend *self)
{
}


/* Base ALCbackendFactory method implementations. */
void ALCbackendFactory_deinit(ALCbackendFactory* UNUSED(self))
{
}
