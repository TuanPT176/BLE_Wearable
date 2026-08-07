#ifndef ST1VAFE3BX_PLATFORM_H
#define ST1VAFE3BX_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32wb0x_hal.h"
#include "st1vafe3bx_reg.h"

typedef struct
{
  I2C_HandleTypeDef *i2c;
  uint16_t hal_address;
  uint32_t timeout_ms;
} st1vafe3bx_platform_t;

/* Bind the official ST register driver to an STM32 HAL I2C instance. */
void ST1VAFE3BX_PlatformInit(stmdev_ctx_t *context,
                            st1vafe3bx_platform_t *platform,
                            I2C_HandleTypeDef *i2c,
                            bool address_high);

/* Read WHO_AM_I only. No sensor configuration or acquisition is started. */
int32_t ST1VAFE3BX_PlatformProbe(stmdev_ctx_t *context);

#endif /* ST1VAFE3BX_PLATFORM_H */
