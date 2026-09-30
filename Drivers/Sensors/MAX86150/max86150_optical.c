#include "max86150_optical.h"

#define MAX86150_REG_FIFO_WRITE_PTR       0x04U
#define MAX86150_REG_FIFO_OVERFLOW        0x05U
#define MAX86150_REG_FIFO_READ_PTR        0x06U
#define MAX86150_REG_FIFO_DATA            0x07U
#define MAX86150_REG_FIFO_CONFIG          0x08U
#define MAX86150_REG_FIFO_CONTROL_1       0x09U
#define MAX86150_REG_FIFO_CONTROL_2       0x0AU
#define MAX86150_REG_SYSTEM_CONTROL       0x0DU
#define MAX86150_REG_PPG_CONFIG_1         0x0EU
#define MAX86150_REG_PPG_CONFIG_2         0x0FU
#define MAX86150_REG_LED1_CURRENT         0x11U
#define MAX86150_REG_LED2_CURRENT         0x12U
#define MAX86150_REG_LED_RANGE            0x14U
#define MAX86150_REG_LED_PILOT_PA         0x15U
#define MAX86150_REG_ECG_CONFIG_1         0x3CU
#define MAX86150_REG_ECG_CONFIG_3         0x3EU
#define MAX86150_REG_PART_ID              0xFFU

#define MAX86150_SYSTEM_RESET             0x01U
#define MAX86150_SYSTEM_SHUTDOWN          0x02U
#define MAX86150_SYSTEM_FIFO_ENABLE       0x04U
#define MAX86150_FIFO_IR_RED_SLOTS        0x21U
#define MAX86150_FIFO_ROLLOVER            0x1FU
#define MAX86150_PPG_CONFIG_100HZ_400US   0xD3U
#define MAX86150_PPG_INTEGRATION_DELAY    0x18U
#define MAX86150_RESET_TIMEOUT_MS         100U
#define MAX86150_DEFAULT_TIMEOUT_MS       20U
#define MAX86150_FIFO_SAMPLE_BYTES        6U
#define MAX86150_SAMPLE_MASK              0x0007FFFFUL
/* ECG settings checked against the MAX86150 datasheet (19-8402 Rev 2). */
#define MAX86150_FIFO_ECG_SLOT            0x09U  /* FD1 = 1001 ECG, FD2 = none */
#define MAX86150_ECG_CONFIG_200SPS        0x03U  /* ECG_ADC_CLK 0, OSR 11: 200 sps typ. */
#define MAX86150_ECG_GAIN_IA9_5_PGA8      0x0DU  /* PGA_ECG_GAIN[3:2] 11 = 8, IA_GAIN[1:0] 01 = 9.5 */
#define MAX86150_ECG_SAMPLE_BYTES         3U
#define MAX86150_ECG_SAMPLE_MASK          0x0003FFFFUL
#define MAX86150_ECG_SIGN_BIT             0x00020000UL

static uint16_t MAX86150_HALAddress(const max86150_optical_t *device)
{
  return (uint16_t)device->address << 1U;
}

static max86150_optical_result_t MAX86150_Read(
    max86150_optical_t *device,
    uint8_t reg,
    uint8_t *data,
    uint16_t length)
{
  if ((device == NULL) || (device->i2c == NULL) ||
      (data == NULL) || (length == 0U))
  {
    return MAX86150_OPTICAL_INVALID_ARGUMENT;
  }

  return (HAL_I2C_Mem_Read(device->i2c,
                           MAX86150_HALAddress(device),
                           reg,
                           I2C_MEMADD_SIZE_8BIT,
                           data,
                           length,
                           device->timeout_ms) == HAL_OK)
             ? MAX86150_OPTICAL_OK
             : MAX86150_OPTICAL_BUS_ERROR;
}

static max86150_optical_result_t MAX86150_Write(
    max86150_optical_t *device,
    uint8_t reg,
    uint8_t value)
{
  if ((device == NULL) || (device->i2c == NULL))
  {
    return MAX86150_OPTICAL_INVALID_ARGUMENT;
  }

  return (HAL_I2C_Mem_Write(device->i2c,
                            MAX86150_HALAddress(device),
                            reg,
                            I2C_MEMADD_SIZE_8BIT,
                            &value,
                            1U,
                            device->timeout_ms) == HAL_OK)
             ? MAX86150_OPTICAL_OK
             : MAX86150_OPTICAL_BUS_ERROR;
}

