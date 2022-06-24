#include <threads.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#define HP_MAX_THREADS 10

#define HP_MAX_HPS 5

#define max_protect 3
#define HP_MAX_RETIRED (HP_MAX_THREADS * HP_MAX_HPS)
#define HP_THRESHOLD_R 0



typedef struct retirelist {
    int size;
    uintptr_t *list;
} retirelist_t;

typedef void(list_hp_deletefunc_t)(void *);

typedef struct list_hp {
    int max_hps;
    _Atomic uintptr_t *hp[HP_MAX_THREADS]; // protect list
    retirelist_t *rl[HP_MAX_THREADS]; // retire list
    list_hp_deletefunc_t *deletefunc;
} list_hp_t;


/* Create a new hazard pointer array of size 'max_hps' (or a reasonable
 * default value if 'max_hps' is 0). The function 'deletefunc' will be
 * used to delete objects protected by hazard pointers when it becomes
 * safe to retire them.
 */
list_hp_t *list_hp_new(size_t max_hps, list_hp_deletefunc_t *deletefunc);


/* Destroy a hazard pointer array and clean up all objects protected
 * by hazard pointers.
 */
void list_hp_destroy(list_hp_t *hp);


/* Clear all hazard pointers in the array for the current thread.
 * Progress condition: wait-free bounded (by max_hps)
 */
void list_hp_clear(list_hp_t *hp, int tid);


/* This returns the same value that is passed as ptr.
 * Progress condition: wait-free population oblivious.
 */
uintptr_t list_hp_protect_ptr(list_hp_t *hp, int ihp, uintptr_t ptr,int tid);

/* Same as list_hp_protect_ptr(), but explicitly uses memory_order_release.
 * Progress condition: wait-free population oblivious.
 */
uintptr_t list_hp_protect_release(list_hp_t *hp, int ihp, uintptr_t ptr, int tid);

/* Retire an object that is no longer in use by any thread, calling
 * the delete function that was specified in list_hp_new().
 *
 * Progress condition: wait-free bounded (by the number of threads squared)
 */
void list_hp_retire(list_hp_t *hp, uintptr_t ptr, int tid);

static void free_func(void *arg);


list_hp_t *hp;
