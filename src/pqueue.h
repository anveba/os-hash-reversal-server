#ifndef PQUEUE_H_INCLUDED
#define PQUEUE_H_INCLUDED

#include <stdint.h>
#include <stdlib.h>

typedef struct pq_item
{
    uint64_t key;
    void* value;
} pq_item;

struct pqueue
{
    pq_item* items;
    size_t size;
    size_t capacity;
};

void pqueue_init(struct pqueue* pq);
void pqueue_destroy(struct pqueue* pq);

void pqueue_insert(struct pqueue* pq, uint64_t key, void* value);

void pqueue_min(struct pqueue* pq, struct pq_item* item);
void pqueue_decrease_min(struct pqueue* pq, uint64_t new_key);
void pqueue_remove_min(struct pqueue* pq);

int pqueue_empty(struct pqueue* pq);

#endif