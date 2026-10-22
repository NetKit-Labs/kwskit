#pragma once

#include <cstddef>
#include <cstdint>

using k_timeout_t = int;
constexpr int K_NO_WAIT = 0, K_FOREVER = -1;
#define K_MSEC(ms) (ms)
#define K_KERNEL_STACK_MEMBER(name, size) alignas(8) char name[size]
#define K_KERNEL_STACK_SIZEOF(name) sizeof(name)

struct k_thread {
    void (*entry)(void *, void *, void *);
    void *context;
    int priority;
    bool started;
};

using k_tid_t = k_thread *;

struct k_mutex {
    bool held;
};

struct k_sem {
    unsigned count, limit;
};

struct k_msgq {
    unsigned used;
};

void k_mutex_init(k_mutex *);
int k_mutex_lock(k_mutex *, k_timeout_t);
void k_mutex_unlock(k_mutex *);
void k_sem_init(k_sem *, unsigned, unsigned);
void k_sem_give(k_sem *);
int k_sem_take(k_sem *, k_timeout_t);
void k_msgq_init(k_msgq *, char *, std::size_t, unsigned);
int k_msgq_put(k_msgq *, const void *, k_timeout_t);
int k_msgq_get(k_msgq *, void *, k_timeout_t);
void k_msgq_purge(k_msgq *);
k_tid_t k_thread_create(k_thread *, char *, std::size_t, void (*)(void *, void *, void *), void *,
    void *, void *, int, unsigned, k_timeout_t);
void k_thread_start(k_tid_t);
void k_thread_abort(k_tid_t);
k_tid_t k_current_get();
void k_thread_priority_set(k_tid_t, int);
void k_sched_lock();
void k_sched_unlock();
void k_msleep(int);
