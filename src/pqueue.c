#include "pqueue.h"

#include <assert.h>
#include <memory.h>
#include <stdlib.h>

#include "sched.h"

#define PQUEUE_INITIAL_CAPACITY 32
#define PQUEUE_ROOT 1

static int pqueue_upholds_heap_property(struct pqueue* pq, pq_node_t k);

void pqueue_init(struct pqueue* pq)
{
    pq->capacity = PQUEUE_INITIAL_CAPACITY;
    pq->items = malloc((pq->capacity + PQUEUE_ROOT) * sizeof(struct pq_item));
    pq->size = 0;
}

void pqueue_destroy(struct pqueue* pq)
{
    free(pq->items);
}

static pq_node_t right(pq_node_t k)
{
    return k * 2 + 1;
}

static pq_node_t left(pq_node_t k)
{
    return k * 2;
}

static pq_node_t parent(pq_node_t k)
{
    return k / 2;
}

static int pqueue_is_node(struct pqueue* pq, pq_node_t k)
{
    return k > 0 && k <= pq->size;
}

static void pqueue_bubble_down(struct pqueue* pq, pq_node_t k)
{
    assert(pqueue_is_node(pq, k));
    pq_node_t l = left(k);
    pq_node_t r = right(k);

    pq_node_t smallest = k;
    if (l <= pq->size && pq->items[l].key < pq->items[smallest].key)
        smallest = l;
    if (r <= pq->size && pq->items[r].key < pq->items[smallest].key)
        smallest = r;

    if (smallest != k) {
        struct pq_item temp = pq->items[k];
        pq->items[k] = pq->items[smallest];
        pq->items[smallest] = temp;
        pq->items[k].task->pq_node = k;
        pq->items[smallest].task->pq_node = smallest;
        pqueue_bubble_down(pq, smallest);
    }
}

static void pqueue_bubble_up(struct pqueue* pq, pq_node_t k)
{
    if (k <= PQUEUE_ROOT)
        return;

    assert(pqueue_is_node(pq, k));

    pq_node_t p = parent(k);

    if (pq->items[k].key < pq->items[p].key) {
        struct pq_item temp = pq->items[k];
        pq->items[k] = pq->items[p];
        pq->items[p] = temp;
        pq->items[k].task->pq_node = k;
        pq->items[p].task->pq_node = p;
        pqueue_bubble_up(pq, p);
    }
}

static void pqueue_expand(struct pqueue* pq)
{
    size_t old_capacity = pq->capacity;
    struct pq_item* old_items = pq->items;

    pq->capacity *= 2;
    pq->items = malloc((pq->capacity + PQUEUE_ROOT) * sizeof(struct pq_item));

    memcpy(pq->items + PQUEUE_ROOT, old_items + PQUEUE_ROOT, old_capacity * sizeof(struct pq_item));
    free(old_items);
}

void pqueue_insert(struct pqueue* pq, uint64_t key, struct task* task)
{
    assert(task != NULL);
    if (pq->size * 2 >= pq->capacity)
        pqueue_expand(pq);
    pq->size++;
    pq->items[pq->size].key = key;
    pq->items[pq->size].task = task;
    task->pq_node = pq->size;
    pqueue_bubble_up(pq, pq->size);
    assert(pqueue_upholds_heap_property(pq, PQUEUE_ROOT));
}

struct pq_item* pqueue_min(struct pqueue* pq)
{
    if (pq->size < PQUEUE_ROOT)
        return NULL;
    return pq->items + PQUEUE_ROOT;
}

static void pqueue_bubble(struct pqueue* pq, pq_node_t k)
{
    assert(pqueue_is_node(pq, k));
    if (k == PQUEUE_ROOT || pq->items[parent(k)].key <= pq->items[k].key)
        pqueue_bubble_down(pq, k);
    else
        pqueue_bubble_up(pq, k);
}

void pqueue_decrease(struct pqueue* pq, pq_node_t k, uint64_t new_key)
{
    assert(pqueue_is_node(pq, k));
    assert(new_key <= pq->items[k].key);
    pq->items[k].key = new_key;
    pqueue_bubble(pq, k);
    assert(pqueue_upholds_heap_property(pq, PQUEUE_ROOT));
}

void pqueue_remove(struct pqueue* pq, pq_node_t k)
{
    assert(pqueue_is_node(pq, k));
    pq->items[k].task->pq_node = PQUEUE_NOT_A_NODE;
    pq->size--;
    if (k <= pq->size) {
        pq->items[k] = pq->items[pq->size + 1];
        pq->items[k].task->pq_node = k;
        pqueue_bubble(pq, k);
        assert(pqueue_upholds_heap_property(pq, PQUEUE_ROOT));
    }
}

int pqueue_is_empty(struct pqueue* pq)
{
    return pq->size == 0;
}

static int pqueue_upholds_heap_property(struct pqueue* pq, pq_node_t k)
{
    assert(pqueue_is_node(pq, k));
    assert(!pq->items[k].task->done);
    pq_node_t l = left(k);
    pq_node_t r = right(k);
    return (!pqueue_is_node(pq, l) || (pq->items[l].key >= pq->items[k].key && pqueue_upholds_heap_property(pq, l))) &&
           (!pqueue_is_node(pq, r) || (pq->items[r].key >= pq->items[k].key && pqueue_upholds_heap_property(pq, r)));
}