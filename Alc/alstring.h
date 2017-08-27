#ifndef ALSTRING_H
#define ALSTRING_H

#include <string.h>

#include "vector.h"


typedef char al_string_char_type;
TYPEDEF_VECTOR(al_string_char_type, al_string)
TYPEDEF_VECTOR(al_string, vector_al_string)

inline void alstr_reset(al_string *str)
{ VECTOR_DEINIT(*str); }
#define AL_STRING_INIT(_x)       do { (_x) = (al_string)NULL; } while(0)
#define AL_STRING_INIT_STATIC()  ((al_string)NULL)
#define AL_STRING_DEINIT(_x)     alstr_reset(&(_x))

inline size_t alstr_length(const_al_string str)
{ return VECTOR_SIZE(str); }

inline ALboolean alstr_empty(const_al_string str)
{ return alstr_length(str) == 0; }

inline const al_string_char_type *alstr_get_cstr(const_al_string str)
{ return str ? &VECTOR_FRONT(str) : ""; }

void alstr_clear(al_string *str);

int alstr_cmp(const_al_string str1, const_al_string str2);
int alstr_cmp_cstr(const_al_string str1, const al_string_char_type *str2);

void alstr_append_char(al_string *str, const al_string_char_type c);
void alstr_append_cstr(al_string *str, const al_string_char_type *from);
void alstr_append_range(al_string *str, const al_string_char_type *from, const al_string_char_type *to);


#endif /* ALSTRING_H */
