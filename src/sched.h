#ifndef SCHED_H_INCLUDED
#define SCHED_H_INCLUDED

#include <pthread.h>

#include "sha256.h"

#define PRIORITY_LEVELS 16

struct task
{
    uint8_t hash[SHA256_LEN];
    uint64_t start;
    uint64_t end;
    uint64_t progress;

    int id;
    uint32_t workers;
    uint8_t done;

    struct task *prev, *next;
};

struct scheduler
{
    pthread_t* threads;
    uint32_t thread_count;
    pthread_mutex_t mtx;
    pthread_cond_t wait_cond;

    // TODO currently task lists are wasting memory; only the linked list part is used.
    struct task task_list1[PRIORITY_LEVELS], task_list2[PRIORITY_LEVELS];
    struct task *expired_tasks, *active_tasks;
    uint32_t current;

    // Arguments are task ID and result.
    void (*callback)(int, uint64_t);
};

void sched_init(struct scheduler* sched, void (*callback)(int, uint64_t));

void sched_destroy(struct scheduler* sched);

void sched_add_task(struct scheduler* sched,
                    int task_id,
                    uint8_t target_hash[SHA256_LEN],
                    uint64_t start,
                    uint64_t end,
                    uint8_t priority);

#endif