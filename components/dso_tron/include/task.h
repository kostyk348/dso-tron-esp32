#pragma once

#include <stdint.h>

#define TASK_NAME_LEN 16
#define TASK_PRI_MAX  7
#define TASK_STACK_DEFAULT 4096

typedef enum { TASK_READY, TASK_BLOCKED, TASK_SUSPENDED, TASK_ZOMBIE } task_state_t;

typedef struct task_control_block {
    uint32_t *sp;          /* stack pointer (saved during ctx switch) */
    uint32_t  entry;       /* task entry point */
    uint32_t  arg;         /* argument */
    uint32_t  stack_base;  /* allocated stack memory */
    uint32_t  stack_size;
    char      name[TASK_NAME_LEN];
    int       pri;
    task_state_t state;
    int       core;        /* pinned core */
    int       tid;

    struct task_control_block *next;  /* for ready queue */
} tcb_t;

void task_init(tcb_t *tcb, const char *name, void (*entry)(void*), void *arg,
               int pri, int core, uint32_t *stack, uint32_t stack_size);
void task_exit(void);
tcb_t *task_current(int core);
