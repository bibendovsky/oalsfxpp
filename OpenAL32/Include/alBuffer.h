#ifndef _AL_BUFFER_H_
#define _AL_BUFFER_H_

#include "alMain.h"


/* User formats */
enum UserFmtType {
    UserFmtByte   = AL_BYTE_SOFT,
    UserFmtUByte  = AL_UNSIGNED_BYTE_SOFT,
    UserFmtShort  = AL_SHORT_SOFT,
    UserFmtUShort = AL_UNSIGNED_SHORT_SOFT,
    UserFmtFloat  = AL_FLOAT_SOFT
};
enum UserFmtChannels {
    UserFmtMono      = AL_MONO_SOFT,
    UserFmtStereo    = AL_STEREO_SOFT
};


/* Storable formats */
enum FmtType {
    FmtByte  = UserFmtByte,
    FmtShort = UserFmtShort,
    FmtFloat = UserFmtFloat,
};
enum FmtChannels {
    FmtMono   = UserFmtMono,
    FmtStereo = UserFmtStereo
};

constexpr auto MAX_INPUT_CHANNELS = 8;


#endif
