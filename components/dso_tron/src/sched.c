#include "task.h"
#include "sdkconfig.h"

#define MAX_TASKS CONFIG_DSO_MAX_TASKS
#define NUM_PRIOS (TASK_PRI_MAX + 1)

static tcb_t *ready_queues[NUM_PRIOS];
static tcb_t *current_tasks[2];  /* per-core */
static int    num_tasks = 0;
static tcb_t *task_table[MAX_TASKS];

/* debug IDs for IPC shell output */
extern int dso_printf(const char *fmt, ...);

void sched_init(void) {
    for (int i = 0; i < NUM_PRIOS; i++)
        ready_queues[i] = NULL;
    current_tasks[0] = NULL;
    current_tasks[1] = NULL;
}

tcb_t *task_current(int core) {
    if (core < 0 || core > 1) return NULL;
    return current_tasks[core];
}

int task_register(tcb_t *tcb) {
    if (num_tasks >= MAX_TASKS) return -1;
    tcb->tid = num_tasks;
    task_table[num_tasks++] = tcb;
    return tcb->tid;
}

void sched_add_ready(tcb_t *tcb) {
    int pri = tcb->pri;
    tcb->state = TASK_READY;
    tcb->next = NULL;
    if (!ready_queues[pri]) {
        ready_queues[pri] = tcb;
        tcb->next = tcb;  /* circular */
    } else {
        tcb->next = ready_queues[pri]->next;
        ready_queues[pri]->next = tcb;
        ready_queues[pri] = tcb;
    }
}

static tcb_t *sched_remove_head(int pri) {
    if (!ready_queues[pri]) return NULL;
    tcb_t *head = ready_queues[pri]->next;
    if (head == ready_queues[pri]) {
        ready_queues[pri] = NULL;
    } else {
        ready_queues[pri]->next = head->next;
    }
    head->next = NULL;
    return head;
}

tcb_t *sched_pick_next(int core) {
    for (int p = TASK_PRI_MAX; p >= 0; p--) {
        if (!ready_queues[p]) continue;
        /* round-robin: move head to tail, pick the new head */
        tcb_t *prev = ready_queues[p];
        tcb_t *next = prev->next;
        /* Advance round-robin pointer */
        ready_queues[p] = next;
        return next;
    }
    return NULL;
}

void sched_yield(int core) {
    tcb_t *prev = current_tasks[core];
    tcb_t *next = sched_pick_next(core);
    if (!next || next == prev) return;

    current_tasks[core] = next;

    /* Architecture-specific context switch */
    extern void _dso_ctx_switch(tcb_t **prev_ptr, tcb_t *next);
    _dso_ctx_switch(&prev, next);
}

/* Called from timer interrupt (serialized on each core) */
void sched_tick(int core) {
    tcb_t *prev = current_tasks[core];
    if (!prev) return;
    tcb_t *next = sched_pick_next(core);
    if (!next || next == prev) return;

    current_tasks[core] = next;
    extern void _dso_ctx_switch(tcb_t **prev_ptr, tcb_t *next);
    _dso_ctx_switch(&prev, next);
}

void sched_start(void) {
    /* Pick first task for each core */
    for (int c = 0; c < 2; c++) {
        tcb_t *t = sched_pick_next(c);
        if (t) current_tasks[c] = t;
    }
    /* Start first task on current core */
    if (current_tasks[0]) {
        extern void _dso_first_task(tcb_t *tcb);
        _dso_first_task(current_tasks[0]);
    }
}
