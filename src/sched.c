#include "sched.h"

#include <assert.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>

#include "hreversal.h"

#define COMPUTATION_UNIT 3200

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

struct task_slice
{
    struct task* task;
    uint64_t start, end;
};

// Computes the internally used priority of the task (smaller is higher priority)
static uint64_t task_key(struct task* task)
{
    uint64_t total_priority = 0;
    for (struct tid_list* tid = &task->tids; tid != NULL; tid = tid->next)
        total_priority += tid->priority;
    assert(total_priority > 0);
    return (task->end - task->progress) / total_priority;
}

void sched_add_task(struct scheduler* sched,
                    int task_id,
                    uint8_t target_hash[SHA256_LEN],
                    uint64_t start,
                    uint64_t end,
                    uint8_t priority)
{
    assert(end > start);
    assert(priority > 0);

    pthread_mutex_lock(&sched->mtx);

    // Check if task has previously been requested. If it was previously completed, we send the
    // reponse immediately. If it is in progress, we add the client to the list of interested
    // parties and update the internally used priority. If the task has not been requested before,
    // we add it to the task queue as normal.
    struct ht_item* ht_item = htable_get(&sched->ht, target_hash);
    if (ht_item) {
        if (ht_item->task->done) {
#ifdef SB_VERBOSE
            printf("[HT hit] Previously completed\n");
#endif
            sched->callback(task_id, ht_item->value);
        } else {
#ifdef SB_VERBOSE
            printf("[HT hit] Task in progress\n");
#endif
            struct tid_list* tid = malloc(sizeof(struct tid_list));
            tid->id = task_id;
            tid->priority = priority;
            tid->next = ht_item->task->tids.next;
            ht_item->task->tids.next = tid;

            // Update queue position. If all remaining slices are in progress, the task might not be in the queue
            if (ht_item->task->pq_node != PQUEUE_NOT_A_NODE)
                pqueue_decrease(&sched->pq, ht_item->task->pq_node, task_key(ht_item->task));
        }
        pthread_mutex_unlock(&sched->mtx);
    } else {
        struct task* task = (struct task*)malloc(sizeof(struct task));
        memcpy(task->hash, target_hash, SHA256_LEN);
        task->start = start;
        task->end = end;
        task->progress = start;

        task->tids.id = task_id;
        task->tids.priority = priority;
        task->tids.next = NULL;
        task->done = 0;

        pqueue_insert(&sched->pq, task_key(task), task);
        htable_add(&sched->ht, task);

        pthread_mutex_unlock(&sched->mtx);
        pthread_cond_broadcast(&sched->wait_cond);
    }
}

// Free the linked list of clients and send the result to them.
static void sched_send_result_and_free(struct scheduler* sched, struct tid_list* tid, uint64_t result)
{
    if (tid == NULL)
        return;
    sched->callback(tid->id, result);
    sched_send_result_and_free(sched, tid->next, result);
    free(tid);
}

// Mark the task as completed. Send results to clients, remember the result in lookup
// table, and clean up.
static void sched_finalise_task(struct scheduler* sched, struct task* task, uint64_t result)
{
    pthread_mutex_lock(&sched->mtx);

    sched->callback(task->tids.id, result);
    sched_send_result_and_free(sched, task->tids.next, result);
    task->tids.next = NULL;

    struct ht_item* ht_item = htable_get(&sched->ht, task->hash);
    assert(ht_item);
    ht_item->value = result;
    task->done = 1;
    if (task->pq_node != PQUEUE_NOT_A_NODE)
        pqueue_remove(&sched->pq, task->pq_node);

    pthread_mutex_unlock(&sched->mtx);
}

// Gets a task for a worker (the caller) to work on. Function is blocking.
// Returns whether an abort signal was caught.
static int sched_get_task(struct scheduler* sched, struct task_slice* task_slice)
{
    pthread_mutex_lock(&sched->mtx);

    struct task* task = NULL;

    // Continue searching for a task. If no task is available, wait.
    while (1) {
        if (sched->abort)
            break;

        struct pq_item* pq_item;
        if ((pq_item = pqueue_min(&sched->pq))) {
            task = pq_item->task;
            assert(!task->done);

            task_slice->start = task->progress;
            task_slice->end = MIN(task->progress + COMPUTATION_UNIT, task->end);
            task_slice->task = task;

            task->progress = task_slice->end;

            // If this is the last slice, remove it from the task queue
            if (task->progress >= task->end)
                pqueue_remove(&sched->pq, task->pq_node);
            else
                pqueue_decrease(&sched->pq, task->pq_node, task_key(task));

            break;
        }

        assert(task == NULL);
        pthread_cond_wait(&sched->wait_cond, &sched->mtx);
    }

    int abort = sched->abort;

    pthread_mutex_unlock(&sched->mtx);

    return abort;
}

// Main worker loop. Look for a task, do the task, and finialise the task if the result was found.
static void sched_worker_loop(struct scheduler* sched)
{
    while (1) {
        struct task_slice task_slice;
        if (sched_get_task(sched, &task_slice))
            break;

        struct reversal_result result;
        reverse_hash(task_slice.task->hash, task_slice.start, task_slice.end, &task_slice.task->done, &result);

        if (result.success)
            sched_finalise_task(sched, task_slice.task, result.result);
    }
}

static void* sched_init_worker(void* message)
{
    struct scheduler* sched = (struct scheduler*)message;
    sched_worker_loop(sched);
    return NULL;
}

void sched_init(struct scheduler* sched, void (*callback)(int, uint64_t))
{
    int cpu_count = get_nprocs();
#ifdef SB_VERBOSE
    printf("Found %d CPUs.\n", cpu_count);
#endif

    sched->thread_count = cpu_count * 2;
    sched->threads = malloc(sizeof(pthread_t) * sched->thread_count);
    pthread_mutex_init(&sched->mtx, NULL);
    pthread_cond_init(&sched->wait_cond, NULL);
    sched->abort = 0;

    pqueue_init(&sched->pq);
    htable_init(&sched->ht);

    sched->callback = callback;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 65536);

    for (int i = 0; i < sched->thread_count; i++)
        pthread_create(sched->threads + i, &attr, sched_init_worker, sched);
}

static void task_free(struct task* task)
{
    free(task);
}

void sched_destroy(struct scheduler* sched)
{
    pthread_mutex_lock(&sched->mtx);
    sched->abort = 1;
    pthread_mutex_unlock(&sched->mtx);

    pthread_cond_broadcast(&sched->wait_cond);
    for (int i = 0; i < sched->thread_count; i++)
        pthread_join(sched->threads[i], NULL);
    free(sched->threads);

    pqueue_destroy(&sched->pq);
    htable_destroy(&sched->ht, task_free);

    pthread_mutex_destroy(&sched->mtx);
    pthread_cond_destroy(&sched->wait_cond);
}