#ifndef INTHASH_H
#define INTHASH_H

#include <inttypes.h>

/* Useful integer hash routines and their inverses.
 *
 * they purposeuflly have f(0) == 0, because 0 is often used as a sentinel. if
 * you do not desire this, you can add a constant beforee hashing. */

uint16_t hash_uint16(uint16_t);
uint16_t ihash_uint16(uint16_t);
uint32_t hash_uint32(uint32_t);
uint32_t ihash_uint32(uint32_t);
uint64_t hash_uint64(uint64_t);
uint64_t ihash_uint64(uint64_t);

/* the switch statements will be constant and just expand to the correct
 * function, they are static to avoid extern inline declarations that
 * duplicate code and are not macros so type promotion and checking is handled
 * properly. */

#define _INTHASH_GENERATE(ty,name)                       \
static inline ty hash_##name(ty x)                       \
{                                                        \
        switch (sizeof(ty)) {                            \
        case sizeof(uint64_t): return hash_uint64(x);    \
        case sizeof(uint32_t): return hash_uint32(x);    \
        case sizeof(uint16_t): return hash_uint16(x);    \
        }                                                \
}                                                        \
                                                         \
static inline ty ihash_##name(ty x)                      \
{                                                        \
        switch (sizeof(ty)) {                            \
        case sizeof(uint64_t): return ihash_uint64(x);   \
        case sizeof(uint32_t): return ihash_uint32(x);   \
        case sizeof(uint16_t): return ihash_uint16(x);   \
        }                                                \
}

_INTHASH_GENERATE(uintptr_t, uintptr)
_INTHASH_GENERATE(unsigned long long, ulonglong)
_INTHASH_GENERATE(unsigned long, ulong)
_INTHASH_GENERATE(unsigned short, ushort)
_INTHASH_GENERATE(unsigned, unsigned)

/* A few known values for the hashs so they may be used by the preprocessor or
 * inlined as constants. */

#define HASH16_0  UINT16_C(0)
#define HASH16_1  UINT16_C(0x7dea)
#define HASH16_2  UINT16_C(0xa1f8)
#define HASH16_3  UINT16_C(0xf88)

#define HASH32_0  UINT32_C(0)
#define HASH32_1  UINT32_C(0x31251ba7)
#define HASH32_2  UINT32_C(0x66a79298)
#define HASH32_3  UINT32_C(0xdfb6d245)

#define HASH64_0  UINT64_C(0)
#define HASH64_1  UINT64_C(0x4179b061e0c0e0d0)
#define HASH64_2  UINT64_C(0x1c9963305febc252)
#define HASH64_3  UINT64_C(0x70d9f876237016c6)


/* create a hash family from a hash,
 * this has not been theoretically verified but seems to work okay in
 * practice. If you know of a better way, or can validate this is a good way to
 * do it, then let me know.
 *
 * Simplistic things like just xoring m before or after hashing make the
 * values look different but will lead to the same data having the same
 * collisions just in a different spot in either key or hash space.
 *
 * My specious reasoning behind thinking this will work is that a valid way to
 * combine HMAC of two message A and B is HMAC (HMAC(A) || HMAC(B)) and this is
 * a similar construction.
 */

#define HASH_MIX(hash_fn, x, m)  (hash_fn(hash_fn(x + m) + ~m) + m)
#define IHASH_MIX(hash_fn, x, m) (hash_fn(hash_fn(x - m) - ~m) - m)

#endif /* end of include guard: INTHASH_H */
