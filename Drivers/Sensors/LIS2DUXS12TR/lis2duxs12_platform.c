#include "lis2duxs12_platform.h"

#include <stddef.h>

#define LIS2DUXS12_PLATFORM_TIMEOUT_MS  20U

static int32_t LIS2DUXS12_PlatformWrite(void *handle, uint8_t reg,
                                        const uint8_t *data, uint16_t length)
{
  lis2duxs12_platform_t *platform = (lis2duxs12_platform_t *)handle;
  if ((platform == NULL) || (platform->i2c == NULL) ||
      (data == NULL) || (length == 0U))
  {
    return -1;
  }
  return (HAL_I2C_Mem_Write(platform->i2c, platform->hal_address, reg,
                            I2C_MEMADD_SIZE_8BIT, (uint8_t *)data, length,
                            platform->timeout_ms) == HAL_OK) ? 0 : -1;
}

static int32_t LIS2DUXS12_PlatformRead(void *handle, uint8_t reg,
                                       uint8_t *data, uint16_t length)
{
  lis2duxs12_platform_t *platform = (lis2duxs12_platform_t *)handle;
  if ((platform == NULL) || (platform->i2c == NULL) ||
      (data == NULL) || (length == 0U))
  {
    return -1;
  }
  return (HAL_I2C_Mem_Read(platform->i2c, platform->hal_address, reg,
                           I2C_MEMADD_SIZE_8BIT, data, length,
                           platform->timeout_ms) == HAL_OK) ? 0 : -1;
}

static void LIS2DUXS12_PlatformDelay(uint32_t milliseconds)
{
  HAL_Delay(milliseconds);
}

void LIS2DUXS12_PlatformInit(stmdev_ctx_t *context,
                             lis2duxs12_platform_t *platform,
                             lis2duxs12_priv_t *priv_data,
                             I2C_HandleTypeDef *i2c,
                             bool address_high)
{
  if ((context == NULL) || (platform == NULL))
  {
    return;
  }
  platform->i2c = i2c;
  platform->hal_address = (uint16_t)(address_high ? LIS2DUXS12_I2C_ADD_H
                                                   : LIS2DUXS12_I2C_ADD_L) & 0xFEU;
  platform->timeout_ms = LIS2DUXS12_PLATFORM_TIMEOUT_MS;
  context->write_reg = LIS2DUXS12_PlatformWrite;
  context->read_reg = LIS2DUXS12_PlatformRead;
  context->mdelay = LIS2DUXS12_PlatformDelay;
  context->handle = platform;
  context->priv_data = priv_data;
}

int32_t LIS2DUXS12_PlatformProbe(stmdev_ctx_t *context)
{
  uint8_t who_am_i = 0U;
  if (context == NULL)
  {
    return -1;
  }
  /* Per ST's own example (lis2duxs12_qvar_read_data.c): the part can power
   * up in deep power-down, where WHO_AM_I may not answer correctly until
   * this is cleared. Ignore the return code here - a wrong I2C address just
   * fails this (and the WHO_AM_I check right after) harmlessly. */
  (void)lis2duxs12_exit_deep_power_down(context);
  if (lis2duxs12_device_id_get(context, &who_am_i) != 0)
  {
    return -1;
  }
  return (who_am_i == LIS2DUXS12_ID) ? 0 : -1;
}
