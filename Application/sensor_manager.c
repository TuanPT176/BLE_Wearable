#include "sensor_manager.h"

#include <stddef.h>

#include "main.h"
#include "NEH7100/neh7100.h"
#include "../Drivers/max30208.h"
#include "../Drivers/supercap_monitor.h"
#include "../Drivers/Sensors/LIS2DUXS12TR/lis2dux12_motion.h"

/*
 * Driver implementation bundle
 * ----------------------------
 * This CubeIDE project keeps application sources as linked resources. Some
 * existing Eclipse workspaces cache the old .project description and silently
 * omit newly-added linked .c files from objects.list. SensorManager is an
 * original, stable build resource, so the mandatory sensor implementations are
 * compiled in this translation unit. Do not also add these driver .c files as
 * standalone build resources.
 */
#include "../Drivers/max30208.c"
#include "../Drivers/Sensors/LIS2DUXS12TR/lis2dux12_reg.c"
#include "../Drivers/Sensors/LIS2DUXS12TR/lis2dux12_platform.c"
#include "../Drivers/Sensors/LIS2DUXS12TR/lis2dux12_motion.c"

#define TEMPERATURE_FIRST_POLL_DELAY_MS   20U
#define TEMPERATURE_RETRY_DELAY_MS         5U
#define TEMPERATURE_CONVERSION_TIMEOUT_MS 60U
static wearable_sensor_data_t latest_data;
static bool initialized;
static bool running;
static sensor_temperature_status_t temperature_status;
static sensor_motion_status_t motion_status;
static uint32_t temperature_conversion_elapsed_ms;
static uint32_t temperature_async_delay_ms;
static uint32_t motion_delay_ms;

static void SensorManager_StartTemperatureConversion(void)
{
  if ((!running) ||
      ((temperature_status != SENSOR_TEMPERATURE_IDLE) &&
       (temperature_status != SENSOR_TEMPERATURE_VALID) &&
       (temperature_status != SENSOR_TEMPERATURE_TIMEOUT) &&
       (temperature_status != SENSOR_TEMPERATURE_BUS_ERROR)))
  {
    return;
  }

  if (TempSensor_TriggerOneShot() == TEMP_SENSOR_RESULT_OK)
  {
    temperature_conversion_elapsed_ms = 0U;
    temperature_async_delay_ms = TEMPERATURE_FIRST_POLL_DELAY_MS;
    temperature_status = SENSOR_TEMPERATURE_CONVERTING;
  }
  else
  {
    temperature_async_delay_ms = 0U;
    temperature_status = SENSOR_TEMPERATURE_BUS_ERROR;
  }
}

bool SensorManager_Init(void)
{
  lis2dux12_acceleration_t acceleration;
  latest_data.heart_rate_bpm = 72U;
  latest_data.spo2_percent = 98U;
  latest_data.temperature_centi_c = WEARABLE_TEMPERATURE_INVALID_CENTI_C;
  latest_data.supercap_mv = 0U;
  latest_data.power_state = 1U;
  latest_data.flags = 0U;
  latest_data.accel_x = 0;
  latest_data.accel_y = 0;
  latest_data.accel_z = 0;
  running = false;
  initialized = SupercapMonitor_Init();
  if (initialized)
  {
    latest_data.supercap_mv = SupercapMonitor_ReadMillivolts();
  }

  /* The PMIC is optional for BLE/ADC operation; record presence independently. */
  if (NEH7100_Init())
  {
    (void)NEH7100_EnsureConfig();
  }

  temperature_status = (TempSensor_Init() == TEMP_SENSOR_RESULT_OK) ?
                       SENSOR_TEMPERATURE_IDLE : SENSOR_TEMPERATURE_NOT_PRESENT;
  motion_status = (LIS2DUX12_MotionInit(&hi2c1) == LIS2DUX12_MOTION_OK) ?
                  SENSOR_MOTION_ACCELEROMETER_READY : SENSOR_MOTION_NOT_PRESENT;
  motion_delay_ms = 0U;
  if (motion_status == SENSOR_MOTION_ACCELEROMETER_READY)
  {
    APP_DBG_MSG("-- LIS2DUXS12TR: WHO_AM_I OK, I2C=0x%02x\n",
                (unsigned int)(LIS2DUX12_MotionGetHalAddress() >> 1U));
    if (LIS2DUX12_MotionReadAcceleration(&acceleration) ==
        LIS2DUX12_MOTION_OK)
    {
      APP_DBG_MSG("-- LIS2DUXS12TR XYZ [mg]: %ld, %ld, %ld\n",
                  (long)acceleration.mg[0],
                  (long)acceleration.mg[1],
                  (long)acceleration.mg[2]);
    }
    
    if (LIS2DUX12_MotionInitQvar() == LIS2DUX12_MOTION_OK)
    {
      APP_DBG_MSG("-- LIS2DUXS12TR QVar initialized on INT1\n");
    }
  }
  else
  {
    APP_DBG_MSG("-- LIS2DUXS12TR: not present (optional)\n");
  }
  temperature_conversion_elapsed_ms = 0U;
  temperature_async_delay_ms = 0U;
  return initialized;
}

bool SensorManager_Start(void)
{
  if (!initialized)
  {
    return false;
  }
  running = true;
  SensorManager_StartTemperatureConversion();
  return true;
}

bool SensorManager_Stop(void)
{
  running = false;
  temperature_async_delay_ms = 0U;
  if (temperature_status != SENSOR_TEMPERATURE_NOT_PRESENT)
  {
    (void)TempSensor_Sleep();
    temperature_status = SENSOR_TEMPERATURE_IDLE;
  }
  return initialized;
}

