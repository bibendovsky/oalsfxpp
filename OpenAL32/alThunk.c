/**
 * OpenAL cross platform audio library
 * Copyright (C) 1999-2007 by authors.
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 */

#include "config.h"

#include <stdlib.h>

#include "alMain.h"
#include "alThunk.h"

#include "almalloc.h"


static int *ThunkArray;
static ALsizei      ThunkArraySize;
static RWLock ThunkLock;

void ThunkInit(void)
{
    RWLockInit(&ThunkLock);
    ThunkArraySize = 1024;
    ThunkArray = al_calloc(16, ThunkArraySize * sizeof(*ThunkArray));
}

void ThunkExit(void)
{
    al_free(ThunkArray);
    ThunkArray = NULL;
    ThunkArraySize = 0;
}

ALenum NewThunkEntry(ALuint *index)
{
    void *NewList;
    ALsizei i;

    ReadLock(&ThunkLock);
    for(i = 0;i < ThunkArraySize;i++)
    {
        int old_thunk = ThunkArray[i];
        ThunkArray[i] = 1;
        if(!old_thunk)
        {
            ReadUnlock(&ThunkLock);
            *index = i+1;
            return AL_NO_ERROR;
        }
    }
    ReadUnlock(&ThunkLock);

    WriteLock(&ThunkLock);
    /* Double-check that there's still no free entries, in case another
     * invocation just came through and increased the size of the array.
     */
    for(;i < ThunkArraySize;i++)
    {
        int old_thunk = ThunkArray[i];
        ThunkArray[i] = 1;
        if(!old_thunk)
        {
            WriteUnlock(&ThunkLock);
            *index = i+1;
            return AL_NO_ERROR;
        }
    }

    NewList = al_calloc(16, ThunkArraySize*2 * sizeof(*ThunkArray));
    if(!NewList)
    {
        WriteUnlock(&ThunkLock);
        ERR("Realloc failed to increase to %u entries!\n", ThunkArraySize*2);
        return AL_OUT_OF_MEMORY;
    }
    memcpy(NewList, ThunkArray, ThunkArraySize*sizeof(*ThunkArray));
    al_free(ThunkArray);
    ThunkArray = NewList;
    ThunkArraySize *= 2;

    ThunkArray[i] = 1;
    *index = ++i;

    for(;i < ThunkArraySize;i++)
        ThunkArray[i] = 0;
    WriteUnlock(&ThunkLock);

    return AL_NO_ERROR;
}

void FreeThunkEntry(ALuint index)
{
    ReadLock(&ThunkLock);
    if(index > 0 && (ALsizei)index <= ThunkArraySize)
        ThunkArray[index-1] = 0;
    ReadUnlock(&ThunkLock);
}
