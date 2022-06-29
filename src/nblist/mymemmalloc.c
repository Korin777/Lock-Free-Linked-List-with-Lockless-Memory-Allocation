#include "mymemmalloc.h"

/* the macro OPTFENCE(...) can be invoked with any parameter.
 * The parameters will get calculated, even if gcc doesn't recognize
 * the use of the parameters, e.g. cause they are needed for an inlined asm
 * syscall.
 *
 * The macro translates to an asm jmp and a function call to the function
 * opt_fence, which is defined with the attribute "noipa" -
 * (the compiler "forgets" the function body, so gcc is forced to generate
 * all arguments for the function.)
 */
void __attribute__((noipa, cold, naked)) opt_fence(void *p, ...) {}
#define _optjmp(a, b) __asm__(a "OPTFENCE_" #b)
#define _optlabel(a) __asm__("OPTFENCE_" #a ":")
#define __optfence(a, ...)  \
    _optjmp("jmp ", a);     \
    opt_fence(__VA_ARGS__); \
    _optlabel(a)
#define OPTFENCE(...) __optfence(__COUNTER__, __VA_ARGS__)


static inline char *_getaddr(p_rel *i, p_rel addr)
{
    return ((char *) i + addr);
}
/* translate a relative pointer to an absolute address */
#define getaddr(addr) _getaddr(&addr, addr) // &addr + addr

static inline p_rel _setaddr(p_rel *i, char *p)
{
    return (*i = (p - (char *) i));
}
/* store the absolute pointer as relative address in relative_p */
#define setaddr(relative_p, pointer) _setaddr(&relative_p, pointer) // pointer - &relative_p


thread_local int tid_v = TID_UNKNOWN;
_Atomic int_fast32_t mmap_count = 0;


// size align up to 8 byte
int align_up(int sz) {
    return (sz + 7) & ~7;
}

void push(my_stack_t *s, reuse_block_t *rb) {
    reuse_block_t *old = s->next;
    do {
        rb->next = old;
    } while(!atomic_compare_exchange_strong_explicit(&s->next,
            &old,rb,memory_order_release,memory_order_relaxed));
}


uintptr_t pop(my_stack_t *s) {
    reuse_block_t *old = s->next;
    uintptr_t block;
    do {
        if(!old)
            return 0;
    } while(!atomic_compare_exchange_strong_explicit(&s->next,
            &old, old->next,memory_order_release,memory_order_relaxed));
    block = (uintptr_t)old + 8;
    return block;
}

vm_t *vm_new()
{
    atomic_fetch_add(&mmap_count, 1);
    vm_t *node = mmap(0, PAGESIZE, PROT_READ | PROT_WRITE,
                      MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (node == MAP_FAILED)
        err(ENOMEM, "Failed to map memory");

    node->max = N_VM_ELEMENTS;
    node->next = NULL; /* for clarity */

    setaddr(node->array[0], node->raw);

    /* prevent compilers from optimizing assignments out */
    OPTFENCE(node);
    return node;
}


void vm_destroy(vm_head_t *head)
{
    vm_t *nod = head->next;
    while (nod) {
        char *tmp = (char *) nod;
        nod = nod->next;
        atomic_fetch_sub(&mmap_count,1);
        munmap(tmp, PAGESIZE);
    }
}

vm_t *vm_extend_map(vm_head_t *head)
{
    vm_t *nod = head->next;
    vm_t *new_nod = vm_new();
    new_nod->next = nod;
    head->next = new_nod;
    
    return new_nod;
}

// malloc
uintptr_t vm_add(size_t sz)
{
    if(sz == 0)
        return 0;
    sz = align_up(sz + 16);

    uintptr_t block = pop(&vm[tid_v].freed[(sz >> 3) - 1]);
    if(block) {
        return block;
    }

    vm_t *nod = vm[tid_v].next;
    if(!nod)
        nod = vm_extend_map(&vm[tid_v]);

    retry:
    if ((int) ((nod->array[nod->use] + (sizeof(p_rel) * nod->use) +
                sz)) >= PAGESIZE || nod->use >= nod->max) {
        nod->max = nod->use + 1; /* addr > map */
        nod = vm_extend_map(&vm[tid_v]);
        goto retry;
    }

    char *tmp = getaddr(nod->array[nod->use]);
    char *p = tmp + sz;
    nod->use++;
    setaddr(nod->array[nod->use], p);
    *((uintptr_t *)tmp) = (sz << 32) | vm[tid_v].id;
    return (uintptr_t)tmp + 16;
}

// free
void vm_remove(uintptr_t ptr) {
    if(!ptr)
        return;
    uintptr_t header = *((uintptr_t *)(ptr - 16));
    uintptr_t sz = header >> 32, id = header & 0xffff;
    if(sz == 0)
        return;
    push(&vm[id].freed[(sz >> 3) - 1],(reuse_block_t *)(ptr - 8));
}