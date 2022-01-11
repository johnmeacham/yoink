#ifndef YOINK_PRIVATE_H
#define YOINK_PRIVATE_H
/* some private definitions we don't want to clutter our public header */

#ifdef __GNUC__
#define _MALLOC \
        __attribute__((malloc))  \
        __attribute__((assume_aligned (sizeof(void*))))
#define _MALLOC_SIZE(x) __attribute__((alloc_size (x)))
#define _PRINTF(x,y) __attribute__((format (printf,x,y)))
#else
#define _MALLOC
#define _MALLOC_SIZE(x)
#endif

struct chain;

#endif /* end of include guard: YOINK_PRIVATE_H */
