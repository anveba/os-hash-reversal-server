#include "sched.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <time.h>

#include "hreversal.h"

#define COMPUTATION_UNIT 2000

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

struct task_slice
{
    struct task* task;
    uint64_t start, end;
};

static uint64_t task_key(struct task* task)
{
    return (task->end - task->progress) / (task->priority + 1);
}

void sched_add_task(struct scheduler* sched,
                    int task_id,
                    uint8_t target_hash[SHA256_LEN],
                    uint64_t start,
                    uint64_t end,
                    uint8_t priority)
{
    pthread_mutex_lock(&sched->mtx);

    struct task* task = (struct task*)malloc(sizeof(struct task));
    for (int i = 0; i < SHA256_LEN; i++)
        task->hash[i] = target_hash[i];
    task->start = start;
    task->end = end;
    task->progress = start;
    task->priority = priority;

    task->id = task_id;
    task->workers = 0;
    task->done = 0;

    pqueue_insert(&sched->pq, task_key(task), task);

    pthread_mutex_unlock(&sched->mtx);

    pthread_cond_broadcast(&sched->wait_cond);
}

static void sched_finalise_task(struct scheduler* sched, struct task* task, struct reversal_result* result)
{
    if (result->success)
        sched->callback(task->id, result->result);

    pthread_mutex_lock(&sched->mtx);

    if (result->success)
        task->done = 1;

    assert(task->workers > 0);

    // We wait until all workers have stopped working on a task before freeing it.
    if (--task->workers == 0 && task->done)
        free(task);

    pthread_mutex_unlock(&sched->mtx);
}

// Return whether abort signal was caught.
static int sched_get_task(struct scheduler* sched, struct task_slice* task_slice)
{
    pthread_mutex_lock(&sched->mtx);

    struct task* task = NULL;

    // Continue searching for a task. If no task is available, wait.
    while (1) {
        if (sched->abort)
            break;

        if (!pqueue_empty(&sched->pq)) {
            struct pq_item item;
            pqueue_min(&sched->pq, &item);
            task = item.value;

            task_slice->start = task->progress;
            task_slice->end = MIN(task->progress + COMPUTATION_UNIT, task->end);
            task_slice->task = task;

            task->progress = task_slice->end;
            task->workers++;

            // If this is the last slice, remove it from the task queue
            if (task->progress >= task->end)
                pqueue_remove_min(&sched->pq);
            else
                pqueue_decrease_min(&sched->pq, task_key(task));

            break;
        }

        assert(task == NULL);
        pthread_cond_wait(&sched->wait_cond, &sched->mtx);
    }

    int abort = sched->abort;

    pthread_mutex_unlock(&sched->mtx);

    return abort;
}

static void sched_thread_loop(struct scheduler* sched)
{
    while (1) {
        struct task_slice task_slice;
        if (sched_get_task(sched, &task_slice))
            break;

        struct reversal_result result;
        reverse_hash(task_slice.task->hash, task_slice.start, task_slice.end, &task_slice.task->done, &result);

        sched_finalise_task(sched, task_slice.task, &result);
    }
}

static void* sched_init_thread(void* message)
{
    struct scheduler* sched = (struct scheduler*)message;
    sched_thread_loop(sched);
    return NULL;
}

void sched_init(struct scheduler* sched, void (*callback)(int, uint64_t))
{
    int cpu_count = get_nprocs();
    printf("Found %d CPUs.\n", cpu_count);

    sched->thread_count = cpu_count * 2;
    sched->threads = malloc(sizeof(pthread_t) * sched->thread_count);
    pthread_mutex_init(&sched->mtx, NULL);
    pthread_cond_init(&sched->wait_cond, NULL);
    sched->abort = 0;

    pqueue_init(&sched->pq);

    sched->callback = callback;

    for (int i = 0; i < sched->thread_count; i++)
        pthread_create(sched->threads + i, NULL, sched_init_thread, sched);
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

    pthread_mutex_destroy(&sched->mtx);
    pthread_cond_destroy(&sched->wait_cond);
}