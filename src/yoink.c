#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <stddef.h>
#include <assert.h>
#include <stdatomic.h>
#include "yoink.h"
#include "inthash.h"
#include "ptrhashtable2.h"
#include "resizable_buf.h"

// the ptrs is needed so the size gets aligned to a pointer boundry.
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


#define container_of(ptr, type, member) \
       (type *)( (char *)(ptr) - offsetof(type, member) )

/* we depend on these being the same size */
_Static_assert(sizeof(void*) == sizeof(uintptr_t));

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

void *arena_alloc(Arena *arena, int tsz, int bptrs, int eptrs)
{
        assert((unsigned)bptrs <= UINT8_MAX);
        assert((unsigned)(eptrs - bptrs) <= UINT16_MAX);
        tsz = _ARENA_RUP(tsz) * sizeof(void *); // round up
//        printf("arena_alloc(_,%i,%i,%i)\n", tsz, bptrs, eptrs);
        size_t needed = sizeof(struct chain) + tsz;
        struct chain *chain = calloc(1, needed);
        if(!chain) {
                fprintf(stderr, "arena_alloc error: %s", strerror(errno));
                abort();
        }
        chain->head.tsz  = tsz;
        chain->head.nptrs = eptrs - bptrs;
        chain->head.bptrs = bptrs;
        struct chain *orig = atomic_load(&arena->chain);
        do {
                chain->next = orig;
        } while (!atomic_compare_exchange_weak(&arena->chain, &orig, chain));
        return chain->data;
}

static struct chain *
chain_dup(Arena *to, struct chain *chain)
{
        size_t needed = sizeof(struct chain) + chain->head.tsz;
        struct chain *new = malloc(needed);
        memcpy(new, chain, needed);

        struct chain *orig = atomic_load(&to->chain);
        do {
                new->next = orig;
        } while (!atomic_compare_exchange_weak(&to->chain, &orig, new));
        return new;
}

char *
arena_strdup(Arena *bowl, char *s) {
        size_t len = strlen(s) + 1;
        char *ret = arena_alloc(bowl, len, 0, 0);
        memcpy(ret, s, len);
        return ret;
}
char *
arena_vprintf(Arena *bowl, char *fmt, va_list ap) {
        va_list cap;
        va_copy(cap, ap);
        int size = vsnprintf(NULL, 0, fmt, cap);
        va_end(cap);
        if (size < 0)
                return NULL;
        char *ret = arena_alloc(bowl, size + 1, 0, 0);
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
        return memcpy(arena_alloc(bowl, len, 0, 0), data, len);
}

void
arena_initialize_buffer(Arena *bowl, rb_t *buf) {
        /* clear buffer */
        rb_free(buf);
        rb_calloc(buf, sizeof(struct chain));
}

void *
arena_finalize_buffer(Arena *bowl, rb_t *buf, bool pointer_array) {
        /* make sure we have some breathing room to keep alignments correct. */
        int tsz = _ARENA_RUP(rb_len(buf));
        rb_calloc(buf, tsz*sizeof(void*) - rb_len(buf));
        struct chain *chain = rb_ptr(buf);
        chain->head.tsz = rb_len(buf);
        if(pointer_array)
                chain->head.nptrs = tsz;

        struct chain *orig = atomic_load(&bowl->chain);
        do {
                chain->next = orig;
        } while (!atomic_compare_exchange_weak(&bowl->chain, &orig, chain));
        return rb_take(buf);
}

/* trace will contain integers with the offsets to all the pointers in rb, hash
 * table will be filled with a map of pointers to offsets, if keep_meta is true
 * the header will be copied as well. */
