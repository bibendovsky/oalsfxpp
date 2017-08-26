#ifndef _AL_SOURCE_H_
#define _AL_SOURCE_H_

#include "bool.h"
#include "alMain.h"
#include "alu.h"

#define MAX_SENDS      1
#define DEFAULT_SENDS  1

#ifdef __cplusplus
extern "C" {
#endif


struct ALsource;


typedef struct ALsource {
    /** Direct filter and auxiliary send info. */
    struct {
        ALfloat Gain;
        ALfloat GainHF;
        ALfloat HFReference;
        ALfloat GainLF;
        ALfloat LFReference;
    } Direct;
    struct {
        struct ALeffectslot *Slot;
        ALfloat Gain;
        ALfloat GainHF;
        ALfloat HFReference;
        ALfloat GainLF;
        ALfloat LFReference;
    } *Send;

    /** Source state (initial, playing, paused, or stopped) */
    ALenum state;
} ALsource;


void UpdateAllSourceProps(ALCcontext *context);

ALvoid ReleaseALSources(ALCcontext *Context);

#ifdef __cplusplus
}
#endif

#endif
