#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdbool.h>

#include "wearable_data.h"

typedef enum
{
  SENSOR_TEMPERATURE_NOT_PRESENT = 0,
  SENSOR_TEMPERATURE_IDLE,
  SENSOR_TEMPERATURE_CONVERTING,
  SENSOR_TEMPERATURE_VALID,
  SENSOR_TEMPERATURE_TIMEOUT,
  SENSOR_TEMPERATURE_BUS_ERROR
} sensor_temperature_status_t;

typedef enum
{
  SENSOR_MOTION_NOT_PRESENT = 0,
  SENSOR_MOTION_ACCELEROMETER_READY,
  SENSOR_MOTION_CLASSIFIER_READY,
  SENSOR_MOTION_BUS_ERROR
} sensor_motion_status_t;

bool SensorManager_Init(void);
bool SensorManager_Start(void);
bool SensorManager_Stop(void);
bool SensorManager_GetLatestData(wearable_sensor_data_t *data);
void SensorManager_Process(void);
void SensorManager_ProcessAsync(void);
bool SensorManager_GetAsyncDelayMs(uint32_t *delay_ms);
sensor_temperature_status_t SensorManager_GetTemperatureStatus(void);
sensor_motion_status_t SensorManager_GetMotionStatus(void);
void SensorManager_ProcessMotionInterrupt(void);
void SensorManager_ProcessMotionTimeout(void);
bool SensorManager_GetMotionDelayMs(uint32_t *delay_ms);
void SensorManager_SetPowerState(uint8_t power_state);
void SensorManager_SetFlag(uint8_t flag, bool enabled);

#endif /* SENSOR_MANAGER_H */