void MAX86150_OpticalBind(max86150_optical_t *device,
                          I2C_HandleTypeDef *i2c)
{
  if (device != NULL)
  {
    device->i2c = i2c;
    device->address = MAX86150_OPTICAL_I2C_ADDRESS;
    device->timeout_ms = MAX86150_DEFAULT_TIMEOUT_MS;
    device->present = false;
  }
}

max86150_optical_result_t MAX86150_OpticalProbe(max86150_optical_t *device)
{
  uint8_t part_id = 0U;

  if ((device == NULL) || (device->i2c == NULL))
  {
    return MAX86150_OPTICAL_INVALID_ARGUMENT;
  }

  device->present = false;
  if (HAL_I2C_IsDeviceReady(device->i2c,
                            MAX86150_HALAddress(device),
                            2U,
                            device->timeout_ms) != HAL_OK)
  {
    return MAX86150_OPTICAL_NOT_PRESENT;
  }

  if (MAX86150_Read(device, MAX86150_REG_PART_ID, &part_id, 1U) !=
      MAX86150_OPTICAL_OK)
  {
    return MAX86150_OPTICAL_BUS_ERROR;
  }

  if (part_id != MAX86150_OPTICAL_EXPECTED_PART_ID)
  {
    return MAX86150_OPTICAL_NOT_PRESENT;
  }

  device->present = true;
  return MAX86150_OPTICAL_OK;
}

static max86150_optical_result_t MAX86150_SoftReset(
    max86150_optical_t *device)
{
  uint8_t control = MAX86150_SYSTEM_RESET;
  uint32_t started_at;

  if (MAX86150_Write(device, MAX86150_REG_SYSTEM_CONTROL,
                     MAX86150_SYSTEM_RESET) != MAX86150_OPTICAL_OK)
  {
    return MAX86150_OPTICAL_BUS_ERROR;
  }

  started_at = HAL_GetTick();
  while ((control & MAX86150_SYSTEM_RESET) != 0U)
  {
    if (MAX86150_Read(device, MAX86150_REG_SYSTEM_CONTROL,
                      &control, 1U) != MAX86150_OPTICAL_OK)
    {
      return MAX86150_OPTICAL_BUS_ERROR;
    }
    if ((HAL_GetTick() - started_at) >= MAX86150_RESET_TIMEOUT_MS)
    {
      return MAX86150_OPTICAL_TIMEOUT;
    }
  }
  return MAX86150_OPTICAL_OK;
}

max86150_optical_result_t MAX86150_OpticalConfigureRedIr(
    max86150_optical_t *device,
    uint8_t red_current,
    uint8_t ir_current)
{
  max86150_optical_result_t result;

  if ((device == NULL) || !device->present)
  {
    return MAX86150_OPTICAL_NOT_PRESENT;
  }

  result = MAX86150_SoftReset(device);
  if (result != MAX86150_OPTICAL_OK)
  {
    return result;
  }

  /* FIFO contains IR then Red only. Slots 3/4 and all ECG registers stay off. */
  if ((MAX86150_Write(device, MAX86150_REG_FIFO_CONFIG,
                      MAX86150_FIFO_ROLLOVER) != MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_FIFO_CONTROL_1,
                      MAX86150_FIFO_IR_RED_SLOTS) != MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_FIFO_CONTROL_2, 0U) !=
       MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_PPG_CONFIG_1,
                      MAX86150_PPG_CONFIG_100HZ_400US) !=
       MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_PPG_CONFIG_2,
                      MAX86150_PPG_INTEGRATION_DELAY) !=
       MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_LED_RANGE, 0U) !=
       MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_LED2_CURRENT, red_current) !=
       MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_LED1_CURRENT, ir_current) !=
       MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_FIFO_WRITE_PTR, 0U) !=
       MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_FIFO_OVERFLOW, 0U) !=
       MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_FIFO_READ_PTR, 0U) !=
       MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_SYSTEM_CONTROL,
                      MAX86150_SYSTEM_FIFO_ENABLE) != MAX86150_OPTICAL_OK))
  {
    return MAX86150_OPTICAL_BUS_ERROR;
  }

  return MAX86150_OPTICAL_OK;
}

