#include "hp.h"

list_hp_t *list_hp_new(size_t max_hps, list_hp_deletefunc_t *deletefunc)
{
    list_hp_t *hp = malloc(sizeof(*hp));
    assert(hp);

    if (max_hps == 0)
        max_hps = HP_MAX_HPS;

    *hp = (list_hp_t){.max_hps = max_hps, .deletefunc = deletefunc};

    for (int i = 0; i < HP_MAX_THREADS; i++) {
        hp->hp[i] = calloc(HP_MAX_HPS,sizeof(hp->hp[i][0]));
        hp->rl[i] = calloc(1, sizeof(*hp->rl[0]));
        // hp->hp[i] = calloc(CLPAD * 2, sizeof(hp->hp[i][0]));
        // hp->rl[i * CLPAD] = calloc(1, sizeof(*hp->rl[0]));
        for (int j = 0; j < hp->max_hps; j++)
            atomic_init(&hp->hp[i][j], 0);
        hp->rl[i]->list = calloc(HP_MAX_RETIRED, sizeof(uintptr_t));
    }

    return hp;
}

void list_hp_destroy(list_hp_t *hp)
{
    for (int i = 0; i < HP_MAX_THREADS; i++) {
        free(hp->hp[i]);
        retirelist_t *rl = hp->rl[i];
        for (int j = 0; j < rl->size; j++) {
            void *data = (void *) rl->list[j];
            hp->deletefunc(data);
        }
        free(rl->list);
        free(rl);
    }
    free(hp);
}

void list_hp_clear(list_hp_t *hp, int tid)
{
    for (int i = 0; i < hp->max_hps; i++)
        atomic_store_explicit(&hp->hp[tid][i], 0, memory_order_release);
}

uintptr_t list_hp_protect_ptr(list_hp_t *hp, int ihp, uintptr_t ptr,int tid)
{
    atomic_store(&hp->hp[tid][ihp], ptr);
    return ptr;
}

uintptr_t list_hp_protect_release(list_hp_t *hp, int ihp, uintptr_t ptr, int tid)
{
    atomic_store_explicit(&hp->hp[tid][ihp], ptr, memory_order_release);
    return ptr;
}

void list_hp_retire(list_hp_t *hp, uintptr_t ptr, int tid)
{
    retirelist_t *rl = hp->rl[tid];
    rl->list[rl->size++] = ptr;
    assert(rl->size < HP_MAX_RETIRED);

    if (rl->size < HP_THRESHOLD_R)
        return;

    for (size_t iret = 0; iret < rl->size;) {
        uintptr_t obj = rl->list[iret];
        bool can_delete = true;
        for (int itid = 0; itid < HP_MAX_THREADS && can_delete; itid++) {
            for (int ihp = hp->max_hps - 1; ihp >= 0; ihp--) {
                if (atomic_load(&hp->hp[itid][ihp]) == obj) {
                    can_delete = false;
                    break;
                }
            }
        }

        if (can_delete) {
            size_t bytes = (rl->size - iret) * sizeof(rl->list[0]);
            memmove(&rl->list[iret], &rl->list[iret + 1], bytes);
            rl->size--;
            hp->deletefunc((void *) obj);
        } else {
            iret++;
        }
    }
}
