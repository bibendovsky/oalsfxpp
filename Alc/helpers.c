/**
 * OpenAL cross platform audio library
 * Copyright (C) 2011 by authors.
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
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <malloc.h>

#include "alMain.h"
#include "alstring.h"


extern inline size_t alstr_length(const_al_string str);
extern inline ALboolean alstr_empty(const_al_string str);

void alstr_clear(al_string *str)
{
    if(!alstr_empty(*str))
    {
        /* Reserve one more character than the total size of the string. This
         * is to ensure we have space to add a null terminator in the string
         * data so it can be used as a C-style string.
         */
        VECTOR_RESIZE(*str, 0, 1);
        VECTOR_ELEM(*str, 0) = 0;
    }
}

static inline int alstr_compare(const al_string_char_type *str1, size_t str1len,
                                const al_string_char_type *str2, size_t str2len)
{
    size_t complen = (str1len < str2len) ? str1len : str2len;
    int ret = memcmp(str1, str2, complen);
    if(ret == 0)
    {
        if(str1len > str2len) return  1;
        if(str1len < str2len) return -1;
    }
    return ret;
}
int alstr_cmp(const_al_string str1, const_al_string str2)
{
    return alstr_compare(&VECTOR_FRONT(str1), alstr_length(str1),
                         &VECTOR_FRONT(str2), alstr_length(str2));
}
int alstr_cmp_cstr(const_al_string str1, const al_string_char_type *str2)
{
    return alstr_compare(&VECTOR_FRONT(str1), alstr_length(str1),
                         str2, strlen(str2));
}

void alstr_append_char(al_string *str, const al_string_char_type c)
{
    size_t len = alstr_length(*str);
    VECTOR_RESIZE(*str, len, len+2);
    VECTOR_PUSH_BACK(*str, c);
    VECTOR_ELEM(*str, len+1) = 0;
}

void alstr_append_cstr(al_string *str, const al_string_char_type *from)
{
    size_t len = strlen(from);
    if(len != 0)
    {
        size_t base = alstr_length(*str);
        size_t i;

        VECTOR_RESIZE(*str, base+len, base+len+1);
        for(i = 0;i < len;i++)
            VECTOR_ELEM(*str, base+i) = from[i];
        VECTOR_ELEM(*str, base+i) = 0;
    }
}

void alstr_append_range(al_string *str, const al_string_char_type *from, const al_string_char_type *to)
{
    size_t len = to - from;
    if(len != 0)
    {
        size_t base = alstr_length(*str);
        size_t i;

        VECTOR_RESIZE(*str, base+len, base+len+1);
        for(i = 0;i < len;i++)
            VECTOR_ELEM(*str, base+i) = from[i];
        VECTOR_ELEM(*str, base+i) = 0;
    }
}
