# Wearable sensor drivers

All sensors share `I2C1` on PB6 (SCL) and PB7 (SDA).

## Runtime roles

- MAX30208: body-temperature acquisition. The active STM32 HAL port is
  `Drivers/max30208.c`; asynchronous scheduling is handled by
  `Application/sensor_manager.c`.
- MAX86150: Red/IR PPG (HR/SpO2) and single-lead ECG, one mode at a time
  (`max86150_optical.*`). ECG runs at 200 sps with IA 9.5 x PGA 8 gain and is
  drained by the `CFG_TASK_WEARABLE_ECG_ID` task every 45 ms (FIFO lasts
  160 ms). INTB is on PB4 but not used yet (polled FIFO).
- LIS2DUXS12TR: accelerometer, QVar (wear detection) and Machine Learning Core
  (MLC). Board wiring: INT1 is the QVar electrode, INT2 is unconnected, and the
  RES pin is connected to STM32WB09 PB2 as the interrupt line. Runtime I2C
  processing is deferred from the GPIO ISR to a sequencer task.

LIS2DUXS12TR is probed at both SA0 addresses and starts at 100 Hz, +/-4 g in
low-power mode. Its official `lis2duxs12_reg` driver (the "S" variant, with
QVar/analog-hub support) is compiled unchanged via `Application/sensor_manager.c`
so existing CubeIDE linked-resource projects also include it.

With `AH_QVAR_EN = 1` the chip connects its QVar buffers to INT1/INT2, so no
interrupt may be routed to those pins. `lis2duxs12_motion.c` always sets
`INT1_ON_RES`, which sends every INT1 interrupt source out on RES instead, and
keeps `PIN_CTRL` at its reset value as in ST's QVar example.

QVar wear detection: the chip has no threshold interrupt on the QVar channel
(only MLC/FSM can classify it on-chip). Until an MLC program exists,
`sensor_manager.c` enables the data-ready pulse on RES while measuring (not
during an ECG session), reads one QVar sample per pulse (100 Hz) and
classifies worn / not worn from the peak-to-peak activity of 1 s windows with
hysteresis. A change of the wear flag (`0x40`) is notified immediately on
`SENSOR_DATA` and `DEVICE_STATUS`. The `QVAR_*` thresholds are uncalibrated
first guesses; read `g_qvarDiag` over SWD (same method as `g_ecgDiag`) worn
and not worn to tune them. If the raw value is stuck or saturated whatever the
contact, look at the hardware first: INT2 is the second input of the
differential QVar buffer and is floating on this board.

No MLC program is enabled by default. Generate a UCF table with ST Unico,
load it with `LIS2DUXS12_MotionLoadUcf()`, install the model-specific output
mapping with `LIS2DUXS12_MotionSetClassRules()`, then call
`LIS2DUXS12_MotionArmMlcInterrupt()` (the MLC interrupt also arrives on
RES/PB2). Until a class rule maps an MLC output to
`LIS2DUXS12_ACTIVITY_FALL`, the firmware does not set a false fall alarm.
An I2C failure on the chip sets `SENSOR_MOTION_BUS_ERROR`; the chip is then
re-probed and reconfigured every 5 s while measuring, which also resets it, so
a loaded UCF must be loaded again after such a recovery.

## Reference sources

The supplied Arduino MAX30208 and MAX86150 sources are preserved unchanged in
their respective `ArduinoReference` folders. They are not compiled because
they depend on Arduino `Wire`, `millis`, and `delay` APIs.

The LIS2DUXS12 register driver (`lis2duxs12_reg.*`) is copied unmodified from
STMicroelectronics' official per-part driver repo
([lis2duxs12-pid](https://github.com/STMicroelectronics/lis2duxs12-pid), as
referenced by the
[lis2duxs12_STdC](https://github.com/STMicroelectronics/STMems_Standard_C_drivers/tree/master/lis2duxs12_STdC)
example collection) and kept platform-independent. `lis2duxs12_platform.*`
provides the STM32 HAL I2C bridge, while `lis2duxs12_motion.*` provides the
application-facing acceleration/QVar/MLC API. A copy of the upstream example
sources (including `lis2duxs12_qvar_read_data.c`, the reference this port's
QVar handling follows) is kept for reference under
`References/STMicroelectronics/lis2duxs12_STdC/` at the repo root - not part
of the build.
