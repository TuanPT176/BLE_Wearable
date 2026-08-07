#ifndef MAX86150_OPTICAL_H
#define MAX86150_OPTICAL_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32wb0x_hal.h"

#define MAX86150_OPTICAL_I2C_ADDRESS       0x5EU
#define MAX86150_OPTICAL_EXPECTED_PART_ID  0x1EU

typedef enum
{
  MAX86150_OPTICAL_OK = 0,
  MAX86150_OPTICAL_NOT_PRESENT,
  MAX86150_OPTICAL_BUS_ERROR,
  MAX86150_OPTICAL_TIMEOUT,
  MAX86150_OPTICAL_INVALID_ARGUMENT
} max86150_optical_result_t;

typedef struct
{
  I2C_HandleTypeDef *i2c;
  uint8_t address;
  uint32_t timeout_ms;
  bool present;
} max86150_optical_t;

/*
 * This STM32 port intentionally exposes only the Red/IR optical path.
 * ECG acquisition belongs to ST1VAFE3BX in this product.
 */
void MAX86150_OpticalBind(max86150_optical_t *device,
                          I2C_HandleTypeDef *i2c);
max86150_optical_result_t MAX86150_OpticalProbe(max86150_optical_t *device);
max86150_optical_result_t MAX86150_OpticalConfigureRedIr(
    max86150_optical_t *device,
    uint8_t red_current,
    uint8_t ir_current);
max86150_optical_result_t MAX86150_OpticalReadSample(
    max86150_optical_t *device,
    uint32_t *red,
    uint32_t *ir);
max86150_optical_result_t MAX86150_OpticalShutdown(
    max86150_optical_t *device,
    bool enable);

#endif /* MAX86150_OPTICAL_H */
