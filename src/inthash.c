/* decent invertable integer hash functions */

/* this originally used jenkins hashes but switched to ones found by the hash
 * prospector.
 *
 * All of these hashes have the property that hash(0) = 0. this is because zero is
 * often used as an sentinel so having it be a known hash is useful. if you
 * don't want this to happen, just add a random value before and after the hash.
 */
#include "inthash.h"

uint16_t hash_uint16(uint16_t x)
{
        x ^= x >> 8;
        x *= 0x88b5U;
        x ^= x >> 7;
        x *= 0xdb2dU;
        x ^= x >> 9;
        return x;
}
uint16_t ihash_uint16(uint16_t x)
{
        x ^= x >> 9 ;
        x *= 0x2ca5U;
        x ^= x >> 7 ^ x >> 14;
        x *= 0x259dU;
        x ^= x >> 8;
        return x;
}

uint32_t hash_uint32(uint32_t x)
{
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = (x >> 16) ^ x;
        return x;
}

uint32_t ihash_uint32(uint32_t x)
{
        x = ((x >> 16) ^ x) * 0x119de1f3;
        x = ((x >> 16) ^ x) * 0x119de1f3;
        x = (x >> 16) ^ x;
        return x;
}

uint64_t hash_uint64(uint64_t x)
{
        x = ((x >> 32) ^ x) * 0Xd6e8feb86659fd93;
        x = ((x >> 32) ^ x) * 0Xd6e8feb86659fd93;
        x = ((x >> 32) ^ x);
        return x;
}

uint64_t ihash_uint64(uint64_t x)
{
        x = ((x >> 32) ^ x) * 0Xcfee444d8b59a89b;
        x = ((x >> 32) ^ x) * 0Xcfee444d8b59a89b;
        x = ((x >> 32) ^ x);
        return x;
}


#ifdef TESTING

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "print_util.h"
int main(int argc, char *argv[])
{
        for (unsigned i = 0; i < 10; i++) {
                printf("hash32(%x)          = %lx\n", i, (long)hash_uint32(i));
                printf("ihash32(%x)         = %lx\n", i, (long)ihash_uint32(i));
                printf("hash32(ihash32(%x)) = %llx\n", i, (long long)hash_uint32(ihash_uint32(i)));
                printf("ihash32(hash32(%x)) = %llx\n", i, (long long)ihash_uint32(hash_uint32(i)));
                /* printf("HASH_MIX(hash32,1,%i)     = %llx\n", i, (long long)HASH_MIX(hash_uint32,1,i)); */
                /* printf("IHASH_MIX(hash32,1,%i)    = %llx\n", i, (long long)IHASH_MIX(ihash_uint32,1,i)); */
                /* printf("IHASH_MIX(HASH_MIX .. %i) = %llx\n", i, (long long)IHASH_MIX(ihash_uint32,HASH_MIX(hash_uint32,1,i),i)); */
                printf("\n");
        }
#if 1
        printf("----\n");
        for (unsigned i = 0; i < 10; i++) {
                printf("hash64(%x)          = %llx\n", i, (long long)hash_uint64(i));
                printf("ihash64(%x)         = %llx\n", i, (long long)ihash_uint64(i));
                printf("hash64(ihash64(%x)) = %llx\n", i, (long long)hash_uint64(ihash_uint64(i)));
                printf("ihash64(hash64(%x)) = %llx\n", i, (long long)ihash_uint64(hash_uint64(i)));
                printf("\n");
                //printf("ihash64(%x) = %llx\n", i, (long long)ihash_uint64(i));
        }
        for (unsigned i = 0; i < 10; i++) {
                printf("hash16(%x)          = %llx\n", i, (long long)hash_uint16(i));
                printf("ihash16(%x)         = %llx\n", i, (long long)ihash_uint16(i));
                printf("hash16(ihash16(%x)) = %llx\n", i, (long long)hash_uint16(ihash_uint16(i)));
                printf("ihash16(hash16(%x)) = %llx\n", i, (long long)ihash_uint16(hash_uint16(i)));
                printf("\n");
                //printf("ihash64(%x) = %llx\n", i, (long long)ihash_uint64(i));
        }
#endif
        const int COUNT = 1 << 27;
        uint32_t i32 = 0, u32 = 0;
        uint64_t i64 = 0, u64 = 0;
        uint16_t i16 = 0, u16 = 0;
        timeit(NULL);
        for (long long i =  0; i < COUNT; i++)
                u32 = hash_uint32(u32 ^ i);
        timeit("uint32");
        for (long long i =  0; i < COUNT; i++)
                i32 = ihash_uint32(i32 ^ i);
        timeit("iuint32");
        for (long long i =  0; i < COUNT; i++)
                u64 = hash_uint64(u64 ^ i);
        timeit("uint64");
        for (long long i =  0; i < COUNT; i++)
                i64 = ihash_uint64(i64 ^ i);
        timeit("iuint64");
        for (long long i =  0; i < COUNT; i++)
                u16 = hash_uint16(u16 ^ i);
        timeit("uint16");
        for (long long i =  0; i < COUNT; i++)
                i16 = ihash_uint16(i16 ^ i);
        timeit("iuint16");
        for (long long i =  0; i < COUNT; i++)
                u32 = HASH_MIX(hash_uint32, u32 ^ i, 1);
        timeit("mix1");
        for (long long i =  0; i < COUNT; i++)
                u32 = IHASH_MIX(ihash_uint32, u32 ^ i, 1);
        timeit("imix1");
        /* for(long long i =  0; i < COUNT; i++) */
        /*         i64 = splittable64(i64 ^ i); */
        /* timeit("splittable64"); */
        timeit(NULL);
        printf("u32: %lx\n", (long)u32);
        printf("i32: %lx\n", (long)i32);
        printf("u64: %llx\n", (long long)u64);
        printf("i64: %llx\n", (long long)i64);
        printf("u16: %llx\n", (long long)u16);
        printf("i16: %llx\n", (long long)i16);
        assert(HASH16_1 == hash_uint16(1));
        assert(HASH16_2 == hash_uint16(2));
        assert(HASH16_3 == hash_uint16(3));
        assert(HASH32_1 == hash_uint32(1));
        assert(HASH32_2 == hash_uint32(2));
        assert(HASH32_3 == hash_uint32(3));
        assert(HASH64_1 == hash_uint64(1));
        assert(HASH64_2 == hash_uint64(2));
        assert(HASH64_3 == hash_uint64(3));
        return 0;
}
#endif
