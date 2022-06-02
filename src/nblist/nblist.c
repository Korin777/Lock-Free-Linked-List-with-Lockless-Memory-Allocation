#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include<stdio.h>

#include "nblist.h"


enum {
    F_MARK = 1, /* dead */
    F_FLAG = 2, /* next is about to die */
    F__MASK = 3,
};

static inline struct nblist_node *n_ptr(uintptr_t x)
{
    return (struct nblist_node *) (x & ~F__MASK);
}

/* set flag on *@pp, passing through backlinks as necessary.
 * @p_val is set to a value of (*@pp)->next. *@pp is updated to the node that
 * points to @n, which may be the list head, or NULL if @n is not on the list
 * anymore. returns true if *@pp points to @n and this flagged it, false
 * otherwise.
 */
// pp: del_node's predecessor
// p_val: del_node's predecessor->next
// n: del_node   
static bool try_flag(struct nblist_node **pp,
                     uintptr_t p_val,
                     struct nblist_node *n)
{
    struct nblist_node *p = *pp;
    const uintptr_t new_val = (uintptr_t) n | F_FLAG;

    bool got;
    for (;;) {
        // already flagged
        if (p_val == new_val) {
            *pp = p;
            return false;
        }

        uintptr_t old_val = (uintptr_t) n;
        got = atomic_compare_exchange_strong(&p->next, &old_val, new_val);
        if (got || old_val == new_val) {
            /* success, or concurrent flagging. */
            *pp = p;
            return got;
        }

        p_val = old_val;

        /* failure due to concurrent marking. follow backlinks. */
        // someone mark del_node's predecessor 
        while ((p_val & F_MARK) != 0) {
            p = atomic_load_explicit(&p->backlink, memory_order_relaxed);
            assert(p);
            p_val = atomic_load_explicit(&p->next, memory_order_relaxed);
        }

        /* @p is no longer @n's parent. walk forward until the parent is
         * found, or return NULL.
         */
        // assert(n_ptr(p_val));
        if (!n_ptr(p_val)) {
            *pp = NULL;
            return false;
        }
        while (n_ptr(p_val) != n) {
            p = n_ptr(p_val);
            p_val = atomic_load_explicit(&p->next, memory_order_relaxed);
            if (!n_ptr(p_val)) {
                *pp = NULL;
                return false;
            }
        }
    }

    *pp = p;
    return got;
}

/* complete removal of @prev -> @n, where @nextval == @n->next. */
// prev: del_node's predecessor
// n: del_node
// nextval: del_node's sucessor with mark
static inline void rend_the_marked(struct nblist_node *prev,
                                   struct nblist_node *n,
                                   uintptr_t nextval)
{
    assert((nextval & F_MARK) != 0);
    assert((nextval & F_FLAG) == 0);
    uintptr_t prevval = (uintptr_t) n | F_FLAG;

    atomic_compare_exchange_strong_explicit(
        &prev->next, &prevval, nextval & ~F__MASK, memory_order_release,
        memory_order_relaxed);
}

/* complete removal of @n from flagged parent @prev. */
// prev: del_node's predecessor
// n: del_node
static void clear_flag(struct nblist_node *prev, struct nblist_node *n)
{
    // Setting n's backlink to point to its predecessor
    struct nblist_node *old =
        atomic_exchange_explicit(&n->backlink, prev, memory_order_release);
    assert(!old || old == prev); // n->backlink not set or n->backlink same

    /* set mark, load fresh @n->next. */
    uintptr_t nextval = atomic_load_explicit(&n->next, memory_order_relaxed);
    while ((nextval & F_MARK) == 0) { // mark n
        while ((nextval & F_FLAG) != 0) { // n->next 要被移除 , 要先移出它才能 mark n
            clear_flag(n, n_ptr(nextval));
            nextval = atomic_load(&n->next);
        }
        if (atomic_compare_exchange_strong_explicit(
                &n->next, &nextval, nextval | F_MARK, memory_order_release,
                memory_order_relaxed)) {
            nextval |= F_MARK;
        }
    }

    rend_the_marked(prev, n, nextval);
}


