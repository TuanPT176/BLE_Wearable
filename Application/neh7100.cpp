#include "neh7100.h"

#include "stm32wb0x_hal.h"

extern "C" I2C_HandleTypeDef hi2c1;

static const uint32_t NEH7100_I2C_TIMEOUT_MS = 20U;
static uint8_t neh7100_registers[NEH7100_REGISTER_COUNT];
static bool neh7100_present;

static bool NEH7100_ReadRegister(uint8_t register_address, uint8_t *value)
{
  return HAL_I2C_Mem_Read(&hi2c1,
                          static_cast<uint16_t>(NEH7100_I2C_ADDRESS << 1U),
                          register_address,
                          I2C_MEMADD_SIZE_8BIT,
                          value,
                          1U,
                          NEH7100_I2C_TIMEOUT_MS) == HAL_OK;
}

static bool NEH7100_WriteRegister(uint8_t register_address, uint8_t value)
{
  return HAL_I2C_Mem_Write(&hi2c1,
                           static_cast<uint16_t>(NEH7100_I2C_ADDRESS << 1U),
                           register_address,
                           I2C_MEMADD_SIZE_8BIT,
                           &value,
                           1U,
                           NEH7100_I2C_TIMEOUT_MS) == HAL_OK;
}

bool NEH7100_Init(void)
{
  if (hi2c1.Instance != I2C1)
  {
    neh7100_present = false;
    return false;
  }

  neh7100_present = (HAL_I2C_IsDeviceReady(
                       &hi2c1,
                       static_cast<uint16_t>(NEH7100_I2C_ADDRESS << 1U),
                       2U,
                       NEH7100_I2C_TIMEOUT_MS) == HAL_OK);
  return neh7100_present;
}

bool NEH7100_ReadAll(uint8_t registers[NEH7100_REGISTER_COUNT])
{
  uint8_t index;

  if ((!neh7100_present) || (registers == nullptr))
  {
    return false;
  }

  for (index = 0U; index < NEH7100_REGISTER_COUNT; index++)
  {
    if (!NEH7100_ReadRegister(index, &registers[index]))
    {
      neh7100_present = false;
      return false;
    }
    neh7100_registers[index] = registers[index];
  }
  return true;
}

bool NEH7100_EnsureConfig(void)
{
  static const uint8_t config_registers[] = {0x00U, 0x01U, 0x04U, 0x05U};
  static const uint8_t config_values[] = {
    NEH7100_REG00_EXPECTED,
    NEH7100_REG01_EXPECTED,
    NEH7100_REG04_EXPECTED,
    NEH7100_REG05_EXPECTED
  };
  uint8_t index;

  if (!NEH7100_ReadAll(neh7100_registers))
  {
    return false;
  }

  for (index = 0U; index < sizeof(config_registers); index++)
  {
    const uint8_t address = config_registers[index];
    if ((neh7100_registers[address] != config_values[index]) &&
        (!NEH7100_WriteRegister(address, config_values[index])))
    {
      return false;
    }
    if (neh7100_registers[address] != config_values[index])
    {
      HAL_Delay(10U);
      neh7100_registers[address] = config_values[index];
    }
  }
  return true;
}

bool NEH7100_SetFrequency(neh7100_frequency_t maximum,
                          neh7100_frequency_t minimum)
{
  if ((maximum > NEH7100_FREQ_1M) || (minimum > NEH7100_FREQ_1M))
  {
    return false;
  }
  return NEH7100_WriteRegister(
    0x03U,
    static_cast<uint8_t>((static_cast<uint8_t>(maximum) << 4U) |
                         static_cast<uint8_t>(minimum)));
}

bool NEH7100_UpdateFrequency(uint16_t current_uA_x10)
{
  if (current_uA_x10 < 20U)
  {
    return NEH7100_SetFrequency(NEH7100_FREQ_64K, NEH7100_FREQ_32K);
  }
  if (current_uA_x10 < 100U)
  {
    return NEH7100_SetFrequency(NEH7100_FREQ_128K, NEH7100_FREQ_32K);
  }
  if (current_uA_x10 < 250U)
  {
    return NEH7100_SetFrequency(NEH7100_FREQ_512K, NEH7100_FREQ_64K);
  }
  return NEH7100_SetFrequency(NEH7100_FREQ_1M, NEH7100_FREQ_128K);
}

uint16_t NEH7100_GetCurrent_uA_x10(void)
{
  const uint8_t current_range = neh7100_registers[0x09U] & 0x03U;
  const uint8_t raw_value = neh7100_registers[0x0AU];
  uint32_t current = 0U;

  switch (current_range)
  {
    case 0U: current = (static_cast<uint32_t>(raw_value) * 706U) / 1000U; break;
    case 1U: current = (static_cast<uint32_t>(raw_value) * 478U) / 100U; break;
    case 2U: current = (static_cast<uint32_t>(raw_value) * 471U) / 10U; break;
    case 3U: current = static_cast<uint32_t>(raw_value) * 675U; break;
    default: break;
  }
  return static_cast<uint16_t>(current);
}

bool NEH7100_IsPresent(void)
{
  return neh7100_present;
}
