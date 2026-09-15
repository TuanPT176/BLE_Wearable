# Wearable sensor drivers

All sensors share `I2C1` on PB6 (SCL) and PB7 (SDA).

## Runtime roles

- MAX30208: body-temperature acquisition. The active STM32 HAL port is
  `Drivers/max30208.c`; asynchronous scheduling is handled by
  `Application/sensor_manager.c`.
- MAX86150: target device for Red/IR PPG and ECG. The current STM32 driver is
  still optical-only; ECG FIFO/configuration support remains to be added.
- LIS2DUXS12TR: accelerometer and Machine Learning Core (MLC). INT1 is connected
  to STM32WB09 PB2. Runtime I2C processing is deferred from the GPIO ISR to a
  sequencer task.

LIS2DUXS12TR is probed at both SA0 addresses and starts at 100 Hz, +/-4 g in
low-power mode. Its official `lis2duxs12_reg` driver (the "S" variant, with
QVar/analog-hub support) is compiled unchanged via `Application/sensor_manager.c`
so existing CubeIDE linked-resource projects also include it.

No MLC program is enabled by default. Generate a UCF table with ST Unico,
load it with `LIS2DUXS12_MotionLoadUcf()`, install the model-specific output
mapping with `LIS2DUXS12_MotionSetClassRules()`, then call
`LIS2DUXS12_MotionArmMlcInterrupt()`. Until a class rule maps an MLC output to
`LIS2DUXS12_ACTIVITY_FALL`, the firmware does not set a false fall alarm.

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
