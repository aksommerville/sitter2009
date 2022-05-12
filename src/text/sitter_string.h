#ifndef SITTER_STRING_H
#define SITTER_STRING_H

#include <stdint.h>

// how much prefix do a and b have in common? (case-insensitive)
int sitter_strlenicmp(const char *a,const char *b);

/* Break a string into a newly-allocated array of newly-allocated strings.
 * Array is NULL-terminated.
 * Array is allocated even when <s> is empty or NULL.
 * 'selfDescribing' means the first character of s is the delimiter (and the
 * list begins at s[1]).
 * 'white' means break on any whitespace and treat 2+ white characters as one.
 * Lists returned by 'white' will never contain empty items.
 */
char **sitter_strsplit_selfDescribing(const char *s);
char **sitter_strsplit_white(const char *s);

/* Fast, unportable hash. Values should not be saved.
 */
uint32_t sitter_strhash(const char *s,int slen=-1);

#endif
