# yoink
Pointer chaser for C, copy live data, pickle it, arena allocator, etc.

yoink is a pointer chasing library that allows you to copy complex data
structures in C in various ways. At its simpliest it is a manual copying
garbage collector, however you can utilize the metadata for various
operations. It is only a few C files and can be directly included in
projects or added as a submodule.

The original motivation was for a compiler, where each phase lexing,
parsing, optimization, etc. can run without caring about memory and the final
result of the pass can be 'yoink'ed out.

Some things it lets you do.

  * Allocate data in an arena and yoink some data and all its dependencies
    into a new arena. arbitrarily complex data structures can be used with
    internal pointers.
  * Create a complete copy of a complex data structure such as a linked list
    or tree.
  * take a complicated object and place it into a single contiguous memory
    chunk allocated with malloc. very useful for writing APIs where you want
    to hide the complexity of your internal representation but let someone
    treat it as malloced memory that can be freed with free.
  * serialize data in a way it can be stored to disk or replicated via
    freeze and thaw. The format is machine specific, but can be used as a
    cache, or to record intermediate results to disk.

What it can be used for

  * Arena allocator, quickly allocate a bunch of memory and free it all at
    once.
  * Copying garbage collector. yoink your results when you want to GC, free
    the old arena.
  * Data pickling/serialization. Serialize C data structures to disk.

# API

## Allocating and yoinking some data

An arena is a region of memory where you can allocate new data, but the
entire arena is freed at once.

In order for yoink to know where the pointers are in your structure you
should use the BEGIN_PTRS and END_PTRS macros to delineate where the
pointers are.

```c
 struct node {
      BEGIN_PTRS;
      struct node *left;
      struct node right;
      END_PTRS;
      int data;
      }
```

 Now in order to use it, you can do the following



```c
Arena the_arena = ARENA_INIT;

struct node *root = ARENA_CALLOC(&the_arena, root);
struct node *n1 = ARENA_CALLOC(&the_arena, n1);
struct node *n2 = ARENA_CALLOC(&the_arena, n2);

root->left = n1;
n1->left  = n2;

Arena new_arena = ARENA_INIT;
struct node *new = yoink_to_arena(&new_arena,n1);
arena_free(&the_arena);

```

After this, the_arena has been freed and is empty, and new_arena contains
a copy of n1 and n2 while root has been freed. new points to the copy of n1.

## yoinking

simply yoink some data into the arena to.

```c
void *yoink_to_arena(Arena *to, void *root);
```

More complicated version of yoink, can yoink multiple roots and return the
size of the data copied. roots is updated in place with the new pointers.
```c
ssize_t yoinks_to_arena(Arena *to, int nroots, void *roots[nroots]);
```

yoink to a continuous compact buffer that was created via a single malloc
call. This always makes a full independent copy of the data.

The buffer may be freed by calling free.  Useful for APIs when you want to
return a complicated object with internal pointers that can be freed with
free or if you want a more efficient memory/cache layout for long lived
data and don't intend to use the arena again.

*len will contain the length of data allocated.

root must be a valid pointer or NULL in which case NULL is returned.

```c
void *yoink_to_malloc(void *root, size_t *len);
```

Set flags on an alloction that modify how data is copied. This allows
flagging some data as epheremel that should not be copied, instead the
pointer will be replaced by NULL. this can be used to free up cached data
that is no longer important, or data that can be regenerated and we don't
want to store it to disk.

valid flags

 * YFLAG_NULL_CHILDREN - do not copy any children of this node and instead set all pointers to NULL
 * YFLAG_NULL_SELF     - do not copy this pointer ever, instead it is replaced by NULL.
 * YFLAG_ALIAS_SELF    - do not copy this pointer but maintain the pointer
   to the shared version.
 *
```c
uint32_t yoink_set_flags(void *, uint32_t flags);
```

## freezing and thawing

You can freeze and thaw data, frozen data is contiguous data that can be
stored to disk or copied, and then thawed out in place into usable data
structures.

```c

// open ended structure that appears at the beginning of frozen data
struct frozen {
        uintptr_t length;  // length in bytes including frozen header
        ...
        void *data[];
};

struct frozen *yoink_freeze(void *, struct frozen *ice);

void *yoink_thaw(struct frozen *ice);
```
