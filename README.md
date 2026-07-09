# DSO-TRON for ESP32

Deterministic real-time OS kernel ported to ESP32 (Xtensa LX6 dual-core).

Based on [DSO-TRON](https://github.com/kostyk348/DSO-TRON) — ITRON 4.0 inspired RTOS
with static allocation, lock-free IPC, and time-triggered scheduling.

## Features

- Preemptive multi-tasking (1 kHz tick)
- Dual-core SMP (PRO_CPU + APP_CPU)
- Lock-free SPSC/MPSC rings (port from [lockfree](https://github.com/kostyk348/lockfree))
- Fixed-point DSP pipeline (port from [qdsp](https://github.com/kostyk348/qdsp))
- ESP-IDF component — uses IDF bootloader + HAL, DSO-TRON replaces FreeRTOS

## Demo

Power up, open UART (115200 baud), see shell:

```
dso-tron/esp32> ps
  TASK  PRI STATE  CORE
  idle   0  READY  0
  shell  1  READY  0
  dsp    2  READY  1
dso-tron/esp32> dsp tone 1000
dsp: Goertzel 1000 Hz on core 1
dso-tron/esp32> led blink 500
led: blinking at 500 ms
```

## Build

```bash
# Prerequisites: ESP-IDF (or just xtensa-esp32-elf toolchain)
export IDF_PATH=/path/to/esp-idf
. $IDF_PATH/export.sh

idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## QEMU (no hardware)

```bash
qemu-system-xtensa -machine esp32 -nographic -kernel build/dso-tron-esp32.elf
```

## Directory

```
components/dso_tron/   — RTOS kernel
main/                  — demo (shell + DSP + LED)
```

## License

MIT
