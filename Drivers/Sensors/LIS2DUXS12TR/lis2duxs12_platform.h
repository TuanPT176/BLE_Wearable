#ifndef LIS2DUXS12_PLATFORM_H
#define LIS2DUXS12_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32wb0x_hal.h"
#include "lis2duxs12_reg.h"

typedef struct
{
  I2C_HandleTypeDef *i2c;
  uint16_t hal_address;
  uint32_t timeout_ms;
} lis2duxs12_platform_t;

void LIS2DUXS12_PlatformInit(stmdev_ctx_t *context,
                             lis2duxs12_platform_t *platform,
                             lis2duxs12_priv_t *priv_data,
                             I2C_HandleTypeDef *i2c,
                             bool address_high);
int32_t LIS2DUXS12_PlatformProbe(stmdev_ctx_t *context);

#endif /* LIS2DUXS12_PLATFORM_H */
