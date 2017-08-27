
#include "config.h"

#include "almalloc.h"

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <windows.h>

void *al_malloc(size_t alignment, size_t size)
{
    return _aligned_malloc(size, alignment);
}

void *al_calloc(size_t alignment, size_t size)
{
    void *ret = al_malloc(alignment, size);
    if(ret) memset(ret, 0, size);
    return ret;
}

void al_free(void *ptr)
{
    _aligned_free(ptr);
}