max86150_optical_result_t MAX86150_OpticalGetAvailableSamples(
    max86150_optical_t *device,
    uint8_t *count)
{
  uint8_t write_ptr;
  uint8_t read_ptr;

  if (count == NULL)
  {
    return MAX86150_OPTICAL_INVALID_ARGUMENT;
  }
  if ((device == NULL) || !device->present)
  {
    return MAX86150_OPTICAL_NOT_PRESENT;
  }

  if ((MAX86150_Read(device, MAX86150_REG_FIFO_WRITE_PTR, &write_ptr, 1U) !=
       MAX86150_OPTICAL_OK) ||
      (MAX86150_Read(device, MAX86150_REG_FIFO_READ_PTR, &read_ptr, 1U) !=
       MAX86150_OPTICAL_OK))
  {
    return MAX86150_OPTICAL_BUS_ERROR;
  }

  *count = (uint8_t)((write_ptr - read_ptr) & 0x1FU);
  return MAX86150_OPTICAL_OK;
}

max86150_optical_result_t MAX86150_OpticalReadSample(
    max86150_optical_t *device,
    uint32_t *red,
    uint32_t *ir)
{
  uint8_t sample[MAX86150_FIFO_SAMPLE_BYTES];

  if ((red == NULL) || (ir == NULL))
  {
    return MAX86150_OPTICAL_INVALID_ARGUMENT;
  }
  if ((device == NULL) || !device->present)
  {
    return MAX86150_OPTICAL_NOT_PRESENT;
  }
  if (MAX86150_Read(device, MAX86150_REG_FIFO_DATA, sample,
                    sizeof(sample)) != MAX86150_OPTICAL_OK)
  {
    return MAX86150_OPTICAL_BUS_ERROR;
  }

  *ir = ((((uint32_t)sample[0] << 16U) |
          ((uint32_t)sample[1] << 8U) |
          sample[2]) & MAX86150_SAMPLE_MASK);
  *red = ((((uint32_t)sample[3] << 16U) |
           ((uint32_t)sample[4] << 8U) |
           sample[5]) & MAX86150_SAMPLE_MASK);
  return MAX86150_OPTICAL_OK;
}

max86150_optical_result_t MAX86150_OpticalShutdown(
    max86150_optical_t *device,
    bool enable)
{
  uint8_t control;

  if ((device == NULL) || !device->present)
  {
    return MAX86150_OPTICAL_NOT_PRESENT;
  }
  if (MAX86150_Read(device, MAX86150_REG_SYSTEM_CONTROL,
                    &control, 1U) != MAX86150_OPTICAL_OK)
  {
    return MAX86150_OPTICAL_BUS_ERROR;
  }

  if (enable)
  {
    control |= MAX86150_SYSTEM_SHUTDOWN;
  }
  else
  {
    control &= (uint8_t)~MAX86150_SYSTEM_SHUTDOWN;
  }
  return MAX86150_Write(device, MAX86150_REG_SYSTEM_CONTROL, control);
}

max86150_optical_result_t MAX86150_EcgConfigure(max86150_optical_t *device)
{
  max86150_optical_result_t result;

  if ((device == NULL) || !device->present)
  {
    return MAX86150_OPTICAL_NOT_PRESENT;
  }

  /* Reset leaves both LED currents at 0 and the PPG slots unused. */
  result = MAX86150_SoftReset(device);
  if (result != MAX86150_OPTICAL_OK)
  {
    return result;
  }

  if ((MAX86150_Write(device, MAX86150_REG_FIFO_CONFIG,
                      MAX86150_FIFO_ROLLOVER) != MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_FIFO_CONTROL_1,
                      MAX86150_FIFO_ECG_SLOT) != MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_FIFO_CONTROL_2, 0U) !=
       MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_ECG_CONFIG_1,
                      MAX86150_ECG_CONFIG_200SPS) != MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_ECG_CONFIG_3,
                      MAX86150_ECG_GAIN_IA9_5_PGA8) != MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_FIFO_WRITE_PTR, 0U) !=
       MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_FIFO_OVERFLOW, 0U) !=
       MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_FIFO_READ_PTR, 0U) !=
       MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_SYSTEM_CONTROL,
                      MAX86150_SYSTEM_FIFO_ENABLE) != MAX86150_OPTICAL_OK))
  {
    return MAX86150_OPTICAL_BUS_ERROR;
  }

  return MAX86150_OPTICAL_OK;
}

