#include <stdatomic.h>
#include <stdbool.h>
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
// &addr + addr
#define getaddr(addr) _getaddr(&addr, addr)

static inline p_rel _setaddr(p_rel *i, char *p)
{
    return (*i = (p - (char *) i));
}
/* store the absolute pointer as relative address in relative_p */
// pointer - &relative_p
#define setaddr(relative_p, pointer) _setaddr(&relative_p, pointer)




vm_t *vm_new()
{
    vm_t *node = mmap(0, PAGESIZE, PROT_READ | PROT_WRITE,
                      MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (node == MAP_FAILED)
        err(ENOMEM, "Failed to map memory");

    node->max = N_VM_ELEMENTS;
    node->next = NULL; /* for clarity */

    setaddr(node->array[0], node->str);

    /* prevent compilers from optimizing assignments out */
    OPTFENCE(node);
    return node;
}


void vm_destroy(vm_t *nod, void *(callback)(const char *e))
{
    do {
        char *tmp = (char *) nod;
        nod = nod->next;
        munmap(tmp, PAGESIZE);
    } while (nod);
}

// static vm_t *vm_extend_map(vm_t *nod)
// {
//     vm_t *new_nod = mmap(0, PAGESIZE, PROT_READ | PROT_WRITE,
//                      MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
//     if (new_nod == MAP_FAILED)
//         err(ENOMEM, "Failed to map memory");
//     vm_t *nullptr = NULL;

//     new_nod->max = N_VM_ELEMENTS;
//     // new_nod->next = NULL;
//     new_nod->next = nod;
//     new_nod->use = 0;
//     setaddr(new_nod->array[0], new_nod->str);

//     while(!atomic_compare_exchange_strong_explicit(&nod->next,&nullptr,new_nod,memory_order_seq_cst, memory_order_seq_cst)) {
//         nullptr = NULL;
//         nod = nod->next;
//     }
    
//     return new_nod;
// }

static vm_t *vm_extend_map(struct vm_head *head)
{
    vm_t *nod = head->h.next;
    vm_t *new_nod = mmap(0, PAGESIZE, PROT_READ | PROT_WRITE,
                     MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (new_nod == MAP_FAILED)
        err(ENOMEM, "Failed to map memory");

    new_nod->max = N_VM_ELEMENTS;
    new_nod->next = nod;
    // new_nod->use = 0;
    setaddr(new_nod->array[0], new_nod->str);
    head->h.next = new_nod;
    
    return new_nod;
}

uintptr_t vm_add(size_t sz, struct vm_head *head)
{

    vm_t *nod = head->h.next;
    retry:
    if ((int) ((nod->array[nod->use] + (sizeof(p_rel) * nod->use) +
                sz)) >= PAGESIZE) {
        nod->max = nod->use + 1; /* addr > map */
        nod = vm_extend_map(head);
        goto retry;
    }

    char *p = getaddr(nod->array[nod->use]) + sz;
    setaddr(nod->array[nod->use + 1], p);
    nod->use++;
    return getaddr(nod->array[nod->use - 1]);
}

static void *dealloc(const char *e)
{
    /* FIXME: implement real deallocation */
    return NULL;
}
