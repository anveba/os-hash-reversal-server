#include "htable.h"

#include <assert.h>
#include <memory.h>

#include "sched.h"

#define HTABLE_INITIAL_CAPACITY_POWER 1
#define HTABLE_LOAD_FACTOR 0.5f

void htable_init(struct htable* ht)
{
    ht->capacity_power = HTABLE_INITIAL_CAPACITY_POWER;
    ht->items = malloc((1 << ht->capacity_power) * sizeof(struct ht_item));

    for (size_t i = 0; i < (1 << ht->capacity_power); i++)
        ht->items[i].task = NULL;

    ht->size = 0;
}

void htable_destroy(struct htable* ht, void(task_free(struct task*)))
{
    for (size_t i = 0; i < (1 << ht->capacity_power); i++)
        if (ht->items[i].task)
            task_free(ht->items[i].task);
    free(ht->items);
}

static size_t htable_index_of(struct htable* ht, size_t n)
{
    return n & ((1 << ht->capacity_power) - 1);
}

static struct ht_item* htable_get_free(struct htable* ht, uint8_t hash[SHA256_LEN])
{
    size_t idx = htable_index_of(ht, *((size_t*)hash));
    while (ht->items[idx].task)
        idx = htable_index_of(ht, idx + 1);
    return ht->items + idx;
}

static void htable_expand(struct htable* ht)
{
    struct ht_item* old_items = ht->items;

    ht->capacity_power++;
    ht->items = malloc((1 << ht->capacity_power) * sizeof(struct ht_item));
    for (size_t i = 0; i < (1 << ht->capacity_power); i++)
        ht->items[i].task = NULL;

    for (size_t i = 0; i < (1 << (ht->capacity_power - 1)); i++)
        if (old_items[i].task)
            memcpy(htable_get_free(ht, old_items[i].task->hash), old_items + i, sizeof(struct ht_item));

    free(old_items);
}

struct ht_item* htable_add(struct htable* ht, struct task* task)
{
    if (++ht->size >= HTABLE_LOAD_FACTOR * (1 << ht->capacity_power))
        htable_expand(ht);
    struct ht_item* item = htable_get_free(ht, task->hash);
    item->task = task;
    return item;
}

struct ht_item* htable_get(struct htable* ht, uint8_t hash[SHA256_LEN])
{
    for (size_t idx = htable_index_of(ht, *((size_t*)hash)); ht->items[idx].task; idx = htable_index_of(ht, idx + 1))
        if (!memcmp(ht->items[idx].task->hash, hash, SHA256_LEN))
            return ht->items + idx;
    return NULL;
}