static void
_arena_yoink_to_rb(rb_t *target, bool keep_meta, HashTable *ht, rb_t *trace, void *root)
{
        /* stack is where live objects are kept track of. */
        rb_t stack = RB_BLANK;
        for (void *np = root; np; np = RB_MPOP(void *, &stack, NULL)) {
                uintptr_t *pp = NULL;
                if (ht_ins(ht, (uintptr_t)np, &pp)) {
                        int loc = rb_len(target) + (keep_meta ? sizeof(struct header) : 0);
                        struct chain *chain = container_of(np, struct chain, data);
                        for (int i = 0; i < chain->head.nptrs; i++) {
                                if (!chain->data[i] || (uintptr_t)chain->data[i] & 1)
                                        continue;
                                RB_PUSH(void *, &stack) = chain->data[i];
                                RB_PUSH(int, trace) = loc + sizeof(void *)*i;
                        }
                        *pp = loc;
                        if(keep_meta) {
                                rb_append(target, &chain->head, sizeof(chain->head) + chain->head.tsz);
                        } else {
                                rb_append(target, chain->data, chain->head.tsz);
                        }
                }
        }
        rb_free(&stack);
}

void *
arena_yoink_to_malloc(void *root, size_t *len)
{
        if (len)
                *len = 0;
        if (!root || ((uintptr_t)root & 1))
                return NULL;
        rb_t trace = RB_BLANK;
        rb_t output = RB_BLANK;
        HashTable ht = HASHMAP_INIT;
        _arena_yoink_to_rb(&output, false, &ht, &trace, root);
        void *ptr = rb_ptr(&output);
        RB_FOR(int, tp, &trace) {
                int loc = *tp;
                void **data = ptr + loc;
                void *odata = *data;
                Value *v = ht_get(&ht, (uintptr_t) * data);
                *data = ptr + *v;
                printf("radjusting@%i via %lu %p -> %p\n", loc, *v, odata, *data);
                assert(*data >= rb_ptr(&output));
                assert(*data < rb_endptr(&output));
        }
        //ht_dump(&ht);
        printf("trace: %li\n", (long)RB_NITEMS(int, &trace));
        rb_free(&trace);
        ht_free(&ht);
        if (len)
                *len = rb_len(&output);
        return rb_take(&output);
}


//too much of the void to implement this. maybe if it is needed later.
//void *arena_yoink_custom(void *root, void *(*ccopy)(void *, void *), void *);

ssize_t
arena_yoinks(Arena *to, bool always_copy, int nroots, void *root[nroots])
{
        ssize_t tlen = 0;
        rb_t stack = RB_BLANK;
        HashTable ht = HASHMAP_INIT;
        /* if we don't want to copy we have to seed the table with pointers
         * already in target */
        if(!always_copy)
                for (struct chain *c = to->chain; c; c = c->next)
                        *ht_set(&ht, (intptr_t)c->data)  = (intptr_t)c->data;
        for (int i = 0; i < nroots; i++)
                RB_PUSH(void **, &stack) = root + i;

        RB_FOR_ENUM(void **, pnp, &stack) {
                void **np = *pnp.v;
                if(!*np || ((uintptr_t)*np & 1))
                        continue;
                uintptr_t *pp = NULL;
                if (ht_ins(&ht, (uintptr_t)*np, &pp)) {
                        struct chain *chain = container_of(*np, struct chain, data);
                        chain = chain_dup(to, chain);
                        tlen += chain->head.tsz;
                        for (int i = 0; i < chain->head.nptrs; i++)
                                RB_PUSH(void **, &stack) = &chain->data[i];
                        *pp = (uintptr_t)chain->data;
                        assert(*pp);
                }
                *np = (void *)*pp;
        }
        rb_free(&stack);
        ht_free(&ht);
        return tlen;
}

ssize_t
arena_vacuums(Arena *bowl, int nroots, void *root[nroots])
{
        rb_t stack = RB_BLANK;
        HashTable ht = HASHSET_INIT;
        /* if we don't want to copy we have to seed the table with pointers
         * already in target */
        for (int i = 0; i < nroots; i++)
                RB_PUSH(void *, &stack) = root[i];
        RB_FOR_ENUM(void *, pnp, &stack) {
                void *np = *pnp.v;
                if(!np || ((uintptr_t)np & 1))
                        continue;
                if (ht_add(&ht, (uintptr_t)np)) {
                        struct chain *chain = container_of(np, struct chain, data);
                        for (int i = 0; i < chain->head.nptrs; i++)
                                RB_PUSH(void *, &stack) = chain->data[i];
                }
        }
        rb_free(&stack);
        struct chain *chain = bowl->chain;
        struct chain **pch = &chain;
        ssize_t freed  = 0;
        while(*pch) {
                struct chain *next = pch[0]->next;
                if(!ht_in(&ht, (uintptr_t)pch[0]->data)) {
                        freed += pch[0]->head.tsz;
//                        printf("dfree: %p\n", pch[0]->data);
 //                       printf("free: %p\n", pch[0]);
                        free(pch[0]);
                        *pch = next;
                } else {
  //                      printf("dkeep: %p\n", pch[0]->data);
   //                     printf("keep: %p\n", pch[0]);
                        pch = &pch[0]->next;
                }
        }
        bowl->chain = chain;
    //    ht_dump(&ht);
        ht_free(&ht);
        return freed;
}


