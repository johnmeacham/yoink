#ifndef YOINK_PRIVATE_H
#define YOINK_PRIVATE_H
/* some private definitions we don't want to clutter our public header */
#include <inttypes.h>

#ifdef __GNUC__
#define _MALLOC \
        __attribute__((malloc))  \
        __attribute__((assume_aligned (sizeof(void*))))
#define _MALLOC_SIZE(x) __attribute__((alloc_size (x)))
#define _PRINTF(x,y) __attribute__((format (printf,x,y)))
#define _SENTINEL __attribute__((sentinel))
#else
#define _MALLOC
#define _MALLOC_SIZE(x)
#define _SENTINEL
#endif


struct chain;
struct Arena;

struct header {
        int32_t tsz;
        int16_t nptrs;
        int8_t  bptrs;
        int8_t  flags;
        void *data[];
};

struct chain {
        struct chain *next;
        struct header head;
        void *data[];
};

void _arena_add_link(struct Arena *arena, struct chain *chain);

/* round up to next pointer size */
#define _ARENA_RUP(x) (((x) + sizeof(void*) - 1)/sizeof(void*))

#endif /* end of include guard: YOINK_PRIVATE_H */
