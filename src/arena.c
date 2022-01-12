
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include "arena.h"
#include "resizable_buf.h"


static void add_link(Arena *arena, struct chain *chain) {
        struct chain *orig = atomic_load(&arena->chain);
        do {
                chain->next = orig;
        } while (!atomic_compare_exchange_weak(&arena->chain, &orig, chain));
}
void *arena_malloc(Arena *arena, size_t size)
{
        size = _ARENA_RUP(size) * sizeof(void *); // round up
//        printf("arena_alloc(_,%i,%i,%i)\n", tsz, bptrs, eptrs);
        size_t needed = sizeof(struct chain) + size;
        struct chain *chain = malloc(needed);
        if(!chain) {
                fprintf(stderr, "arena_malloc error: %s", strerror(errno));
                abort();
        }
        memset(chain, 0, sizeof(struct chain));
        chain->head.tsz  = size;
        add_link(arena, chain);
        return chain->data;
}

void arena_move(Arena *to, Arena *from) {
        struct chain *orig = atomic_load(&from->chain);
        while (!atomic_compare_exchange_weak(&from->chain, &orig, NULL));
        struct chain *last = orig;
        while(last->next)
                last = last->next;
        struct chain *torig =  atomic_load(&to->chain);
        do {
                last->next = torig;
        }
        while (!atomic_compare_exchange_weak(&to->chain, &torig, orig));
}

void arena_free(Arena *arena)
{
        struct chain *orig = atomic_load(&arena->chain);
        while (!atomic_compare_exchange_weak(&arena->chain, &orig, NULL));
        while (orig) {
                struct chain *nnext = orig->next;
                free(orig);
                orig = nnext;
        };
        assert(!arena->chain);
}

char *
arena_strdup(Arena *bowl, char *s) {
        size_t len = strlen(s) + 1;
        char *ret = arena_malloc(bowl, len);
        return memcpy(ret, s, len);
}

char *
arena_strndup(Arena *bowl, char *s, size_t n) {
        size_t len = strlen(s);
        len = (len > n ? n : len);
        char *ret = arena_malloc(bowl, len + 1);
        ret[len] = 0;
        return memcpy(ret, s, len);
}

char *
arena_vprintf(Arena *bowl, char *fmt, va_list ap) {
        va_list cap;
        va_copy(cap, ap);
        int size = vsnprintf(NULL, 0, fmt, cap);
        va_end(cap);
        if (size < 0)
                return NULL;
        char *ret = arena_malloc(bowl, size + 1);
        size = vsnprintf(ret, size + 1, fmt, ap);
        if (size < 0)
                return NULL;
        return ret;
}

char *
arena_printf(Arena *bowl, char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        char *ret = arena_vprintf(bowl, fmt, ap);
        va_end(ap);
        return ret;
}

void *
arena_memcpy(Arena *bowl, void *data, size_t len) {
        return memcpy(arena_malloc(bowl, len), data, len);
}

void
arena_initialize_buffer(rb_t *buf) {
        /* clear buffer */
        rb_free(buf);
        rb_calloc(buf, sizeof(struct chain));
}

void *
arena_finalize_buffer(Arena *bowl, rb_t *buf) {
        /* make sure we have some breathing room to keep alignments correct. */
        int tsz = _ARENA_RUP(rb_len(buf));
        rb_calloc(buf, tsz*sizeof(void*) - rb_len(buf));
        struct chain *chain = rb_take(buf);
        chain->head.tsz = tsz;
        add_link(bowl, chain);
        return chain->data;
}
