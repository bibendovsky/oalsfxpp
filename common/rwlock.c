
#include "config.h"

#include "rwlock.h"

#include "bool.h"


/* A simple spinlock. Yield the thread while the given integer is set by
 * another. Could probably be improved... */
#define LOCK(l)
#define UNLOCK(l)


void RWLockInit(RWLock *lock)
{
    lock->read_count = 0;
    lock->write_count = 0;
    lock->read_lock = 0;
    lock->read_entry_lock = 0;
    lock->write_lock = 0;
}

void ReadLock(RWLock *lock)
{
    LOCK(lock->read_entry_lock);
    LOCK(lock->read_lock);
    if((lock->read_count++) == 0)
        LOCK(lock->write_lock);
    UNLOCK(lock->read_lock);
    UNLOCK(lock->read_entry_lock);
}

void ReadUnlock(RWLock *lock)
{
    /* NOTE: ATOMIC_SUB returns the *old* value! */
    if((lock->read_count--) == 1)
        UNLOCK(lock->write_lock);
}

void WriteLock(RWLock *lock)
{
    if((lock->write_count++) == 0)
        LOCK(lock->read_lock);
    LOCK(lock->write_lock);
}

void WriteUnlock(RWLock *lock)
{
    UNLOCK(lock->write_lock);
    if((lock->write_count--) == 1)
        UNLOCK(lock->read_lock);
}
