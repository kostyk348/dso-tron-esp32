#include <xtensa/config/core-isa.h>

    .section .iram1,"ax"

/* ------------------------------------------------------------------ */
/*  _dso_first_task — start the first task (no context to save)       */
/*  a2 = tcb_t pointer                                                */
/* ------------------------------------------------------------------ */
    .align  4
    .global _dso_first_task
    .type   _dso_first_task,@function
_dso_first_task:
    l32i    a1, a2, 0          /* tcb->sp */
    /* Restore frame: PS is at fp[1], PC at fp[0] */
    l32i    a3, a1, 4          /* PS */
    l32i    a4, a1, 0          /* PC */
    wsr     a3, PS
    rsync
    /* Set WINDOWSTART = 1 */
    movi    a5, 1
    wsr     a5, WINDOWSTART
    isync
    /* Restore a0-a15 from stack frame */
    l32i    a0, a1, 8          /* a0 = return address (task_exit tramp) */
    l32i    a2, a1, 12
    l32i    a3, a1, 16
    l32i    a4, a1, 20
    l32i    a5, a1, 24
    l32i    a6, a1, 28
    l32i    a7, a1, 32
    l32i    a8, a1, 36
    l32i    a9, a1, 40
    l32i    a10, a1, 44
    l32i    a11, a1, 48
    l32i    a12, a1, 52
    l32i    a13, a1, 56
    l32i    a14, a1, 60
    l32i    a15, a1, 64
    /* Jump to PC (ret to entry with WOE, which will do entry/retw) */
    wsr     a4, 0              /* EPC1 — set exception PC */
    rsr     a3, PS
    movi    a4, 0x0001F001     /* WOE=1, EXCM=1 (to use RFE) */
    wsr     a4, PS
    rsync
    rfe                      /* Return from exception -> jumps to PC with WOE */

/* ------------------------------------------------------------------ */
/*  _dso_ctx_switch — save current context, restore next               */
/*  a2 = prev_tcb_ptr (address of current task's tcb_t*)              */
/*  a3 = next_tcb (tcb_t pointer)                                     */
/* ------------------------------------------------------------------ */
    .align  4
    .global _dso_ctx_switch
    .type   _dso_ctx_switch,@function
_dso_ctx_switch:
    entry   a1, 32

    /* Save current SP to prev task's TCB */
    s32i    a1, a2, 0          /* *prev->sp = current SP */

    /* Load next task's SP from next TCB */
    l32i    a1, a3, 0          /* SP = next->sp */

    /* Add 4 for the EXCM-1 trick: the saved frame has PS at fp[1], PC at fp[0] */
    /* Actually we need to do this properly: restore PS, PC via RFE */
    l32i    a4, a1, 4          /* next PS */
    l32i    a5, a1, 0          /* next PC */
    wsr     a4, PS
    wsr     a5, 0              /* EPC1 */
    rsync

    /* Restore a0-a15 from new stack frame */
    l32i    a0, a1, 8
    l32i    a2, a1, 12
    l32i    a3, a1, 16
    l32i    a6, a1, 28
    l32i    a7, a1, 32
    l32i    a8, a1, 36
    l32i    a9, a1, 40
    l32i    a10, a1, 44
    l32i    a11, a1, 48
    l32i    a12, a1, 52
    l32i    a13, a1, 56
    l32i    a14, a1, 60
    l32i    a15, a1, 64
    /* a4 and a5 were loaded with PS/PC earlier */
    l32i    a4, a1, 20
    l32i    a5, a1, 24

    /* RFE to switch to new PC with new PS */
    movi    a3, 0x0001F001
    wsr     a3, PS
    rsync
    rfe

/* ------------------------------------------------------------------ */
/*  _dso_task_exit_tramp — called when task function returns           */
/* ------------------------------------------------------------------ */
    .align  4
    .global _dso_task_exit_tramp
    .type   _dso_task_exit_tramp,@function
_dso_task_exit_tramp:
    call4   task_exit
1:  j       1b
