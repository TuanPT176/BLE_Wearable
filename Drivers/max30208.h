#ifndef MAX30208_H
#define MAX30208_H

#include <stdbool.h>
#include <stdint.h>

/*
 * STM32 HAL port used by SensorManager. The supplied Arduino library is kept
 * unchanged under Drivers/Sensors/MAX30208/ArduinoReference for comparison.
 */

/* GPIO1/GPIO0 select one of the four 7-bit addresses 0x50..0x53. */
#define MAX30208_I2C_ADDRESS_MIN       0x50U
#define MAX30208_I2C_ADDRESS_MAX       0x53U

typedef enum
{
  TEMP_SENSOR_RESULT_OK = 0,
  TEMP_SENSOR_RESULT_NOT_READY,
  TEMP_SENSOR_RESULT_NOT_PRESENT,
  TEMP_SENSOR_RESULT_BUS_ERROR,
  TEMP_SENSOR_RESULT_INVALID_ARGUMENT
} temp_sensor_result_t;

temp_sensor_result_t TempSensor_Init(void);
temp_sensor_result_t TempSensor_TriggerOneShot(void);
temp_sensor_result_t TempSensor_TryReadCentiC(int16_t *temperature_centi_c);
temp_sensor_result_t TempSensor_Sleep(void);
bool TempSensor_IsPresent(void);
uint8_t TempSensor_GetAddress(void);

#endif /* MAX30208_H */
