# Wearable sensor drivers

All sensors share `I2C1` on PB6 (SCL) and PB7 (SDA).

## Runtime roles

- MAX30208: body-temperature acquisition. The active STM32 HAL port is
  `Drivers/max30208.c`; its asynchronous scheduling is handled by
  `Application/sensor_manager.c`.
- MAX86150: Red/IR optical acquisition only. The STM32 port never assigns an
  ECG FIFO slot and never writes an ECG configuration register.
- ST1VAFE3BX: accelerometer and embedded MLC/FSM motion classifier. Its INT pin
  is connected to STM32WB09 PB2. Runtime I2C reads are deferred from the GPIO
  ISR to a sequencer task. ECG/vAFE remains a hardware capability, but this
  motion layer does not enable or process the ECG path.

MAX86150 is deliberately not called during startup. ST1VAFE3BX is probed as an
optional sensor at both SA0 addresses and starts as a 50 Hz, +/-4 g
accelerometer when present. No MLC/FSM program is enabled until a generated UCF
table is supplied and explicitly armed.

The motion API also accepts a classification rule table that maps future MLC
or FSM output bytes to `NOT_WORN`, `SLEEPING`, `NORMAL`, or `FALL`. Until those
model-specific output codes are provided, the semantic result remains
`UNKNOWN`, so no false fall alarm can be generated.

Before the MLC/FSM model is available, the inertial engine provides a staged
fall-candidate path: approximately 60 ms below 312 mg, an impact/wake event
within 1.2 seconds, then a gravity-range confirmation after 500 ms. A confirmed
candidate sets `WEARABLE_FLAG_FALL_CANDIDATE`; it does not yet trigger LoRa or
the emergency state.

## Reference sources

The supplied Arduino MAX30208 and MAX86150 sources are preserved unchanged in
their respective `ArduinoReference` folders. They are not compiled because
they depend on Arduino `Wire`, `millis`, and `delay` APIs. The ST register
driver is platform-independent and is compiled unchanged.