/*     Search from curr_node and find two consecutive nodes
 *   n1 and n2 such that n1.value ≤ k < n2.value
 */
struct nblist_node *list_search(val_t val, struct nblist_node *curr_node, struct nblist_node **left_node)
{
    uintptr_t curr_p = atomic_load(&curr_node->next);
    struct nblist_node *next_node = n_ptr(curr_p);

    while(next_node && (container_of(next_node,struct item, link)->value <= val)) {
        uintptr_t next_p = atomic_load(&next_node->next);
        while((next_p & F_MARK) && (curr_p & F_FLAG)) {
            if(n_ptr(curr_node->next) == next_node) {// help physically delete
                rend_the_marked(curr_node,next_node,next_p);
            }
            curr_p = atomic_load(&curr_node->next);
            next_node = n_ptr(curr_p);
            if(next_node == 0) // arrive list end
                break;
            next_p = atomic_load(&next_node->next);
        }
        if(next_node && container_of(next_node,struct item, link)->value <= val) {
            curr_node = next_node;
            curr_p = atomic_load(&curr_node->next);
            next_node = n_ptr(curr_p);
        }
    }

    *left_node = curr_node;
    return next_node;
}


bool list_insert(struct nblist *the_list, val_t val)
{
    struct nblist_node *prev = NULL;
    struct nblist_node *next = list_search(val, &the_list->n, &prev);

    if(container_of(prev,struct item,link)->value == val) // duplicate key
        return false;
    struct item *it = malloc(sizeof(*it));
    if(!it)
        return false;
    it->value = val;
    struct nblist_node *newNode = &it->link;
    while (1) {
        uintptr_t prev_succ = prev->next; 
        if(prev_succ & F_FLAG) // help the corresponding deletion to complete
            clear_flag(prev,n_ptr(prev_succ));
        else {
            newNode->next = (uintptr_t)next;
            if(atomic_compare_exchange_strong_explicit(&prev->next, &next, (uintptr_t) newNode,
                        memory_order_release, memory_order_relaxed)) {
                // ensure prev and next are consecutive and prev isn't marked or flaged
                return true;
            }
            else {
                prev_succ = prev->next; 
                if(prev_succ & F_FLAG)
                    clear_flag(prev,n_ptr(prev_succ)); 
                while(prev->next & F_MARK) // Possibly a failure due to marking. Traverse a chain of backlinks to reach an unmarked node.
                    prev = prev->backlink;
            }
        }
        next = list_search(val,prev,&prev); // search two consecutive node again from prev node
        if(container_of(prev,struct item,link)->value == val) {
            free(it);
            return false;
        }
    }
}

bool list_delete(struct nblist *the_list, val_t val)
{
    struct nblist_node *prev = NULL;
    struct nblist_node *next = list_search(val-1, &the_list->n, &prev);

    if(!next || container_of(next,struct item,link)->value != val) // no such key
        return false;

    bool got = try_flag(&prev, prev->next, next);
    
    if (prev)
        clear_flag(prev, next);
    return got;
}



bool nblist_push(struct nblist *list, struct nblist_node *top, struct nblist_node *n)
{
    assert(((uintptr_t) n & F__MASK) == 0);
    uintptr_t old = atomic_load_explicit(&list->n.next, memory_order_acquire);
    while ((old & F_FLAG) != 0) {
        clear_flag(&list->n, n_ptr(old));
        old = atomic_load(&list->n.next);
    }
    assert((old & F_MARK) == 0);
    n->next = old;
    n->backlink = NULL;
    return n_ptr(old) == top && atomic_compare_exchange_strong_explicit(
                                    &list->n.next, &old, (uintptr_t) n,
                                    memory_order_release, memory_order_relaxed);
}

