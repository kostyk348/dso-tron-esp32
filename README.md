# DSO-TRON for ESP32

Deterministic real-time OS for ESP32 (Xtensa LX6 dual-core) — ITRON 4.0 inspired.
Static allocation, lock-free IPC, preemptive scheduling, no heap.

```
dso-tron/esp32> ps
  TASK        PRI  STATE   CORE
  idle0       0    READY   0
  shell       1    READY   0
  led         1    READY   0
  dsp         2    READY   1

dso-tron/esp32> dsp on
dsp: Goertzel 1000 Hz power=32767 DETECTED
```

## Advantages vs alternatives

| | FreeRTOS (ESP-IDF) | Zephyr | **DSO-TRON (this)** |
|---|---|---|---|
| Allocation | Dynamic (heap) | Dynamic + static | **Static only** — zero malloc at runtime |
| Tasks | `xTaskCreate()` | `k_thread_create()` | **`task_init()` on preallocated TCB** |
| Scheduler | Preemptive + co-op | Preemptive | **Priority + RR per level** |
| IPC | Queue, semaphore, mutex | FIFO, mailbox, sem | **Semaphore, mutex + lockfree rings** |
| Lock-free | No | No | **Native SPSC/MPSC** (atomic, no scheduler lock) |
| Preemptibility | Tick + deferred yield | Tick + preempt points | **Tick + immediate yield** |
| Footprint | ~15 KB | ~20 KB | **~8 KB** (no heap, no dynamic alloc) |
| Licence | MIT | Apache 2.0 | **MIT** |

## Architecture

```
                   ┌─────────────────────────┐
  Core 0 (PRO_CPU) │  shell   led    idle0   │  UART console
                   │  SCHEDULER (1 kHz tick)  │  GPIO blink
                   └──────────┬──────────────┘
                              │ lockfree SPSC
                   ┌──────────┴──────────────┐
  Core 1 (APP_CPU) │    dsp           idle1  │  Goertzel DSP
                   │  SCHEDULER (1 kHz tick)  │  (qdsp math)
                   └─────────────────────────┘
```

### Components

| Module | Source | Role |
|---|---|---|
| **Scheduler** | `sched.c` | Priority 0..7, round-robin within level, per-core ready queue |
| **Task** | `task.c` | Static TCB allocation, stack init with Xtensa frame |
| **Context switch** | `ctx_switch.S` | Xtensa windowed register save/restore via `RFE` |
| **Timer** | `timer.c` | TIMG0 at 1 kHz, prescaler 80, auto-reload |
| **Dual-core** | `dual_core.c` | APP_CPU release via DPORT register |
| **Sync** | `sync.c` | Semaphore, mutex (atomic, spin for now) |
| **Shell** | `main.c` | UART shell: ps, dsp on, led, help |

## Project applications

### 1. Audio DSP co-processor

Dual-core audio pipeline: PRO_CPU runs control + UART, APP_CPU runs fixed-point DSP:

```
I2S mic → lockfree SPSC → Goertzel/Biquad/FIR → I2S speaker
          ↓
      UART shell (spectrum, detection events)
```

Benefit over FreeRTOS: deterministic timing, no heap fragmentation, lock-free IPC guarantees no deadline miss on DSP path.

### 2. Sensor fusion node

Multiple sensors (IMU, magnetometer, barometer) on SPI/I2C:

| Task | Core | Priority | Period |
|---|---|---|---|
| `imu_read` | 0 | 2 | 1 ms |
| `mag_read` | 0 | 2 | 10 ms |
| `fusion` | 1 | 2 | 1 ms |
| `uart_report` | 0 | 1 | 100 ms |
| `led_heartbeat` | 0 | 0 | 500 ms |

Static allocation guarantees worst-case execution time (WCET) — critical for control loops.

### 3. CAN gateway (with DSO-TRON x86 + CAN controller)

PRO_CPU: CAN RX/TX + protocol. APP_CPU: filter + logging. lockfree SPSC bridges them without priority inversion.

## Quick start

### 1. Prerequisites

```bash
git clone https://github.com/kostyk348/dso-tron-esp32.git
cd dso-tron-esp32
```

Install ESP-IDF v5.4:

```bash
git clone --depth=1 --branch v5.4 https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32
cd ..
```

### 2. Build

```bash
. ./esp-idf/export.sh
idf.py build
```

### 3. Flash

Подключите ESP32 через USB-UART (CP2102, CH340, Silicon Labs):

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Если порт другой — проверьте `ls /dev/ttyUSB*` или `ls /dev/ttyACM*`.

### 4. QEMU (без железа)

```bash
qemu-system-xtensa -machine esp32 -nographic -kernel build/dso-tron-esp32.elf
```

### 5. Shell commands

| Command | Description |
|---|---|
| `ps` | Список задач + состояние + ядро |
| `dsp on` | Запустить Goertzel детектор на ядре 1 |
| `led` | Переключить LED |
| `help` | Список команд |

## Build system

ESP-IDF component model. DSO-TRON — `components/dso_tron/`, демо — `main/`.

```bash
idf.py menuconfig  # DSO-TRON → Max tasks
idf.py build
idf.py -p /dev/ttyUSB0 flash
```

## Connecting repos

DSO-TRON ESP32 интегрирует код из:

- **[DSO-TRON](https://github.com/kostyk348/DSO-TRON)** — ядро RTOS (scheduler, task model)
- **[qdsp](https://github.com/kostyk348/qdsp)** — Q15 Goertzel на втором ядре
- **[lockfree](https://github.com/kostyk348/lockfree)** — SPSC/MPSC для межъядерной связи
- **[MPTC](https://github.com/kostyk348/MPTC)** — ternary/dual-number algebra (future)

## License

[MIT](LICENSE)