void *arena_yoink(Arena *to, void *root)
{
        arena_yoinks(to, true, 1, &root);
        return root;
}

/* very basic signature, this isn't cryptographically secure or anything, it is
 * just to catch gross errors early */
static
uintptr_t mk_signature(void) {
        uintptr_t sig = 0x1;
        uintptr_t byteorder = 0x10203040;
        sig = hash_uintptr(sig ^ sizeof(short));
        sig = hash_uintptr(sig ^ sizeof(int));
        sig = hash_uintptr(sig ^ sizeof(long));
        sig = hash_uintptr(sig ^ sizeof(long long));
        sig = hash_uintptr(sig ^ sizeof(uintptr_t));
        for(int i = 0; i < sizeof(uintptr_t);i++)
                sig = hash_uintptr(sig ^ ((char *)&byteorder)[i]);
        return sig;
}
static uintptr_t signature = 0;

void arena_freeze(rb_t *to, void *root, int key) {
        if(!signature)
                signature = mk_signature();
        size_t blen = rb_len(to);
        RB_PUSH(uintptr_t, to) = signature ^ key;
        HashTable ht = HASHMAP_INIT;
        rb_t trace = RB_BLANK;
        uintptr_t *plen = RB_PUSHN(uintptr_t, to, 1);
        uintptr_t *troot = RB_PUSHN(uintptr_t, to, 1);
        void *ptr = rb_ptr(to);
        _arena_yoink_to_rb(to, true, &ht, &trace, root);
        RB_FOR(int, tp, &trace) {
                int loc = *tp;
                void **data = ptr + loc;
                void *odata = *data;
                Value *v = ht_get(&ht, (uintptr_t)*data);
                *data = (void*)((*v << 2) | 2);
                printf("fradjusting@%i via %lu %p -> %p\n", loc, *v, odata, *data);
        }
        if (!root || (uintptr_t)root & 1)
                *troot = (uintptr_t)root;
        else {
                *troot = (*ht_get(&ht, (uintptr_t)root) << 2) | 2;
        }
        *plen = rb_len(to) - blen;
        ht_free(&ht);
        rb_free(&trace);
}
void *
arena_thaw(int key, size_t len, void **ptr) {
        if(!signature)
                signature = mk_signature();
        if(!ptr || !*ptr || len < sizeof(uintptr_t)*3)
                return NULL;
        uintptr_t *ps = *ptr;
        if((ps[0] ^ key) != signature)
                return NULL;
        if(ps[1] > len)
                return NULL;
        rb_t trace = RB_BLANK;
}

struct node {
        BEGIN_PTRS;
        struct node *left;
        struct node *right;
        END_PTRS;
        int v;
        char *name;
};


void
dump_tree(struct node *n, int idt)
{
        if (!n) {
                return;
        }
        if (idt > 15) {
                for (int i = 0; i < idt; i++)
                        putchar(' ');
                printf("...\n");
                return;
        }
        if (!n->left && !n->right) {
                for (int i = 0; i < idt; i++)
                        putchar(' ');
                printf("%i\n", n->v);
                return;
        }
        //        printf("(\n");
        dump_tree(n->left, idt + 1);
        for (int i = 0; i < idt; i++)
                putchar(' ');
        printf("%i\n", n->v);
        dump_tree(n->right, idt + 1);
        //      for (int i = 0; i < idt; i++)
        //             putchar (' ');
        //       printf(")\n");
}

