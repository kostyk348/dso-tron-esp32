#include <stdint.h>
#include "sdkconfig.h"

/* ESP32 Timer Group 0, Timer 0 registers */
#define TIMG0_BASE          0x3FF5F000
#define TIMG0_T0CONFIG      (*(volatile uint32_t*)(TIMG0_BASE + 0x00))
#define TIMG0_T0LO          (*(volatile uint32_t*)(TIMG0_BASE + 0x04))
#define TIMG0_T0HI          (*(volatile uint32_t*)(TIMG0_BASE + 0x08))
#define TIMG0_T0UPDATE      (*(volatile uint32_t*)(TIMG0_BASE + 0x0C))
#define TIMG0_T0ALARMLO     (*(volatile uint32_t*)(TIMG0_BASE + 0x10))
#define TIMG0_T0ALARMHI     (*(volatile uint32_t*)(TIMG0_BASE + 0x14))
#define TIMG0_T0LOADLO      (*(volatile uint32_t*)(TIMG0_BASE + 0x18))
#define TIMG0_T0LOADHI      (*(volatile uint32_t*)(TIMG0_BASE + 0x1C))
#define TIMG0_T0LOAD        (*(volatile uint32_t*)(TIMG0_BASE + 0x20))

#define TIMG0_INT_ENA       (*(volatile uint32_t*)(TIMG0_BASE + 0x68))
#define TIMG0_INT_CLR       (*(volatile uint32_t*)(TIMG0_BASE + 0x6C))

#define TIMG_T0_EN          (1 << 31)
#define TIMG_T0_INCREASE    (1 << 30)
#define TIMG_T0_AUTORELOAD  (1 << 29)
#define TIMG_T0_DIVIDER_M   (0xFF)
#define TIMG_T0_DIVIDER_S   13
#define TIMG_T0_ALARM_EN    (1 << 10)

/* APB clock = 80 MHz on ESP32 typical.
   Tick every 1000 us = 80_000 cycles at prescaler 80 => 1 MHz. */
#define TICK_PRESCALER      80
#define TICK_ALARM_VALUE    1000    /* 1 kHz tick at 1 MHz */

void timer_init(void) {
    /* Disable timer during setup */
    TIMG0_T0CONFIG = 0;

    /* Set prescaler */
    TIMG0_T0CONFIG = (TICK_PRESCALER << TIMG0_T0_DIVIDER_S);

    /* Set alarm value for 1 kHz */
    TIMG0_T0ALARMLO = TICK_ALARM_VALUE & 0xFFFFFFFF;
    TIMG0_T0ALARMHI = 0;

    /* Enable alarm, autoreload, increase counter */
    TIMG0_T0CONFIG |= TIMG_T0_ALARM_EN | TIMG_T0_AUTORELOAD | TIMG_T0_INCREASE;

    /* Clear pending interrupts */
    TIMG0_INT_CLR = (1 << 0);

    /* Enable timer interrupt in TIMG */
    TIMG0_INT_ENA = (1 << 0);

    /* Enable interrupt at CPU level */
    extern void intr_timer_enable(void);
    intr_timer_enable();

    /* Start timer */
    TIMG0_T0UPDATE = 1;  /* load */
    TIMG0_T0CONFIG |= TIMG_T0_EN;
}

/* Timer interrupt handler — called from Level 1 interrupt vector */
void timer_isr(void) {
    /* Clear interrupt flag */
    TIMG0_INT_CLR = (1 << 0);

    /* Call scheduler tick for current core */
    extern int xPortGetCoreID(void);
    int core = xPortGetCoreID();
    extern void sched_tick(int core);
    sched_tick(core);
}
