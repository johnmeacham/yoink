#ifndef YOINK_H
#define YOINK_H

/* Arena based allocaton that lets you build up an object with an arena, then
 * yoink it out into its own arena or malloced block discarding everything else.
 *
 * In order to use the macros you should annotate which values are pointers in
 * your structures either by using BEGIN_PTRS, END_PTRS and ARENA_CALLOC, or you
 * can just call arena_alloc directly and specify where the pointers are.
 *
 * struct node {
 *      BEGIN_PTRS;
 *      struct node *left;
 *      struct node right;
 *      END_PTRS;
 *      int data;
 *      }
 *
 * Unless otherwise noted, operations are threadsafe and lock-free.
 *
 *
 * managed pointers must either point to a valid data block allocated with
 * arena_alloc or be NULL which is copied as is. Additionally to allow pointer
 * tagging in VM implementations, if the LSB of the pointer is set (i.e. it is
 * odd) then the raw pointer will be copied as is and no attempt to dereference
 * it will happen.
 *
 * */

#include <string.h>
#include <stdarg.h>
#include <stdatomic.h>
#include "resizable_buf.h"
#include "yoink_private.h"
#include "arena.h"



/* allocate some memory in an arena. The new memory will be zero filled.
 * tsz is size of allocation in number of words of size (void*), bptrs is the
 * beginning of the pointers and eptrs is the end of pointers.
 * alloc is thread-safe and non locking itself but may call malloc. */
void *arena_alloc(Arena *arena, int tsz, int bptrs, int eptrs) _MALLOC _MALLOC_SIZE(2);


/* yoink all data dependencies reachable from root into arena to, all managed
 * pointers must point to data in a valid arena or be NULL.
 *
 * If data already exists in to, it will not be copied and the existing data
 * will be returned. If you do not wish this behavior and want a full copy in
 * the same arena, yoink to a new arena and use arena_join to move the copy into
 * the original.
 *
 * */

void *arena_yoink(Arena *to, void *root);

/* yoink with multiple roots, modifies roots array in place. returns number of
 * bytes yoinked. equivalent to calling arena_yoink multiple times but is more
 * efficient.
 *
 * if always_copy is true then items will be copied even if they already exist
 * in 'target' just like arena_yoink, if it is false, pointers to 'target' will
 * not be copied. there is a cost to setting this to false on the order of the
 * size of target due to having to scan it to classify pointer types.
 * */
ssize_t arena_yoinks(Arena *target, bool always_copy, int nroots, void *roots[nroots]);


/* yoink to a continuous compact buffer that was created via a single malloc
 * call. This always makes a full independent copy of the data.
 *
 * The buffer may be freed by calling free.  Useful for APIs when you want to
 * return a complicated object with internal pointers that can be freed with
 * free or if you want a more efficient memory/cache layout for long lived
 * data and don't intend to use the arena again.
 *
 * *len will contain the length of data allocated.
 *
 * All metadata is stripped so only raw pointers remain unless keep_metadata is
 * true. If you keep metadata you can yoink from pointers into this malloced
 * space, otherwise you may not.
 *
 * the metadata on root is never kept even if keep_metadata is true so it may be
 * returned at the beginning of the malloced buffer.
 */
void *arena_yoink_to_malloc(void *what, size_t *len, bool keep_metadata);

/*
 * This is equivalent to
 *
 * Arena new = ARENA_INIT;
 * arena_yoinks(&new, nroots, roots);
 * arena_free(bowl);
 * *bowl = new;
 *
 * however it is free to reuse data in new intsead of copying it.
 * but may be more efficient and reuse data that is already in bowl however
 * reuse is not guerenteed. yoinks behaves as it would otherwise and copies
 * data.
 *
 * This is roughly the equivalent of yoinking into a new arena and replacing the
 * old arena with the new one however no data is copied so all pointers remain
 * valid and it is much more efficient. only things reachable from roots are
 * kept.  Returns the number of bytes freed by the vacuuming.  If validate is
 * true it will raise an error and return -1 if any pointers are found that
 * point outside the arena, otherwise it will ignore them. vacuum is not
 * threadsafe as it modifies the arena in place and won't catch pointers that
 * mutate while it is running.
 * */

ssize_t arena_vacuums(Arena *bowl, int nroots, void *roots[nroots]);


/* Initialize a buffer for inclusion in an arena. the buffer will be seeded with
 * appropriate bookkeeping data that you should not modify and take into account
 * when looking at rb_len.
 *
 * Until finalize is called, the buffer is fully owned and must be freed by you
 * if you choose not to finalize it.
 * Once finalize is called the memory is managed by and will be freed with the
 * arena.
 *
 * This can be used for building up arrays incrementally.
 *
 * if pointer_array is true the data will be assumed to be an array of managed
 * pointers, otherwise it is raw uninterpreted data. */
void arena_initialize_buffer(Arena *bowl, rb_t *rb);
void *arena_finalize_buffer(Arena *bowl, rb_t *rb, bool pointer_array);

/* useful macros so you don't have to get the number of pointers right. */

/* if zero length arrays don't work then an empty struct might */
#define BEGIN_PTRS struct { void *_dummy; } _arena_begin_ptrs[0]
/* if zero length arrays don't work then an empty struct might */
#define END_PTRS struct { void *_dummy; } _arena_end_ptrs[0]


#define ARENA_CALLOC(arena, x) \
    arena_alloc(arena, sizeof(x), \
        ((char *)&(x)._arena_begin_ptrs - (char *)&(x))/sizeof(void *), \
        ((char *)&(x)._arena_end_ptrs - (char *)&(x))/sizeof(void *))

/* these freeze and thaw an arena to a pickled version that can be copied
 * around or stored.
 *
 * It may even be stored to disk on machines of the same architecture and
 * endianness which is useful for a cache or distributing data between machines
 * with the same memory model.
 *
 * data may be thawed in-place and directly used.
 */

/*
 * freezes the data to a single contiguous buffer in a machine dependent
 * serializable format.
 *
 * the frozen data is location independent so may be copied around, duplicated
 * or even stored to disk (However the format will be machine dependent so it
 * should only be used for caches and not primary data.)
 *
 * performs a yoink under the hood, nothing not pointed to by ptr will be pulled
 * in.
 *
 * key is included with the frozen data and may be passed to thaw which will
 * verify it before unfreezing. This can be used for very basic error detection
 * or versioning.
 *
 * Unlike yoinking, this will validate all pointers point to sothing in from or
 * are NULL and will fail if this is not the case.
 *
 * if the unsafe flag is set to true, it will behave like other yoinks and
 * serialize unknown pointers rather than attempt to follow them. This is
 * dangerous as it could lead to a corrupt frozen stream.
 *
 * */

void arena_freeze(rb_t *to, void *, int key);

/* This thaws data _in place_. the data will not be associated with an arena but
 * will still reside in *ptr which is still owned by the caller of thaw. It may
 * be referenced by other arena allocated data directly though and things will
 * work properly. If you want to put the data in an arena, it can be yoinked
 * out.
 *
 * thaw data from from *ptr, *ptr will be changed to the end of the frozen
 * data, length is the size of data pointed to.
 *
 * some minimal checking is done, if an error occurs then NULL is returned and
 * *ptr is unmodified.
 *
 * if the key passed in is not the same as the one used to freeze the data, then
 * NULL is returned.
 * */

void *arena_thaw(int key, size_t len, void **ptr);


/* get the key from the frozen data passed to freeze, this can be used for basic
 * validation and versioning */
int arena_check_frozen_key(void *data);

#endif /* end of include guard: YOINK_H */
