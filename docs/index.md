
# yoink
Pointer chaser for C, copy live data, pickle it, arena allocator, etc.

##

yoink is a pointer chasing library that allows you to copy complex data
structures in C in various ways. At its simpliest it is a manual copying
garbage collector, however you can utilize the metadata for various
operations.

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
