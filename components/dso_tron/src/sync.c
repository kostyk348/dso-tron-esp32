#include "sync.h"
#include "task.h"

void sem_init(sem_t *s, int initial) {
    s->count = initial;
    s->waiters = 0;
}

void sem_wait(sem_t *s) {
    while (1) {
        if (s->count > 0) {
            s->count--;
            return;
        }
        s->waiters++;           /* would block in real RTOS */
        /* spin for now (demo) */
        s->waiters--;
    }
}

void sem_post(sem_t *s) {
    s->count++;
}

void mutex_init(mutex_t *m) {
    m->locked = 0;
}

void mutex_lock(mutex_t *m) {
    while (__sync_lock_test_and_set(&m->locked, 1));
}

void mutex_unlock(mutex_t *m) {
    __sync_lock_release(&m->locked);
}