bool SensorManager_GetLatestData(wearable_sensor_data_t *data)
{
  if ((!initialized) || (data == NULL))
  {
    return false;
  }
  *data = latest_data;
  return true;
}

void SensorManager_Process(void)
{
  if (!running)
  {
    return;
  }

  latest_data.supercap_mv = SupercapMonitor_ReadMillivolts();

  SensorManager_StartTemperatureConversion();

  if (motion_status == SENSOR_MOTION_ACCELEROMETER_READY || motion_status == SENSOR_MOTION_CLASSIFIER_READY)
  {
    lis2dux12_acceleration_t acceleration;
    int16_t qvar_raw = 0;
    
    if (LIS2DUX12_MotionReadAcceleration(&acceleration) == LIS2DUX12_MOTION_OK)
    {
      latest_data.accel_x = (int16_t)acceleration.mg[0];
      latest_data.accel_y = (int16_t)acceleration.mg[1];
      latest_data.accel_z = (int16_t)acceleration.mg[2];
    }
    
    if (LIS2DUX12_MotionReadQvar(&qvar_raw) == LIS2DUX12_MOTION_OK)
    {
       /* Simple wear/no-wear logic (0x40 is the arbitrary WEARING flag for now) */
       /* A proper threshold should be calibrated based on your hardware design */
       if (qvar_raw > 1000 || qvar_raw < -1000) 
       {
          latest_data.flags |= 0x40; /* Wear detected */
       }
       else
       {
          latest_data.flags &= ~0x40; /* No wear detected */
       }
    }
  }

  /* Only HR/SpO2 remain mock data until their sensor stages are implemented. */
  latest_data.heart_rate_bpm++;
  if (latest_data.heart_rate_bpm > 82U)
  {
    latest_data.heart_rate_bpm = 68U;
  }
}

void SensorManager_ProcessAsync(void)
{
  temp_sensor_result_t result;
  int16_t temperature_centi_c;

  if ((!running) || (temperature_status != SENSOR_TEMPERATURE_CONVERTING))
  {
    temperature_async_delay_ms = 0U;
    return;
  }

  temperature_conversion_elapsed_ms += temperature_async_delay_ms;
  result = TempSensor_TryReadCentiC(&temperature_centi_c);
  if (result == TEMP_SENSOR_RESULT_OK)
  {
    latest_data.temperature_centi_c = temperature_centi_c;
    temperature_async_delay_ms = 0U;
    temperature_status = SENSOR_TEMPERATURE_VALID;
    return;
  }

  if ((result == TEMP_SENSOR_RESULT_NOT_READY) &&
      (temperature_conversion_elapsed_ms < TEMPERATURE_CONVERSION_TIMEOUT_MS))
  {
    temperature_async_delay_ms = TEMPERATURE_RETRY_DELAY_MS;
    return;
  }

  temperature_async_delay_ms = 0U;
  temperature_status = (result == TEMP_SENSOR_RESULT_NOT_READY) ?
                       SENSOR_TEMPERATURE_TIMEOUT : SENSOR_TEMPERATURE_BUS_ERROR;
}

bool SensorManager_GetAsyncDelayMs(uint32_t *delay_ms)
{
  if ((delay_ms == NULL) ||
      (temperature_status != SENSOR_TEMPERATURE_CONVERTING) ||
      (temperature_async_delay_ms == 0U))
  {
    return false;
  }

  *delay_ms = temperature_async_delay_ms;
  return true;
}

sensor_temperature_status_t SensorManager_GetTemperatureStatus(void)
{
  return temperature_status;
}

sensor_motion_status_t SensorManager_GetMotionStatus(void)
{
  return motion_status;
}

void SensorManager_ProcessMotionInterrupt(void)
{
  lis2dux12_motion_result_t result;
  lis2dux12_motion_event_t event;

  if ((motion_status == SENSOR_MOTION_NOT_PRESENT) ||
      (motion_status == SENSOR_MOTION_BUS_ERROR))
  {
    return;
  }

  result = LIS2DUX12_MotionProcessInterrupt();
  if (result == LIS2DUX12_MOTION_OK)
  {
    if (!LIS2DUX12_MotionGetLatestEvent(&event))
    {
      return;
    }
    if (event.mlc_status != 0U)
    {
      motion_status = SENSOR_MOTION_CLASSIFIER_READY;
    }
    if (event.activity == LIS2DUX12_ACTIVITY_FALL)
    {
      SensorManager_SetFlag(WEARABLE_FLAG_FALL_CANDIDATE, true);
      APP_DBG_MSG("-- LIS2DUXS12TR: MLC fall candidate\n");
    }
  }
  else if (result == LIS2DUX12_MOTION_BUS_ERROR)
  {
    motion_status = SENSOR_MOTION_BUS_ERROR;
  }
}

void SensorManager_ProcessMotionTimeout(void)
{
  /* MLC classification is delivered directly through the sensor interrupt. */
  motion_delay_ms = 0U;
}

bool SensorManager_GetMotionDelayMs(uint32_t *delay_ms)
{
  if ((delay_ms == NULL) || (motion_delay_ms == 0U))
  {
    return false;
  }
  *delay_ms = motion_delay_ms;
  return true;
}

void SensorManager_SetPowerState(uint8_t power_state)
{
  latest_data.power_state = power_state;
}

void SensorManager_SetFlag(uint8_t flag, bool enabled)
{
  if (enabled)
  {
    latest_data.flags |= flag;
  }
  else
  {
    latest_data.flags &= (uint8_t)~flag;
  }
}
