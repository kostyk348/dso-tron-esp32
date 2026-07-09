#include "task.h"
#include <string.h>

void task_init(tcb_t *tcb, const char *name, void (*entry)(void*), void *arg,
               int pri, int core, uint32_t *stack, uint32_t stack_size) {
    memset(tcb, 0, sizeof(*tcb));
    strncpy(tcb->name, name, TASK_NAME_LEN - 1);
    tcb->entry = (uint32_t)entry;
    tcb->arg = (uint32_t)arg;
    tcb->pri = pri;
    tcb->core = core;
    tcb->state = TASK_SUSPENDED;
    tcb->stack_base = (uint32_t)stack;
    tcb->stack_size = stack_size;

    /* Initialize stack for context restore.
       Stack grows downward. Top of stack = stack_base + stack_size.
       We set the initial SP to top-of-stack aligned. */
    uint32_t *sp = stack + stack_size / sizeof(uint32_t);

    /* Push initial context frame (XtExcFrame compatible):
       We store values that _xt_context_restore expects:
       a0-a15, PC, PS, SAR, EXCCAUSE, EXCVADDR, LBEG, LEND, LCOUNT

       For a first-run task, we set up a minimal frame:
       - PC = entry point
       - PS = 0x0001F000 (WOE=1, INTLEVEL=0, OWB=0, CALLINC=1)
       - a0 = return address (point to task_exit trampoline)
       - a1 = SP (stack top)
    */
    extern char _dso_task_exit_tramp[];
    uint32_t ps_val = 0x0001F000;  /* WOE=1, CALLINC=1, INTLEVEL=0, OWB=0 */
    uint32_t *fp = sp;

    *--fp = 0;                          /* EXCVADDR */
    *--fp = 0;                          /* EXCCAUSE */
    *--fp = 0;                          /* SAR */
    *--fp = 0;                          /* LCOUNT */
    *--fp = 0;                          /* LEND */
    *--fp = 0;                          /* LBEG */
    *--fp = (uint32_t)_dso_task_exit_tramp; /* a0 (return PC after task func) */
    for (int i = 1; i < 16; i++)
        *--fp = 0;                      /* a1-a15 */
    *--fp = (uint32_t)entry;            /* PC */
    *--fp = ps_val;                     /* PS */

    tcb->sp = fp;
}

void task_exit(void) {
    tcb_t *self = task_current(0);
    if (self) self->state = TASK_ZOMBIE;
    sched_yield(0);
    while (1);
}
