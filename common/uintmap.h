#ifndef AL_UINTMAP_H
#define AL_UINTMAP_H

#include "AL/al.h"
#include "bool.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UIntMap {
    ALuint *keys;
    /* Shares memory with keys. */
    ALvoid **values;

    ALsizei size;
    ALsizei capacity;
    ALsizei limit;
} UIntMap;
#define UINTMAP_STATIC_INITIALIZE_N(_n) { NULL, NULL, 0, 0, (_n), RWLOCK_STATIC_INITIALIZE }
#define UINTMAP_STATIC_INITIALIZE UINTMAP_STATIC_INITIALIZE_N(INT_MAX)

void InitUIntMap(UIntMap *map, ALsizei limit);
void ResetUIntMap(UIntMap *map);
void RelimitUIntMapNoLock(UIntMap *map, ALsizei limit);
ALenum InsertUIntMapEntry(UIntMap *map, ALuint key, ALvoid *value);
ALenum InsertUIntMapEntryNoLock(UIntMap *map, ALuint key, ALvoid *value);
ALvoid *RemoveUIntMapKey(UIntMap *map, ALuint key);
ALvoid *RemoveUIntMapKeyNoLock(UIntMap *map, ALuint key);
ALvoid *LookupUIntMapKey(UIntMap *map, ALuint key);
ALvoid *LookupUIntMapKeyNoLock(UIntMap *map, ALuint key);

inline void LockUIntMapRead(UIntMap *map)
{}
inline void UnlockUIntMapRead(UIntMap *map)
{}
inline void LockUIntMapWrite(UIntMap *map)
{}
inline void UnlockUIntMapWrite(UIntMap *map)
{}

#ifdef __cplusplus
}
#endif

#endif /* AL_UINTMAP_H */
