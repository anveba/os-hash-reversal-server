#ifndef HTABLE_H_INCLUDED
#define HTABLE_H_INCLUDED

#include <stdint.h>
#include <stdlib.h>

#include "sha256.h"

struct task;
typedef uint64_t ht_value_t;

struct ht_item
{
    struct task* task;
    ht_value_t value;
};

struct htable
{
    struct ht_item* items;
    size_t size, capacity_power;
};

void htable_init(struct htable* ht);
void htable_destroy(struct htable* ht, void(task_free(struct task*)));

// Returns a pointer to a free slot in the table and sets the task field to the given.
struct ht_item* htable_add(struct htable* ht, struct task* task);
// Returns a pointer the slot in the table corresponding to the hash. NULL if it's not in the table.
struct ht_item* htable_get(struct htable* ht, uint8_t hash[SHA256_LEN]);

#endif