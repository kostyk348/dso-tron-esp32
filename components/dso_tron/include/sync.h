#pragma once

#include <stdint.h>

typedef struct {
    volatile int count;
    volatile int waiters;
} sem_t;

void sem_init(sem_t *s, int initial);
void sem_wait(sem_t *s);
void sem_post(sem_t *s);

typedef struct {
    volatile int locked;
} mutex_t;

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);
