#include "st1vafe3bx_platform.h"

#define ST1VAFE3BX_PLATFORM_TIMEOUT_MS  20U

static int32_t ST1VAFE3BX_PlatformWrite(void *handle,
                                       uint8_t reg,
                                       const uint8_t *data,
                                       uint16_t length)
{
  st1vafe3bx_platform_t *platform = (st1vafe3bx_platform_t *)handle;

  if ((platform == NULL) || (platform->i2c == NULL) ||
      (data == NULL) || (length == 0U))
  {
    return -1;
  }

  return (HAL_I2C_Mem_Write(platform->i2c,
                            platform->hal_address,
                            reg,
                            I2C_MEMADD_SIZE_8BIT,
                            (uint8_t *)data,
                            length,
                            platform->timeout_ms) == HAL_OK) ? 0 : -1;
}

static int32_t ST1VAFE3BX_PlatformRead(void *handle,
                                      uint8_t reg,
                                      uint8_t *data,
                                      uint16_t length)
{
  st1vafe3bx_platform_t *platform = (st1vafe3bx_platform_t *)handle;

  if ((platform == NULL) || (platform->i2c == NULL) ||
      (data == NULL) || (length == 0U))
  {
    return -1;
  }

  return (HAL_I2C_Mem_Read(platform->i2c,
                           platform->hal_address,
                           reg,
                           I2C_MEMADD_SIZE_8BIT,
                           data,
                           length,
                           platform->timeout_ms) == HAL_OK) ? 0 : -1;
}

static void ST1VAFE3BX_PlatformDelay(uint32_t milliseconds)
{
  HAL_Delay(milliseconds);
}

void ST1VAFE3BX_PlatformInit(stmdev_ctx_t *context,
                            st1vafe3bx_platform_t *platform,
                            I2C_HandleTypeDef *i2c,
                            bool address_high)
{
  if ((context == NULL) || (platform == NULL))
  {
    return;
  }

  platform->i2c = i2c;
  /* ST constants include the R/W bit; HAL expects the even write address. */
  platform->hal_address = (uint16_t)(address_high
                              ? ST1VAFE3BX_I2C_ADD_H
                              : ST1VAFE3BX_I2C_ADD_L) & 0xFEU;
  platform->timeout_ms = ST1VAFE3BX_PLATFORM_TIMEOUT_MS;

  context->write_reg = ST1VAFE3BX_PlatformWrite;
  context->read_reg = ST1VAFE3BX_PlatformRead;
  context->mdelay = ST1VAFE3BX_PlatformDelay;
  context->handle = platform;
  context->priv_data = NULL;
}

int32_t ST1VAFE3BX_PlatformProbe(stmdev_ctx_t *context)
{
  uint8_t who_am_i = 0U;
  int32_t result;

  if (context == NULL)
  {
    return -1;
  }

  result = st1vafe3bx_device_id_get(context, &who_am_i);
  if (result != 0)
  {
    return result;
  }

  return (who_am_i == ST1VAFE3BX_ID) ? 0 : -1;
}
