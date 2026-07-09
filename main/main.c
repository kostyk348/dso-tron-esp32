#include "task.h"
#include "sync.h"
#include <stdio.h>
#include <string.h>

/* Task stacks (static allocation — ITRON style) */
#define STACK_SIZE 4096
static uint32_t stack_shell[STACK_SIZE / sizeof(uint32_t)];
static uint32_t stack_dsp[STACK_SIZE / sizeof(uint32_t)];
static uint32_t stack_led[STACK_SIZE / sizeof(uint32_t)];
static uint32_t stack_idle0[256 / sizeof(uint32_t)];
static uint32_t stack_idle1[256 / sizeof(uint32_t)];

static tcb_t tcb_shell, tcb_dsp, tcb_led, tcb_idle0, tcb_idle1;
static sem_t dsp_sem;

/* Q15 Goertzel from qdsp port */
typedef short q15_t;
typedef struct { int32_t coeff; q15_t s_prev, s_prev2; int n, N; } goertzel_t;

static q15_t goertzel_process(goertzel_t *g, q15_t x) {
    int32_t s = (int32_t)x + ((g->coeff * (int32_t)g->s_prev) >> 15) - (int32_t)g->s_prev2;
    g->s_prev2 = g->s_prev;
    g->s_prev = (s > 32767) ? 32767 : (s < -32768) ? -32768 : (q15_t)s;
    g->n++;
    return g->s_prev;
}

static q15_t goertzel_power(goertzel_t *g) {
    int32_t sp = g->s_prev, sp2 = g->s_prev2;
    int32_t term = (g->coeff * (int32_t)(sp - sp2)) >> 15;
    int32_t mag = sp * sp + sp2 * sp2 - term * (sp - sp2);
    return (q15_t)(mag >> 15);
}

/* Generate 1000 Hz tone for Goertzel test */
static q15_t gen_tone(int i, int fs, int freq) {
    double t = (double)i / fs;
    double x = 0.5 * sin(2 * 3.14159265 * freq * t);
    return (q15_t)(x * 32767.0);
}

/* ================================================================== */
/*  Tasks                                                              */
/* ================================================================== */

static void idle_task(void *arg) {
    (void)arg;
    while (1) {
        /* Low-power wait */
        __asm__ volatile("waiti 0");
    }
}

static void led_task(void *arg) {
    (void)arg;
    /* LED on GPIO2 (ESP32 DevKitC) */
    volatile uint32_t *gpio_out = (uint32_t*)0x3FF44004;
    volatile uint32_t *gpio_en = (uint32_t*)0x3FF44020;
    *gpio_en |= (1 << 2);
    int state = 0;
    while (1) {
        state = !state;
        if (state)
            *gpio_out |= (1 << 2);
        else
            *gpio_out &= ~(1 << 2);
        /* Busy-wait ~500 ms (approximate) */
        for (volatile int i = 0; i < 500000; i++);
    }
}

static void dsp_task(void *arg) {
    (void)arg;
    goertzel_t gz;
    while (1) {
        sem_wait(&dsp_sem);

        /* Run Goertzel on 480 samples of 1000 Hz tone */
        int fs = 48000, freq = 1000, n = 480;
        gz.coeff = 64973;
        gz.s_prev = gz.s_prev2 = 0;
        gz.n = 0;
        gz.N = n;
        for (int i = 0; i < n; i++)
            goertzel_process(&gz, gen_tone(i, fs, freq));
        q15_t power = goertzel_power(&gz);
        printf("dsp: Goertzel %d Hz power=%d %s\n",
               freq, power, power > 8191 ? "DETECTED" : "low");
    }
}

static void shell_task(void *arg) {
    (void)arg;
    printf("\ndso-tron/esp32> ");

    char buf[64];
    int pos = 0;
    while (1) {
        /* Poll UART (simplified: assume ESP-IDF stdin) */
        int ch = getchar();
        if (ch < 0) continue;
        if (ch == '\r' || ch == '\n') {
            buf[pos] = 0;
            pos = 0;

            if (strcmp(buf, "ps") == 0) {
                printf("  TASK        PRI  STATE   CORE\n");
                /* Print registered tasks (simplified) */
                for (int i = 0; i < 5; i++) {
                    tcb_t *t = NULL;
                    switch (i) {
                        case 0: t = &tcb_idle0; break;
                        case 1: t = &tcb_shell; break;
                        case 2: t = &tcb_dsp; break;
                        case 3: t = &tcb_led; break;
                        case 4: t = &tcb_idle1; break;
                    }
                    if (t) printf("  %-12s %d   %s   %d\n",
                        t->name, t->pri,
                        t->state == TASK_READY ? "READY" :
                        t->state == TASK_BLOCKED ? "BLOCK" : "SUSP",
                        t->core);
                }
            } else if (strcmp(buf, "dsp on") == 0) {
                sem_post(&dsp_sem);
                printf("dsp: started\n");
            } else if (strcmp(buf, "help") == 0) {
                printf("commands: ps  dsp on  led  help\n");
            } else if (buf[0]) {
                printf("? %s\n", buf);
            }
            printf("dso-tron/esp32> ");
        } else if (ch == 127 || ch == '\b') {
            if (pos > 0) { pos--; printf("\b \b"); }
        } else if (pos < (int)sizeof(buf) - 1 && ch >= ' ') {
            buf[pos++] = ch;
            putchar(ch);
        }
    }
}

/* APP_CPU entry point */
void app_cpu_main(void) {
    /* Register idle task for core 1 */
    task_init(&tcb_idle1, "idle1", idle_task, NULL, 0, 1,
              stack_idle1, sizeof(stack_idle1));
    task_register(&tcb_idle1);

    /* Register DSP task on core 1 */
    task_init(&tcb_dsp, "dsp", dsp_task, NULL, 2, 1,
              stack_dsp, sizeof(stack_dsp));
    task_register(&tcb_dsp);

    /* Add to ready queue */
    extern void sched_add_ready(tcb_t*);
    sched_add_ready(&tcb_idle1);
    sched_add_ready(&tcb_dsp);

    /* Start scheduling on core 1 */
    extern void sched_start(void);
    sched_start();
    /* never returns */
}

void app_main(void) {
    sem_init(&dsp_sem, 0);

    /* Register tasks on core 0 */
    task_init(&tcb_idle0, "idle0", idle_task, NULL, 0, 0,
              stack_idle0, sizeof(stack_idle0));
    task_register(&tcb_idle0);

    task_init(&tcb_shell, "shell", shell_task, NULL, 1, 0,
              stack_shell, sizeof(stack_shell));
    task_register(&tcb_shell);

    task_init(&tcb_led, "led", led_task, NULL, 1, 0,
              stack_led, sizeof(stack_led));
    task_register(&tcb_led);

    /* Add to ready queue */
    extern void sched_add_ready(tcb_t*);
    sched_add_ready(&tcb_idle0);
    sched_add_ready(&tcb_shell);
    sched_add_ready(&tcb_led);

    /* Initialize timer for tick */
    extern void timer_init(void);
    timer_init();

    /* Start APP_CPU */
    extern void dual_core_start(void (*)(void));
    dual_core_start(app_cpu_main);

    /* Start scheduler on core 0 */
    extern void sched_start(void);
    sched_start();
    /* never returns */
}
