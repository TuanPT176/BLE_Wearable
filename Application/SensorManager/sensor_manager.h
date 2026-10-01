#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdbool.h>

#include "../wearable_data.h"

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

typedef enum
{
  SENSOR_OPTICAL_NOT_PRESENT = 0,
  SENSOR_OPTICAL_IDLE,
  SENSOR_OPTICAL_ACTIVE,      /* Red/IR PPG running (HR/SpO2) */
  SENSOR_OPTICAL_ECG_ACTIVE   /* same chip switched to ECG; HR/SpO2 frozen */
} sensor_optical_status_t;

/* QVar wear-detector observations. The board has no UART, so read this over
 * SWD like g_ecgDiag and tune the QVAR_* thresholds in sensor_manager.c. */
typedef struct
{
  uint32_t interrupts;         /* RES/PB2 interrupts served while the stream is on */
  uint32_t samples;            /* QVar samples read from the stream */
  uint32_t windows;            /* completed detector windows */
  uint32_t bus_errors;         /* LIS2DUXS12 read/config failures */
  uint32_t state_changes;      /* wear flag toggles */
  uint16_t last_peak_to_peak;  /* last window, raw LSB */
  int16_t last_mean;
  int16_t last_min;
  int16_t last_max;
  uint8_t worn;
  uint8_t stream_active;
} sensor_qvar_diag_t;

extern volatile sensor_qvar_diag_t g_qvarDiag;

bool SensorManager_Init(void);
bool SensorManager_Start(void);
bool SensorManager_Stop(void);
bool SensorManager_GetLatestData(wearable_sensor_data_t *data);
void SensorManager_Process(void);
void SensorManager_ProcessAsync(void);
bool SensorManager_GetAsyncDelayMs(uint32_t *delay_ms);
sensor_temperature_status_t SensorManager_GetTemperatureStatus(void);
/* Outcome of the last finished conversion (VALID, TIMEOUT, BUS_ERROR), or
 * NOT_PRESENT / IDLE. Unlike the status above it does not pass through
 * CONVERTING every second, so it is the one to report as an error code. */
sensor_temperature_status_t SensorManager_GetTemperatureResult(void);
sensor_motion_status_t SensorManager_GetMotionStatus(void);
sensor_optical_status_t SensorManager_GetOpticalStatus(void);
/* PB2 (LIS2DUXS12 RES) interrupt, task context. Returns true when a flag in
 * the sensor data changed (wear state, fall candidate) and should be
 * reported right away. */
bool SensorManager_ProcessMotionInterrupt(void);
void SensorManager_ProcessMotionTimeout(void);
bool SensorManager_GetMotionDelayMs(uint32_t *delay_ms);
void SensorManager_SetPowerState(uint8_t power_state);
void SensorManager_SetFlag(uint8_t flag, bool enabled);

/* ECG on the MAX86150. Call after SensorManager_Start(); switches the chip
 * from PPG to ECG and sets WEARABLE_FLAG_ECG_ACTIVE. SensorManager_Start()
 * switches back to PPG, SensorManager_Stop() ends the session. */
bool SensorManager_StartEcg(void);
bool SensorManager_IsEcgActive(void);
/* Drains waiting ECG samples (200 sps, 18-bit scaled to int16 by >> 2).
 * Must run at least every 160 ms while ECG is active. */
uint8_t SensorManager_ReadEcgSamples(int16_t *samples, uint8_t max_samples);

#endif /* SENSOR_MANAGER_H */
