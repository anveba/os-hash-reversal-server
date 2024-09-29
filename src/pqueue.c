#include "pqueue.h"

#include <assert.h>
#include <memory.h>
#include <stdlib.h>

#define INITIAL_CAPACITY 64

void pqueue_init(struct pqueue* pq)
{
    pq->capacity = INITIAL_CAPACITY;
    pq->items = malloc((pq->capacity + 1) * sizeof(struct pq_item));
    pq->size = 0;
}

void pqueue_destroy(struct pqueue* pq)
{
    free(pq->items);
}

static size_t right(size_t k)
{
    return k * 2 + 1;
}

static size_t left(size_t k)
{
    return k * 2;
}

static size_t parent(size_t k)
{
    return k / 2;
}

static void pqueue_bubble_down(struct pqueue* pq, size_t k)
{
    if (k * 2 > pq->size)
        return;

    size_t l = left(k);
    size_t r = right(k);

    size_t idx_of_smallest = k;
    if (pq->items[l].key < pq->items[idx_of_smallest].key)
        idx_of_smallest = l;
    if (pq->items[r].key < pq->items[idx_of_smallest].key)
        idx_of_smallest = r;

    if (idx_of_smallest != k) {
        struct pq_item temp = pq->items[k];
        pq->items[k] = pq->items[idx_of_smallest];
        pq->items[idx_of_smallest] = temp;
        pqueue_bubble_down(pq, idx_of_smallest);
    }
}

static void pqueue_bubble_up(struct pqueue* pq, size_t k)
{
    if (k <= 1)
        return;

    size_t p = parent(k);

    if (pq->items[k].key < pq->items[p].key) {
        struct pq_item temp = pq->items[k];
        pq->items[k] = pq->items[p];
        pq->items[p] = temp;
        pqueue_bubble_up(pq, p);
    }
}

static void pqueue_expand(struct pqueue* pq)
{
    size_t old_capacity = pq->capacity;
    struct pq_item* old_items = pq->items;

    pq->capacity *= 2;
    pq->items = malloc((pq->capacity + 1) * sizeof(struct pq_item));

    memcpy(pq->items + 1, old_items + 1, old_capacity * sizeof(struct pq_item));
    free(old_items);
}

void pqueue_insert(struct pqueue* pq, uint64_t key, void* value)
{
    if (pq->size * 2 >= pq->capacity)
        pqueue_expand(pq);
    pq->size++;
    pq->items[pq->size].key = key;
    pq->items[pq->size].value = value;
    pqueue_bubble_up(pq, pq->size);
}

void pqueue_min(struct pqueue* pq, struct pq_item* item)
{
    assert(pq->size > 0);
    item->key = pq->items[1].key;
    item->value = pq->items[1].value;
}

void pqueue_decrease_min(struct pqueue* pq, uint64_t new_key)
{
    assert(pq->size > 0);
    assert(new_key <= pq->items[1].key);
    pq->items[1].key = new_key;
}

void pqueue_remove_min(struct pqueue* pq)
{
    assert(pq->size > 0);
    pq->items[1] = pq->items[pq->size];
    pq->size--;
    pqueue_bubble_down(pq, 1);
}

int pqueue_empty(struct pqueue* pq)
{
    return pq->size == 0;
}