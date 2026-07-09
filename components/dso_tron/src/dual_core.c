#include <stdint.h>

/* ESP32 DPORT registers for APP_CPU boot */
#define DPORT_BASE          0x3FF00000
#define DPORT_APPCPU_CTRL_D (*(volatile uint32_t*)(DPORT_BASE + 0x0B00))
#define DPORT_APPCPU_CTRL_B (*(volatile uint32_t*)(DPORT_BASE + 0x0B04))

#define APPCPU_CTRL_B_RESET_VEC 0x100  /* bits 8..31 = reset vector */

void dual_core_start(void (*app_main)(void)) {
    /* Start address must be in IRAM and 4-byte aligned */
    uint32_t entry = (uint32_t)app_main;
    if (entry & 3) return;

    /* Set APP_CPU reset vector */
    DPORT_APPCPU_CTRL_B = entry >> 12;
    DPORT_APPCPU_CTRL_D = 1;  /* release APP_CPU from reset */

    /* APP_CPU is now running; it will start at entry point */
}
