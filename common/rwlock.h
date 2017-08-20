#ifndef AL_RWLOCK_H
#define AL_RWLOCK_H

#include "bool.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned int read_count;
    unsigned int write_count;
    int read_lock;
    int read_entry_lock;
    int write_lock;
} RWLock;
#define RWLOCK_STATIC_INITIALIZE { 0, 0, 0, 0, 0 }

void RWLockInit(RWLock *lock);
void ReadLock(RWLock *lock);
void ReadUnlock(RWLock *lock);
void WriteLock(RWLock *lock);
void WriteUnlock(RWLock *lock);

#ifdef __cplusplus
}
#endif

#endif /* AL_RWLOCK_H */
