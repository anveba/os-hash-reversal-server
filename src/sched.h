#ifndef SCHED_H_INCLUDED
#define SCHED_H_INCLUDED

#include <pthread.h>

#include "htable.h"
#include "pqueue.h"
#include "sha256.h"

// Task ID (requests) linked list
struct tid_list;
struct tid_list
{
    int id;
    uint8_t priority;
    struct tid_list* next;
};

struct task
{
    uint8_t hash[SHA256_LEN];
    uint64_t start;
    uint64_t end;
    uint64_t progress;

    pq_node_t pq_node;
    struct tid_list tids;
    uint8_t done;
};

struct scheduler
{
    pthread_t* threads;
    uint32_t thread_count;
    pthread_mutex_t mtx;
    pthread_cond_t wait_cond;
    uint8_t abort;

    struct pqueue pq;
    struct htable ht;

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