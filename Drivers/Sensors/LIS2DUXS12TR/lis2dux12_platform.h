#ifndef LIS2DUX12_PLATFORM_H
#define LIS2DUX12_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32wb0x_hal.h"
#include "lis2dux12_reg.h"

typedef struct
{
  I2C_HandleTypeDef *i2c;
  uint16_t hal_address;
  uint32_t timeout_ms;
} lis2dux12_platform_t;

void LIS2DUX12_PlatformInit(stmdev_ctx_t *context,
                            lis2dux12_platform_t *platform,
                            I2C_HandleTypeDef *i2c,
                            bool address_high);
int32_t LIS2DUX12_PlatformProbe(stmdev_ctx_t *context);

#endif /* LIS2DUX12_PLATFORM_H */
