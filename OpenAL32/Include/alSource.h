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

    int PropsClean;

    /** Self ID */
    ALuint id;
} ALsource;


inline struct ALsource *LookupSource(ALCcontext *context, ALuint id)
{ return (struct ALsource*)LookupUIntMapKeyNoLock(&context->SourceMap, id); }
inline struct ALsource *RemoveSource(ALCcontext *context, ALuint id)
{ return (struct ALsource*)RemoveUIntMapKeyNoLock(&context->SourceMap, id); }

void UpdateAllSourceProps(ALCcontext *context);

ALvoid ReleaseALSources(ALCcontext *Context);

#ifdef __cplusplus
}
#endif

#endif
