#include "ptrhashtable2.h"
#include "inthash.h"
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// best guess as to cache line, nothing too bad happens if it is wrong
#define CACHE_LINE 64

#define DIST 5
#define INIT_ORDER 3
#define PREEMPTIVE_RESIZE false
#define USE_MAX_DIST true

struct hash_table {
        Key *ks;
        Value *vs;
        int count;
        int order;
        // derived values
        int size;    // 1 << order + red zone
        int dist;
        int mask;   // mask down to order (not including red zone)
};

_INTHASH_GENERATE(Key, key)

/* return the value pointer given the key pointer */
#define VPTR(ht,kp,vsize) ((Value *)(ht->vs + vsize * ((kp) - ht->ks)))

static void ifree(struct hash_table *ht)
{
        free(ht->ks);
        free(ht->vs);
        free(ht);
}


/* fast path */
static Key *
ihash_get(struct hash_table *ht, Key k)
{
        //printf("----%u %x %x %x %x %lx\n",dist,omask,mask,hk, orig, k);
        for (unsigned int i = k, j = 0; j < ht->dist; j++, i++) {
                i &= ht->mask;
                Key *pk = &ht->ks[i];
                if (!*pk || *pk == k)
                        return pk;
        }
        return NULL;
}

static struct hash_table *alloc_table(int order, int vsize)
{
        struct hash_table *ht = calloc(1, sizeof(*ht));
        ht->order = order;
        ht->size = (1 << order); //   + (1 << DIST);
        ht->mask = (1 << order) - 1;
        ht->dist = !USE_MAX_DIST || order < DIST ? ht->size : 1 << DIST;
        ht->ks = aligned_alloc(CACHE_LINE, (ht->size * sizeof(Key)));
        memset(ht->ks, 0, ht->size * sizeof(Key));
        ht->vs = calloc(!vsize + ht->size, !vsize + sizeof(Value) * vsize);
//        printf("malloc %i %i %x %i\n", size, vsize, size, ht->dist);
        return ht;
}

static struct hash_table *
grow_hash_table(struct hash_table *ht, int vsize)
{
        printf("--- %i %i %i\n",
               ht->count, (ht->size), (ht->size) - ((ht->size - 2)));
        struct hash_table *nht = alloc_table(ht->order + 1, vsize);
        nht->count = ht->count;
        for (int i = 0; i < ht->size; i++) {
                if (!ht->ks[i])
                        continue;
                Key *kp = ihash_get(nht, ht->ks[i]);
                assert(kp && !*kp);
                *kp = ht->ks[i];
                for (int k = 0; k < vsize; k++)
                        VPTR(nht, kp, vsize)[k] = VPTR(ht, ht->ks + i, vsize)[k];
        }
        ifree(ht);
        return nht;
}

/* vsize is only relevant if we need to grow the table */
static Key *
ihash_ins(struct hash_table **pht, Key k, int vsize, bool *added)
{
        assert(k >= _RESERVED_ENTRIES);
        *added = false;
        struct hash_table *ht = *pht;
//        assert(ht->count <= ht->size);
        Key *kp = ihash_get(ht, k);
        if (!kp || *kp != k) {
                /* resize the table if needed. */
                while (!kp || (PREEMPTIVE_RESIZE &&  ht->count >= ht->size - (ht->size >> 2))) {
                        *pht = ht = grow_hash_table(ht, vsize);
                        kp = ihash_get(ht, k);
                        assert(!kp || !*kp);
                }
                *added = true;
                *kp = k;
                ht->count++;
        }
        return kp;
}

bool
ht_ins(HashTable *pht, Key k, Value **v)
{
        if (!pht->ht)
                pht->ht =  alloc_table(INIT_ORDER, pht->vsize);
        if (k < _RESERVED_ENTRIES)  {
                if (!pht->res[k]) {
                        *v = pht->res[k] = calloc(pht->vsize + !pht->vsize, sizeof(Value));
                        return true;
                }
                *v = pht->res[k];
                return false;
        }
        bool added = false;
        Key hk = hash_key(k);
        Key *kp =  ihash_ins(&pht->ht, hk, pht->vsize, &added);
        assert(kp && *kp == hk);
        *v = VPTR(pht->ht, kp, pht->vsize);
        return added;
}


/* useful shortcuts, these can omit some work if you don't care about the
 * Value or are using it as a set. */

/* Add an entry with an unspecified value, if the entry already exists then its
 * value will not be changed, if it didn't exist then value is unspecified */
bool ht_add(HashTable *pht, Key k)
{
        Value *dummy;
        return ht_ins(pht, k, &dummy);
}

/* set a value, this is unlike ht_ins in that we are not told whether a new key
 * already existed and the data in Value may not match what was previously in
 * the table and is unspecified since it can wipe out the old record. */
Value *ht_set(HashTable *pht, Key k)
{
        Value *v;
        ht_ins(pht, k, &v);
        return v;
}


