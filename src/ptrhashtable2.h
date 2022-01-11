#ifndef PTRHASHTABLE_H
#define PTRHASHTABLE_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
/* simple hash table mapping uintptrs to uintptrs
 * this maps a uintptr_t to an array of uintptr_ts.
 *
 * The array size is set by the constructor and may be zero so you can use it
 * like a set.
 *
 * This was designed mainly to attach metadata to pointers during a garbage
 * collection, so it is fairly optimized for that and not a general hash table
 * implementation.
 */

/* number of entries that are reserved for internal use so are stored directly
 * in the HashTable struct so they don't collide. */
#define _RESERVED_ENTRIES 1

/* these can be typedefed to other integer types between 16 and 64 bits
 * inclusive if you like */
typedef uintptr_t Key;
typedef uintptr_t Value;

struct hash_table;
typedef struct HashTable {
        struct hash_table *ht;
        Value *res[_RESERVED_ENTRIES];
        int vsize;
} HashTable;

/* vsize should be the number of words that will be in the value field. zero is
 * allowed to make it behave like a set */
#define HASHTABLE_INIT(vsize) { .vsize = vsize }
#define HASHSET_INIT          { .vsize = 0 }
#define HASHMAP_INIT          { .vsize = 1 }

/* Get value, returns NULL if key is not in map. If vsize is zero and the key is
 * in the set this will return an unspecified pointer that is not NULL */
Value *ht_get(HashTable *ht, Key k);

/* insert entry, returns true if a new entry was created and false if it already
 * existed. */
bool ht_ins(HashTable *ht, Key k, Value **v);

/* iterate over entries in an arbitrary order, index is the internal state of
 * the iterator and will be set to zero when no more values are left and should
 * be initialized to zero to begin iteration. */
Key ht_next(HashTable *ht, uintptr_t *index, Value **v);

/* free all resources associated with a hashtable */
void ht_free(HashTable *ht);

/* specialized versions of ht_get and ht_ins */

/* This is a version of ht_ins that does not check if an entry already exists
 * and may or may not return existing data. It can be faster by avoiding
 * migrating the data to a new location if you are just going to overwrite it
 * anyway. */
Value *ht_set(HashTable *ht, Key k);

/* return whether k is in the table */
bool ht_in(HashTable *ht, Key k);
/* add k as a new entry, returns true if it didn't exist before */
bool ht_add(HashTable *ht, Key k);

/* dump table information */
void ht_dump(HashTable *ht);

#endif
