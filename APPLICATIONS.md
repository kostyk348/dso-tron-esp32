# DSO-TRON ESP32 Application Scenarios

## 1. Dual-core audio DSP pipeline

**Problem:** Audio effects (EQ, reverb, pitch detection) need <1 ms latency.
On single-core, the audio buffer risks underflow during UART output or LED
update — clicks and pops.

**Solution:** Core 1 runs only the DSP chain, isolated from I/O jitter.

```
                 ┌──── lockfree SPSC (isochronous) ────┐
  I2S mic ──► DMA ──► FIR decimate ──► biquad EQ ──► DMA ──► I2S speaker
                         Core 1 only                        Core 1 only
                                    │
                         Goertzel pitch detect
                                    │
                              ┌─────┴─────┐
                              │   UART     │  Core 0: shell, status LED
                              │  shell     │
                              └───────────┘
```

**Why DSO-TRON:**
- No heap → no allocation jitter on audio path
- lockfree SPSC → Core 1 never waits for Core 0
- Static priorities → DSP at pri 3, shell at pri 1

---

## 2. Sensor fusion for drone flight controller

**Problem:** IMU at 1 kHz, mag at 100 Hz, baro at 50 Hz, control loop at
500 Hz. WCET must be bounded — priority inversion kills drones.

**Solution:** Static task table with known WCET.

| Task | Core | Pri | Period | WCET | Stack |
|---|---|---|---|---|---|
| `imu` | 0 | 3 | 1 ms | 80 µs | 512 B |
| `control` | 0 | 3 | 2 ms | 200 µs | 1 KB |
| `mag` | 1 | 2 | 10 ms | 50 µs | 256 B |
| `baro` | 1 | 2 | 20 ms | 30 µs | 256 B |
| `fusion` | 1 | 2 | 2 ms | 150 µs | 1 KB |
| `telemetry` | 0 | 1 | 100 ms | 20 µs | 512 B |
| `shell` | 0 | 0 | — | — | 2 KB |
| `idle` | 0/1 | 0 | — | — | 256 B |

**Why DSO-TRON:**
- 8 tasks × 4 KB max = 32 KB total RAM — fits ESP32 DRAM
- No dynamic allocation → WCET = pure compute, no GC/alloc
- Priority scheduling → IMU always preempts telemetry

---

## 3. CAN bus gateway (automotive / industrial)

**Problem:** ESP32 receives CAN frames on PRO_CPU, needs to filter and
forward on APP_CPU without blocking CAN RX.

```
  CAN bus ──► SN65HVD230 ──► ESP32 Core 0 ──► lockfree SPSC ──► Core 1 ──► UART
                                (CAN RX)                            (filter + log)
```

**Why DSO-TRON:**
- lockfree MPSC → multiple sensors can queue to one consumer
- Immediate yield → CAN RX task yields after each frame, tick handles rest
- No malloc → CAN frame pool is preallocated, zero-drop

---

## Comparison: DSO-TRON vs FreeRTOS for these scenarios

| Criterion | FreeRTOS | DSO-TRON |
|---|---|---|
| Max tasks (default) | ~10 | **16 (static)** |
| Stack check | Runtime | **Compile-time (static arrays)** |
| IPC latency | Variable (queue malloc) | **Lock-free, bounded** |
| Priority inversion | Mutex inheritance (complex) | **No inversion (lockfree)** |
| Scheduler jitter | Tick + deferred yield | **Tick + immediate** |
| RAM overhead | ~15 KB + per-task heap | **~8 KB fixed** |
| Determinism | Soft real-time | **Hard real-time capable** |