Value *ht_get(HashTable *ht, Key k)
{
        if (k < _RESERVED_ENTRIES)
                return ht->res[k];
        Key hk = hash_key(k);
        if (ht->ht) {
                Key *kp = ihash_get(ht->ht, hk);
                if (kp && *kp == hk)
                        return VPTR(ht->ht, kp, ht->vsize);
        }
        return NULL;
}

/* check if a key exists in the table */
bool ht_in(HashTable *ht, Key k)
{
        return (bool)ht_get(ht, k);
}

void ht_free(HashTable *ht)
{
        ifree(ht->ht);
        ht->ht = NULL;
        for (int i = 0; i < _RESERVED_ENTRIES; i++)
                free(ht->res[i]);
        memset(ht->res, 0, sizeof(Value *)*_RESERVED_ENTRIES);
}


/* clear all data while keeping the keys. the data is zero filled with the new
 * vsize which may be zero to get a set instead of a map. setting vsize equal to
 * the current vsize is fine and is a fast way to clear all the data in a map and
 * set it to zero. */

void
ht_new_vsize(HashTable *ht, int vsize)
{
        assert(vsize >= 0);
        for (int i = 0; i < _RESERVED_ENTRIES; i++) {
                if (ht->res[i]) {
                        free(ht->res[i]);
                        ht->res[i] = calloc(vsize + !vsize, sizeof(Value));
                }
        }
        ht->vsize = vsize;
        struct hash_table *h = ht->ht;
        if (h) {
                free(h->vs);
                h->vs = calloc(h->size + !vsize, !vsize + sizeof(Value) * vsize);
        }
}

Key
ht_next(HashTable *ht, uintptr_t *index, Value **v)
{
        struct hash_table *h = ht->ht;
        unsigned idx = *index;
        while (idx < _RESERVED_ENTRIES) {
                if (ht->res[idx]) {
                        *index = idx + 1;
                        *v = ht->res[idx];
                        return idx;
                }
                idx++;
        }
        if (h) {
                idx -= _RESERVED_ENTRIES;
                for (; idx < h->size; idx++) {
                        if (h->ks[idx]) {
                                *v =  VPTR(h, h->ks + idx, ht->vsize);
                                *index = idx + _RESERVED_ENTRIES + 1;
                                return ihash_key(h->ks[idx]);
                        }
                }
        }
        *index = 0;
        *v = NULL;
        return 0;
}


void
ht_dump(HashTable *ht)
{
        printf("Hashtable: size:%i count:%i\n", ht->ht->size, ht->ht->count);
        Value *v = NULL;
        uintptr_t index = 0;
        for (Key k = ht_next(ht, &index, &v); index; k = ht_next(ht, &index, &v))
                printf("%lx  %lx:%lx\n", (long)(index - 1), (long)k, (long) * v);
}

void
ht_stat(HashTable *ht, size_t *count, size_t *size, size_t *bytesize)
{
        *count = *size = *bytesize = 0;
        if (!ht->ht)
                return;
        *count = ht->ht->count;
        *size = ht->ht->size;
        *bytesize = sizeof(*ht) + *size * (sizeof(Key) + ht->vsize * sizeof(Value));
}


#ifdef TESTING
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#define THEMAP_INIT HASHMAP_INIT
typedef HashTable TheMap;

#include "map_test.c"
int main(int argc, char *argv[])
{
        unit_test();
        return 0;
//        HashTable ht = HASHTABLE_INIT;
        //unsigned a, b;
        /* for(int i = 0; i < 3000000; i++) { */
        /*         int count, size; */
        /* //               ht_stat(&ht, &count, &size); */
        /* //                printf("%i  %lu\n", count, size*2*sizeof(uintptr_t)); */
        /*         uintptr_t r1 = rnd(), r2 = rnd(); */
        /*         r2 = r1; */
        /*         r1 &= 0xffffffff; */
        /*         r2 &= 0xffffffff; */
        /*         r1 |= 0xDEAD00000000; */
        /*         r2 |= 0xBEEF00000000; */
        /*         Value *v = ht_ins(&ht, r1); */
        /*         *v = r2; */
        /* } */
        /* return 0; */
        /* while (!feof(stdin)) { */
        /*         if (scanf("s %u %u", &a, &b))  { */
        /*                 Value *v = ht_ins(&ht, a); */
        /*                 printf("%u -> %u\n", a, (unsigned)*v); */
        /*                 *v = b; */
        /*         } */
        /*         if (scanf("g %u", &a))  { */
        /*                 Value *v = ht_get(ht, a); */
        /*                 if(v) */
        /*                         printf("%u -> %u\n", a, (unsigned)*v); */
        /*                 else */
        /*                         printf("%u -> NONE\n", a); */
        /*         } */
        /*         getchar(); */
        /* } */
        /* return 0; */
}
#endif