max86150_optical_result_t MAX86150_EcgReadSamples(
    max86150_optical_t *device,
    int32_t *samples,
    uint8_t max_samples,
    uint8_t *count,
    bool *overflowed)
{
  /* FIFO_WR_PTR, OVF_COUNTER, FIFO_RD_PTR are consecutive registers. */
  uint8_t pointers[3];
  uint8_t raw[MAX86150_ECG_FIFO_DEPTH * MAX86150_ECG_SAMPLE_BYTES];
  uint8_t available;
  uint8_t i;
  uint32_t value;

  if ((samples == NULL) || (count == NULL) || (overflowed == NULL))
  {
    return MAX86150_OPTICAL_INVALID_ARGUMENT;
  }
  *count = 0U;
  *overflowed = false;
  if ((device == NULL) || !device->present)
  {
    return MAX86150_OPTICAL_NOT_PRESENT;
  }

  if (MAX86150_Read(device, MAX86150_REG_FIFO_WRITE_PTR, pointers,
                    sizeof(pointers)) != MAX86150_OPTICAL_OK)
  {
    return MAX86150_OPTICAL_BUS_ERROR;
  }

  /* A full, rolled-over FIFO has WR_PTR == RD_PTR like an empty one; the
   * overflow counter tells them apart. */
  if (pointers[1] != 0U)
  {
    available = (uint8_t)MAX86150_ECG_FIFO_DEPTH;
    *overflowed = true;
  }
  else
  {
    available = (uint8_t)((pointers[0] - pointers[2]) & 0x1FU);
  }
  if (available > max_samples)
  {
    available = max_samples;
  }
  if (available > MAX86150_ECG_FIFO_DEPTH)
  {
    available = (uint8_t)MAX86150_ECG_FIFO_DEPTH;
  }
  if (available == 0U)
  {
    return MAX86150_OPTICAL_OK;
  }

  if (MAX86150_Read(device, MAX86150_REG_FIFO_DATA, raw,
                    (uint16_t)((uint16_t)available * MAX86150_ECG_SAMPLE_BYTES)) !=
      MAX86150_OPTICAL_OK)
  {
    return MAX86150_OPTICAL_BUS_ERROR;
  }

  for (i = 0U; i < available; i++)
  {
    value = (((uint32_t)raw[i * 3U] << 16U) |
             ((uint32_t)raw[(i * 3U) + 1U] << 8U) |
             raw[(i * 3U) + 2U]) & MAX86150_ECG_SAMPLE_MASK;
    if ((value & MAX86150_ECG_SIGN_BIT) != 0U)
    {
      value |= ~MAX86150_ECG_SAMPLE_MASK;
    }
    samples[i] = (int32_t)value;
  }
  *count = available;
  return MAX86150_OPTICAL_OK;
}

max86150_optical_result_t MAX86150_LedsOff(max86150_optical_t *device)
{
  if ((device == NULL) || !device->present)
  {
    return MAX86150_OPTICAL_NOT_PRESENT;
  }

  if ((MAX86150_Write(device, MAX86150_REG_LED1_CURRENT, 0U) !=
       MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_LED2_CURRENT, 0U) !=
       MAX86150_OPTICAL_OK) ||
      (MAX86150_Write(device, MAX86150_REG_LED_PILOT_PA, 0U) !=
       MAX86150_OPTICAL_OK))
  {
    return MAX86150_OPTICAL_BUS_ERROR;
  }
  return MAX86150_OPTICAL_OK;
}

max86150_optical_result_t MAX86150_ReadRegister(max86150_optical_t *device,
                                                uint8_t reg,
                                                uint8_t *value)
{
  if (value == NULL)
  {
    return MAX86150_OPTICAL_INVALID_ARGUMENT;
  }
  if ((device == NULL) || !device->present)
  {
    return MAX86150_OPTICAL_NOT_PRESENT;
  }
  return MAX86150_Read(device, reg, value, 1U);
}
