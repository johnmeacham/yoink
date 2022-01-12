#ifndef ARENA_H
#define ARENA_H

#include <stdatomic.h>
#include <stdarg.h>
#include "yoink_private.h"

struct Arena {
        struct chain *_Atomic chain;
};
typedef struct Arena Arena;
#define ARENA_INIT { .chain = ATOMIC_VAR_INIT(NULL) }

/* malloc allocates raw bytes without internal structure that will be freed when
 * the arena is freed. */
void *arena_malloc(Arena *arena, size_t n) _MALLOC _MALLOC_SIZE(2);
/* free an arena and all memory allocated by it.
 * Arena is left as a valid empty arena after this call.
 * arena_free is threadsafe itself in that multiple
 * concurrent calls to it will not double-free or leak memory, but memory freed
 * by arena_free is no longer valid.  */
void arena_free(Arena *arena);

/* move memory from one arena to another. after this from is cleared and to
 * contains both to and from */
void arena_join(Arena *to, Arena *from);

/* utility routines to allocate strings and raw data in an arena. behave like
 * the c standard library routines but allocate return data in the arena */
char *arena_printf(Arena *arena, char *fmt, ...) _MALLOC _PRINTF(2, 3);
char *arena_vprintf(Arena *arena, char *fmt, va_list ap) _MALLOC;
char *arena_strdup(Arena *arena, char *s) _MALLOC;
char *arena_strndup(Arena *arena, char *s, size_t n) _MALLOC;
void *arena_memcpy(Arena *arena, void *data, size_t len) _MALLOC;

#endif /* end of include guard: ARENA_H */
