#ifndef MY_MELLOC
#define MY_MELLOC

#include <asm-generic/param.h>
#define PAGESIZE EXEC_PAGESIZE // In asm-generic/param.h => EXEC_PAGESIZE 4096
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdint.h>


/* how many directories per (preallocated) page
 * -> (PAGESIZE/16) = 256 for 4KiB pages.
 * each path can be in the medium (PAGESIZE - 4 * 256 - 20) / 256 Bytes.
 * The notify dirlist ist dynamically grown.
 */
#define N_VM_ELEMENTS (PAGESIZE / 16) // N_VM_ELEMENTS 256
// #define N_VM_ELEMENTS 244 // N_VM_ELEMENTS 256

/* Avoids with uint32 the penalty of unaligned memory access */
typedef unsigned int p_rel;



typedef struct __vm {
    p_rel array[N_VM_ELEMENTS]; // 4*256
    struct __vm *next; // 8
    _Atomic int max, use; // 4 + 4
    /* dynamic string section starts here */
    char str[0]; // 3056
} vm_t; // 4096

struct head {
    vm_t *next;
};

struct vm_head {
    struct head h;
};


// void vm_new(struct MA ma);
vm_t *vm_new();


void vm_destroy(vm_t *nod, void *(callback)(const char *e));

static vm_t *vm_extend_map(struct vm_head *head);

// uintptr_t vm_add(size_t sz, vm_t *nod);
uintptr_t vm_add(size_t sz, struct vm_head *head);

static void *dealloc(const char *e);


#endif