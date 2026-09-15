# STMicroelectronics reference sources

Reference-only copies of ST's official driver/example repos. **Not part of the
STM32CubeIDE build** (not referenced as a linked resource by the project) -
kept here purely so the exact upstream source is available locally when
porting/debugging the in-tree drivers under `Drivers/Sensors/`.

## lis2duxs12_STdC/

Pulled from STMicroelectronics' GitHub:

- `driver/` - [lis2duxs12-pid](https://github.com/STMicroelectronics/lis2duxs12-pid)
  (`lis2duxs12_reg.c/h`), the authoritative register driver referenced as a
  git submodule by the STdC example collection below. This is the exact
  source copied into `Drivers/Sensors/LIS2DUXS12TR/lis2duxs12_reg.c/h`.
- `examples/` - the `lis2duxs12_STdC/examples` folder from
  [STMems_Standard_C_drivers](https://github.com/STMicroelectronics/STMems_Standard_C_drivers/tree/master/lis2duxs12_STdC),
  ST's official usage examples for this part (QVar, MLC, FIFO, tap, etc. on
  various ST eval boards - not this project's hardware, but useful as the
  canonical reference for correct init/read sequences). In particular
  `lis2duxs12_qvar_read_data.c` is what `Drivers/Sensors/LIS2DUXS12TR/lis2duxs12_motion.c`'s
  QVar handling follows.

Pulled 2026-09-14. Re-pull from the same URLs if you need a newer revision.