struct nblist_node *nblist_pop(struct nblist *list)
{
    struct nblist_node *p = &list->n;
    uintptr_t p_val = atomic_load(&p->next);
    assert((p_val & F_MARK) == 0); // not dead
    struct nblist_node *n = n_ptr(p_val);

    /* find the first n: p -> n where ¬p.flag ∧ ¬p.mark, and atomically set
     * p.flag .
     */
    while (n) {
        if ((p_val & F__MASK) != 0) { // p_val mark or flag
            p = n;
            p_val = atomic_load(&p->next);
        } else if (atomic_compare_exchange_strong(&p->next, &p_val,
                                                  p_val | F_FLAG)) { // 確保 p->next 沒 mark or flag
            break;
        }
        n = n_ptr(p_val);
    }
    if (!n) // empty
        return NULL;

    clear_flag(p, n);
    return n;
}

struct nblist_node *nblist_top(struct nblist *list)
{
    return n_ptr(atomic_load_explicit(&list->n.next, memory_order_acquire));
}

bool nblist_del(struct nblist *list, struct nblist_node *target)
{
    /* find p -> n, where n == target. */
    struct nblist_node *p = &list->n, *n = NULL;
    uintptr_t p_val = atomic_load(&p->next), n_val;
    while (n_ptr(p_val)) {
        n = n_ptr(p_val);
        n_val = atomic_load(&n->next);

        if ((n_val & F_MARK) != 0 && (p_val & F_FLAG) != 0) {
            /* complete an in-progress deletion. */
            rend_the_marked(p, n, n_val);
            if (n == target)
                return false;
            p_val = atomic_load(&p->next);
        } else if (n == target) { /* got it */
            break;
        }
        p = n;
        p_val = n_val;
    }
    if (!n_ptr(p_val))
        return false;

    /* flag and delete. */
    bool got = try_flag(&p, p_val, n); // Flagging the predecessor node
    if (p)
        clear_flag(p, n);
    return got;
}

static struct nblist_node *skip_dead_nodes(struct nblist_iter *it)
{
    while (it->cur) {
        uintptr_t next =
            atomic_load_explicit(&it->cur->next, memory_order_relaxed);
        if ((next & F_MARK) == 0)
            break;
        /* it->prev remains as before. */
        it->cur = n_ptr(next);
    }
    return it->cur;
}

struct nblist_node *nblist_first(struct nblist *list, struct nblist_iter *it)
{
    it->prev = (struct nblist_node *) &list->n;
    it->cur = n_ptr(atomic_load_explicit(&list->n.next, memory_order_acquire));
    return skip_dead_nodes(it);
}

struct nblist_node *nblist_next(struct nblist *list, struct nblist_iter *it)
{
    it->prev = it->cur;
    it->cur =
        n_ptr(atomic_load_explicit(&it->prev->next, memory_order_relaxed));
    return skip_dead_nodes(it);
}

bool nblist_del_at(struct nblist *list, struct nblist_iter *it)
{
    if (!it->cur)
        return false; /* edge case: cursor at end. */
    if (!it->prev)
        return false; /* repeat case: always NULL */

    uintptr_t cur_val =
        atomic_load_explicit(&it->cur->next, memory_order_relaxed);
    if ((cur_val & F_MARK) != 0)
        return false; /* already gone */

    struct nblist_node *p = it->prev;
    uintptr_t p_val = atomic_load_explicit(&p->next, memory_order_acquire);
    bool got = try_flag(&p, p_val, it->cur);
    it->prev = NULL;
    if (p)
        clear_flag(p, it->cur);
    return got;
}

int list_size(struct nblist *list)
{
    int size = 0;
    struct nblist_node *n = (struct nblist_node *) list->n.next;
    while(n) {
        n = (struct nblist_node *) n->next;
        size++;
    }
    return size;
}

void list_print(struct nblist *list)
{
    struct nblist_node *n = (struct nblist_node *) list->n.next;
    while(n) {
        printf("%ld ",container_of(n,struct item,link)->value);
        n = (struct nblist_node *) n->next;
    }
}