struct node *insert(Arena *arena, struct node *root, struct node *n)
{
       // printf("insert(%p %p)\n", root, n);
//       printf("insert(%p,%p)\n",root,n);
        if (!root)
                return n;
        if (n->v == root->v)
                return root;
        struct node *nroot = ARENA_CALLOC(arena, *nroot);
        *nroot = *root;
//        printf("insert(%p %p)\n", root, n);
        if (n->v < root->v)
                root->left =  insert(arena, nroot->left, n);
        if (n->v > root->v)
                root->right = insert(arena, nroot->right, n);
//        printf("%i\n", root->v);
//       printf("rensertinsert(%p %p)\n", root, n);
        return root;
}

struct node *
insert_tree(Arena *arena, struct node *root, int v)
{
//        printf("insert_tree(%p %i)\n", root, v);
        fflush(stdout);
//        printf("b:%p\n", root);
        struct node *new = ARENA_CALLOC(arena, *new);
//       printf("a:%p\n", root);
        new->v = v;
        new->left = NULL;
        new->right = NULL;
        return insert(arena, root, new);
}

void arena_stats(Arena *a, long *nbytes, long *nptrs)
{
        long _nbytes = 0, _nptrs = 0;
        struct chain *c = a->chain;
        while (c) {
                _nbytes += c->head.tsz;
                _nptrs += c->head.nptrs;
                c = c->next;
        }
        *nbytes = _nbytes;
        *nptrs = _nptrs;
}

void
compare_tree(struct node *a, struct node *b)
{
        if (!a && !b)
                return;
        if (a == b) {
                printf("shared: %p\n", a);
                return;
        }
        if (a->v != b->v) {
                printf("mismatch: %i != %i\n", a->v, b->v);
                return;
        }
        compare_tree(a->left, b->left);
        compare_tree(a->right, b->right);
}


long arena_nbytes(Arena *a)
{
        long nbytes, dummy;
        arena_stats(a, &nbytes, &dummy);
        return nbytes;
}

#include <stdlib.h>
int main(int argc, char *argv[])
{
        Arena arena = ARENA_INIT;
        struct node *root = NULL;
        for (int i = 0; i < 100; i++)
                root = insert_tree(&arena, root, rand() % 10);
        dump_tree(root, 0);
        printf("before: %lu\n", arena_nbytes(&arena));
        Arena arena2 = ARENA_INIT;
        struct node *root2 =  arena_yoink(&arena2, root);
        printf("after: %lu\n", arena_nbytes(&arena2));
        dump_tree(root2, 0);
        arena_free(&arena);
        dump_tree(root2, 0);
        // make cyclic
        root2->left->right = root2;
        dump_tree(root2, 0);
        Arena arena3 = ARENA_INIT;
        struct node *root3 = arena_yoink(&arena3, root2);
        printf("after3: %lu\n", arena_nbytes(&arena3));
        dump_tree(root3, 0);
        size_t len;
        void *root4 = arena_yoink_to_malloc(root3, &len);
        printf("after4: %lu\n", len);
        dump_tree(root4, 0);
        printf("big one \n");
        root = NULL;
#if 1
        for (int i = 0; i < 10000; i++)
                root = insert_tree(&arena, root, rand() % 1000);
        dump_tree(root, 0);
        printf("before: %lu\n", arena_nbytes(&arena));
        void *rooty = arena_yoink_to_malloc(root, &len);
        printf("after5: %lu\n", len);
        dump_tree(rooty, 0);
        compare_tree(rooty, root);
        free(rooty);
        free(root4);
        arena_free(&arena3);
#endif
        arena_free(&arena);
        arena_free(&arena2);
        root = NULL;
        for (int i = 0; i < 100; i++)
                root = insert_tree(&arena, root, rand() % 1000);
        printf("nbytes_before: %lu\n", arena_nbytes(&arena));
        void *roots[] = {root};
        arena_yoink(&arena2, root);
        arena_vacuums(&arena, 1, roots);
        printf("nbytes_afterV: %lu\n", arena_nbytes(&arena));
        printf("nbytes_afterY: %lu\n", arena_nbytes(&arena2));
        arena_free(&arena);
        arena_free(&arena2);
        return 0;
}


