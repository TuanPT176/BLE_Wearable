#include "max30208.h"

#include "main.h"

#define MAX30208_REG_STATUS               0x00U
#define MAX30208_REG_FIFO_DATA_COUNT      0x07U
#define MAX30208_REG_FIFO_DATA            0x08U
#define MAX30208_REG_FIFO_CONFIG_2        0x0AU
#define MAX30208_REG_TEMP_SENSOR_SETUP    0x14U
#define MAX30208_REG_PART_IDENTIFIER      0xFFU

#define MAX30208_PART_IDENTIFIER          0x30U
#define MAX30208_FIFO_FLUSH                0x10U
#define MAX30208_TEMP_SETUP_CONVERT        0xC1U
#define MAX30208_I2C_TIMEOUT_MS              20U

static uint8_t device_address;
static bool device_present;

static uint16_t MAX30208_HALAddress(void)
{
  return (uint16_t)device_address << 1U;
}

static bool MAX30208_ReadRegister(uint8_t register_address, uint8_t *value)
{
  return HAL_I2C_Mem_Read(&hi2c1,
                          MAX30208_HALAddress(),
                          register_address,
                          I2C_MEMADD_SIZE_8BIT,
                          value,
                          1U,
                          MAX30208_I2C_TIMEOUT_MS) == HAL_OK;
}

static bool MAX30208_WriteRegister(uint8_t register_address, uint8_t value)
{
  return HAL_I2C_Mem_Write(&hi2c1,
                           MAX30208_HALAddress(),
                           register_address,
                           I2C_MEMADD_SIZE_8BIT,
                           &value,
                           1U,
                           MAX30208_I2C_TIMEOUT_MS) == HAL_OK;
}

temp_sensor_result_t TempSensor_Init(void)
{
  uint8_t address;
  uint8_t part_identifier;

  device_present = false;
  device_address = MAX30208_I2C_ADDRESS_MIN;
  if (hi2c1.Instance != I2C1)
  {
    return TEMP_SENSOR_RESULT_NOT_PRESENT;
  }

  for (address = MAX30208_I2C_ADDRESS_MIN;
       address <= MAX30208_I2C_ADDRESS_MAX;
       address++)
  {
    device_address = address;
    if ((HAL_I2C_IsDeviceReady(&hi2c1,
                               MAX30208_HALAddress(),
                               2U,
                               MAX30208_I2C_TIMEOUT_MS) == HAL_OK) &&
        MAX30208_ReadRegister(MAX30208_REG_PART_IDENTIFIER, &part_identifier) &&
        (part_identifier == MAX30208_PART_IDENTIFIER))
    {
      device_present = true;
      break;
    }
  }

  if (!device_present)
  {
    return TEMP_SENSOR_RESULT_NOT_PRESENT;
  }

  if (!MAX30208_WriteRegister(MAX30208_REG_FIFO_CONFIG_2,
                              MAX30208_FIFO_FLUSH))
  {
    return TEMP_SENSOR_RESULT_BUS_ERROR;
  }
  return TEMP_SENSOR_RESULT_OK;
}

temp_sensor_result_t TempSensor_TriggerOneShot(void)
{
  if (!device_present)
  {
    return TEMP_SENSOR_RESULT_NOT_PRESENT;
  }

  /* Bits 7:6 are reserved and must be written as 1. CONVERT_T is bit 0. */
  if (!MAX30208_WriteRegister(MAX30208_REG_TEMP_SENSOR_SETUP,
                              MAX30208_TEMP_SETUP_CONVERT))
  {
    return TEMP_SENSOR_RESULT_BUS_ERROR;
  }
  return TEMP_SENSOR_RESULT_OK;
}

temp_sensor_result_t TempSensor_TryReadCentiC(int16_t *temperature_centi_c)
{
  uint8_t fifo_count;
  uint8_t raw_bytes[2];
  int16_t raw_temperature;
  int32_t rounded_centi_c;

  if ((!device_present) || (temperature_centi_c == NULL))
  {
    return (temperature_centi_c == NULL) ? TEMP_SENSOR_RESULT_INVALID_ARGUMENT :
                                           TEMP_SENSOR_RESULT_NOT_PRESENT;
  }

  if (!MAX30208_ReadRegister(MAX30208_REG_FIFO_DATA_COUNT, &fifo_count))
  {
    return TEMP_SENSOR_RESULT_BUS_ERROR;
  }
  if ((fifo_count & 0x3FU) == 0U)
  {
    return TEMP_SENSOR_RESULT_NOT_READY;
  }

  if (HAL_I2C_Mem_Read(&hi2c1,
                       MAX30208_HALAddress(),
                       MAX30208_REG_FIFO_DATA,
                       I2C_MEMADD_SIZE_8BIT,
                       raw_bytes,
                       sizeof(raw_bytes),
                       MAX30208_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return TEMP_SENSOR_RESULT_BUS_ERROR;
  }

  raw_temperature = (int16_t)(((uint16_t)raw_bytes[0] << 8U) |
                              raw_bytes[1]);

  /* One raw LSB is 0.005 degC, or one half of a centi-degree. */
  rounded_centi_c = raw_temperature;
  rounded_centi_c += (rounded_centi_c >= 0) ? 1 : -1;
  *temperature_centi_c = (int16_t)(rounded_centi_c / 2);
  return TEMP_SENSOR_RESULT_OK;
}

temp_sensor_result_t TempSensor_Sleep(void)
{
  /* MAX30208 automatically returns to its 0.5 uA standby state. */
  return device_present ? TEMP_SENSOR_RESULT_OK : TEMP_SENSOR_RESULT_NOT_PRESENT;
}

bool TempSensor_IsPresent(void)
{
  return device_present;
}

uint8_t TempSensor_GetAddress(void)
{
  return device_address;
}
