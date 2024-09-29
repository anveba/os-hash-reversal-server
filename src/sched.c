#include "sched.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <time.h>

#include "hreversal.h"

#define COMPUTATION_UNIT 1000

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

struct task_slice
{
    struct task* task;
    uint64_t start, end;
};

// Thread unsafe
static void sched_add_task_to_list(struct scheduler* sched, struct task* task, uint8_t priority)
{
    task->next = sched->expired_tasks[priority].next;
    task->prev = &sched->expired_tasks[priority];
    if (sched->expired_tasks[priority].next)
        sched->expired_tasks[priority].next->prev = task;
    sched->expired_tasks[priority].next = task;
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

    task->id = task_id;
    task->workers = 0;
    task->done = 0;

    sched_add_task_to_list(sched, task, priority);

    pthread_mutex_unlock(&sched->mtx);

    pthread_cond_broadcast(&sched->wait_cond);
}

static void sched_finalise_task(struct scheduler* sched, struct task* task, struct reversal_result* result)
{
    if (result->success)
        sched->callback(task->id, result->result);

    pthread_mutex_lock(&sched->mtx);

    if (result->success) {
        if (task->prev)
            task->prev->next = task->next;
        if (task->next)
            task->next->prev = task->prev;
        task->done = 1;
    }

    assert(task->workers > 0);

    // We wait until all workers have stopped working on a task before freeing it.
    if (--task->workers == 0 && task->done)
        free(task);

    pthread_mutex_unlock(&sched->mtx);
}

static void sched_get_task(struct scheduler* sched, struct task_slice* task_slice)
{
    pthread_mutex_lock(&sched->mtx);

    struct task* task = NULL;

    // Continue searching for a task. If no task is available, wait.
    while (1) {

        // Search the active list then the expired list.
        struct task* initial_active = sched->active_tasks;
        do {
            // Search each linked list for each priority level.
            for (; sched->current < PRIORITY_LEVELS; sched->current++) {
                if (sched->active_tasks[sched->current].next != NULL) {
                    task = sched->active_tasks[sched->current].next;
                    sched->active_tasks[sched->current].next = task->next;
                    if (task->next)
                        task->next->prev = &sched->active_tasks[sched->current];
                    task->next = task->prev = NULL;
                    goto task_found;
                }
            }

            // Swap active and expired tasks.
            struct task* temp = sched->active_tasks;
            sched->active_tasks = sched->expired_tasks;
            sched->expired_tasks = temp;
            sched->current = 0;

        } while (sched->active_tasks != initial_active);
        assert(task == NULL);
        pthread_cond_wait(&sched->wait_cond, &sched->mtx);
    }
task_found:

    task_slice->start = task->progress;
    task_slice->end = MIN(task->progress + COMPUTATION_UNIT * (sched->current + 1), task->end);
    task_slice->task = task;

    task->progress = task_slice->end;
    task->workers++;

    // Add task back to list if it's not done.
    if (task->progress < task->end)
        sched_add_task_to_list(sched, task, sched->current);

    pthread_mutex_unlock(&sched->mtx);
}

static void sched_thread_loop(struct scheduler* sched)
{
    while (1) {
        struct task_slice task_slice;
        sched_get_task(sched, &task_slice);

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

    sched->active_tasks = sched->task_list1;
    sched->expired_tasks = sched->task_list2;
    sched->current = 0;

    for (int i = 0; i < PRIORITY_LEVELS; i++) {
        sched->expired_tasks[i].next = sched->active_tasks[i].next = NULL;
        sched->expired_tasks[i].prev = sched->active_tasks[i].prev = NULL;
    }

    sched->callback = callback;

    for (int i = 0; i < sched->thread_count; i++)
        pthread_create(sched->threads + i, NULL, sched_init_thread, sched);
}

void sched_destroy(struct scheduler* sched)
{
    // TODO free linked lists and stop threads more elegantly

    for (int i = 0; i < sched->thread_count; i++)
        pthread_cancel(sched->threads[i]);

    free(sched->threads);
    pthread_mutex_destroy(&sched->mtx);
    pthread_cond_destroy(&sched->wait_cond);
}