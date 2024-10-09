#ifndef PQUEUE_H_INCLUDED
#define PQUEUE_H_INCLUDED

#include <stdint.h>
#include <stdlib.h>

struct task;
typedef size_t pq_node_t;

#define PQUEUE_NOT_A_NODE 0

typedef struct pq_item
{
    uint64_t key;
    struct task* task;
} pq_item;

struct pqueue
{
    pq_item* items;
    size_t size, capacity;
};

void pqueue_init(struct pqueue* pq);
void pqueue_destroy(struct pqueue* pq);

void pqueue_insert(struct pqueue* pq, uint64_t key, struct task* task);

struct pq_item* pqueue_min(struct pqueue* pq);
void pqueue_decrease(struct pqueue* pq, pq_node_t k, uint64_t new_key);
void pqueue_remove(struct pqueue* pq, pq_node_t k);

int pqueue_is_empty(struct pqueue* pq);

#endif