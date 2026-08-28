/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    WEARABLE_app.c
  * @author  MCD Application Team
  * @brief   WEARABLE_app application definition.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "app_common.h"
#include "app_ble.h"
#include "ble.h"
#include "wearable_app.h"
#include "wearable.h"
#include "stm32_seq.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32wb0x_hal_radio_timer.h"
#include "../../Application/sensor_manager.h"
#include "../../Application/wearable_data.h"
#include "../../Application/wearable_state_manager.h"
#include "../../Application/device_time.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/

/* USER CODE BEGIN PTD */
typedef enum
{
  WEARABLE_CMD_START_MEASUREMENT = 0x01,
  WEARABLE_CMD_STOP_MEASUREMENT  = 0x02,
  WEARABLE_CMD_REQUEST_DATA      = 0x03,
  WEARABLE_CMD_NORMAL_MODE       = 0x04,
  WEARABLE_CMD_LOW_POWER_MODE    = 0x05,
  WEARABLE_CMD_ECG_START         = 0x06,
  WEARABLE_CMD_ECG_STOP          = 0x07,
  WEARABLE_CMD_EMERGENCY_TEST    = 0x08,
  WEARABLE_CMD_SYNC_TIME         = 0x09
} wearable_command_t;

/* USER CODE END PTD */

typedef enum
{
  Sensor_data_NOTIFICATION_OFF,
  Sensor_data_NOTIFICATION_ON,
  Device_status_NOTIFICATION_OFF,
  Device_status_NOTIFICATION_ON,
  /* USER CODE BEGIN Service1_APP_SendInformation_t */

  /* USER CODE END Service1_APP_SendInformation_t */
  WEARABLE_APP_SENDINFORMATION_LAST
} WEARABLE_APP_SendInformation_t;

typedef struct
{
  WEARABLE_APP_SendInformation_t     Sensor_data_Notification_Status;
  WEARABLE_APP_SendInformation_t     Device_status_Notification_Status;
  /* USER CODE BEGIN Service1_APP_Context_t */
  uint8_t ResetCounter;
  uint8_t ErrorCode;
  /* USER CODE END Service1_APP_Context_t */
  uint16_t              ConnectionHandle;
} WEARABLE_APP_Context_t;

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define WEARABLE_SENSOR_PERIOD_MS       1000U
#define WEARABLE_ERROR_NONE              0x00U
#define WEARABLE_ERROR_INVALID_COMMAND   0x01U
#define WEARABLE_ERROR_TEMP_NOT_PRESENT  0x10U
#define WEARABLE_ERROR_TEMP_TIMEOUT      0x11U
#define WEARABLE_ERROR_TEMP_BUS          0x12U
/* USER CODE END PD */

/* External variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private macros ------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
static WEARABLE_APP_Context_t WEARABLE_APP_Context;

uint8_t a_WEARABLE_UpdateCharData[247];

/* USER CODE BEGIN PV */
static VTIMER_HandleType wearable_sensor_timer;
static VTIMER_HandleType wearable_sensor_async_timer;
static VTIMER_HandleType wearable_motion_timer;
static uint8_t wearable_sensor_snapshot[WEARABLE_SENSOR_PAYLOAD_LENGTH];
static uint8_t wearable_status_snapshot[WEARABLE_STATUS_PAYLOAD_LENGTH];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void WEARABLE_Sensor_data_SendNotification(void);
static void WEARABLE_Device_status_SendNotification(void);

/* USER CODE BEGIN PFP */
static void WEARABLE_SensorTask(void);
static void WEARABLE_SensorTimerCallback(void *arg);
static void WEARABLE_SensorAsyncTask(void);
static void WEARABLE_MotionInterruptTask(void);
static void WEARABLE_MotionTimeoutTask(void);
static void WEARABLE_MotionTimerCallback(void *arg);
static void WEARABLE_ScheduleMotionTimeout(void);
static void WEARABLE_SensorAsyncTimerCallback(void *arg);
static void WEARABLE_ScheduleSensorAsyncTask(void);
static void WEARABLE_StopSensorAsyncTask(void);
static void WEARABLE_FillSensorPayload(uint8_t *payload);
static void WEARABLE_RefreshSensorSnapshot(void);
static void WEARABLE_RefreshStatusSnapshot(void);
static void WEARABLE_SendStatus(void);
/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/
void WEARABLE_Notification(WEARABLE_NotificationEvt_t *p_Notification)
{
  /* USER CODE BEGIN Service1_Notification_1 */

  /* USER CODE END Service1_Notification_1 */
  switch(p_Notification->EvtOpcode)
  {
    /* USER CODE BEGIN Service1_Notification_Service1_EvtOpcode */

    /* USER CODE END Service1_Notification_Service1_EvtOpcode */

    case WEARABLE_CONTROL_WRITE_EVT:
      /* USER CODE BEGIN Service1Char1_WRITE_EVT */
      if ((p_Notification->DataTransfered.Length == 0U) ||
          (p_Notification->DataTransfered.p_Payload == NULL))
      {
        APP_DBG_MSG("-- WEARABLE COMMAND: EMPTY PAYLOAD\n");
        break;
      }

      switch ((wearable_command_t)p_Notification->DataTransfered.p_Payload[0])
      {
        case WEARABLE_CMD_START_MEASUREMENT:
          WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_NONE;
          if (SensorManager_Start())
          {
            WearableState_Set(WEARABLE_STATE_MEASURING);
            HAL_RADIO_TIMER_StopVirtualTimer(&wearable_sensor_timer);
            HAL_RADIO_TIMER_StartVirtualTimer(&wearable_sensor_timer, WEARABLE_SENSOR_PERIOD_MS);
            WEARABLE_ScheduleSensorAsyncTask();
          }
          else
          {
            WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_INVALID_COMMAND;
            WearableState_Set(WEARABLE_STATE_ERROR);
          }
          APP_DBG_MSG("-- WEARABLE COMMAND: START MEASUREMENT\n");
          break;

        case WEARABLE_CMD_STOP_MEASUREMENT:
          WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_NONE;
          SensorManager_Stop();
          WearableState_Set(WEARABLE_STATE_IDLE);
          HAL_RADIO_TIMER_StopVirtualTimer(&wearable_sensor_timer);
          WEARABLE_StopSensorAsyncTask();
          APP_DBG_MSG("-- WEARABLE COMMAND: STOP MEASUREMENT\n");
          break;

        case WEARABLE_CMD_REQUEST_DATA:
          WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_NONE;
          UTIL_SEQ_SetTask(1U << CFG_TASK_WEARABLE_SENSOR_ID, CFG_SEQ_PRIO_0);
          APP_DBG_MSG("-- WEARABLE COMMAND: REQUEST CURRENT DATA\n");
          break;

        case WEARABLE_CMD_NORMAL_MODE:
          WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_NONE;
          SensorManager_SetPowerState(1U);
          if (WearableState_Get() == WEARABLE_STATE_LOW_POWER)
          {
            WearableState_Set(WEARABLE_STATE_IDLE);
          }
          WEARABLE_SendStatus();
          break;

        case WEARABLE_CMD_LOW_POWER_MODE:
          WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_NONE;
          SensorManager_Stop();
          SensorManager_SetPowerState(2U);
          WearableState_Set(WEARABLE_STATE_LOW_POWER);
          HAL_RADIO_TIMER_StopVirtualTimer(&wearable_sensor_timer);
          WEARABLE_StopSensorAsyncTask();
          WEARABLE_SendStatus();
          break;

        case WEARABLE_CMD_ECG_START:
          WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_NONE;
          SensorManager_Start();
          SensorManager_SetFlag(WEARABLE_FLAG_ECG_ACTIVE, true);
          WearableState_Set(WEARABLE_STATE_ECG_ACTIVE);
          HAL_RADIO_TIMER_StopVirtualTimer(&wearable_sensor_timer);
          HAL_RADIO_TIMER_StartVirtualTimer(&wearable_sensor_timer, WEARABLE_SENSOR_PERIOD_MS);
          WEARABLE_ScheduleSensorAsyncTask();
          WEARABLE_SendStatus();
          break;

        case WEARABLE_CMD_ECG_STOP:
          WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_NONE;
          SensorManager_Stop();
          SensorManager_SetFlag(WEARABLE_FLAG_ECG_ACTIVE, false);
          WearableState_Set(WEARABLE_STATE_IDLE);
          HAL_RADIO_TIMER_StopVirtualTimer(&wearable_sensor_timer);
          WEARABLE_StopSensorAsyncTask();
          WEARABLE_SendStatus();
          break;

        case WEARABLE_CMD_EMERGENCY_TEST:
          WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_NONE;
          SensorManager_SetFlag(WEARABLE_FLAG_EMERGENCY, true);
          WearableState_Set(WEARABLE_STATE_EMERGENCY);
          HAL_RADIO_TIMER_StopVirtualTimer(&wearable_sensor_timer);
          WEARABLE_SendStatus();
          break;

        case WEARABLE_CMD_SYNC_TIME:
          if (p_Notification->DataTransfered.Length < 8)
          {
            WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_INVALID_COMMAND;
            APP_DBG_MSG("-- WEARABLE TIME: SYNC FAILED\nReason: INVALID LENGTH\n");
          }
          else
          {
            uint8_t *payload = p_Notification->DataTransfered.p_Payload;
            if (payload[7] != 0)
            {
              WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_INVALID_COMMAND;
              APP_DBG_MSG("-- WEARABLE TIME: SYNC FAILED\nReason: RESERVED BYTE NON-ZERO\n");
            }
            else
            {
              uint32_t seconds = (uint32_t)payload[1] | ((uint32_t)payload[2] << 8) | ((uint32_t)payload[3] << 16) | ((uint32_t)payload[4] << 24);
              uint16_t millis = (uint16_t)payload[5] | ((uint16_t)payload[6] << 8);

              if (millis > 999)
              {
                WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_INVALID_COMMAND;
                APP_DBG_MSG("-- WEARABLE TIME: SYNC FAILED\nReason: INVALID MILLIS\n");
              }
              else if (DeviceTime_SetUnixTime(seconds, millis))
              {
                WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_NONE;
                APP_DBG_MSG("-- WEARABLE TIME: SYNC SUCCESS\nUnix seconds: %lu\nMilliseconds: %u\nDevice synchronized: YES\n", (unsigned long)seconds, (unsigned int)millis);
              }
              else
              {
                WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_INVALID_COMMAND;
                APP_DBG_MSG("-- WEARABLE TIME: SYNC FAILED\nReason: INTERNAL ERROR\n");
              }
            }
          }
          break;

        default:
          WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_INVALID_COMMAND;
          WearableState_Set(WEARABLE_STATE_ERROR);
          WEARABLE_RefreshStatusSnapshot();
          APP_DBG_MSG("-- WEARABLE COMMAND: UNSUPPORTED 0x%02X\n",
                      p_Notification->DataTransfered.p_Payload[0]);
          break;
      }
      /* USER CODE END Service1Char1_WRITE_EVT */
      break;

    case WEARABLE_SENSOR_DATA_READ_EVT:
      /* USER CODE BEGIN Service1Char2_READ_EVT */

      /* USER CODE END Service1Char2_READ_EVT */
      break;

    case WEARABLE_SENSOR_DATA_NOTIFY_ENABLED_EVT:
      /* USER CODE BEGIN Service1Char2_NOTIFY_ENABLED_EVT */
      WEARABLE_APP_Context.Sensor_data_Notification_Status = Sensor_data_NOTIFICATION_ON;
      if (WearableState_AllowsPeriodicMeasurement())
      {
        HAL_RADIO_TIMER_StopVirtualTimer(&wearable_sensor_timer);
        HAL_RADIO_TIMER_StartVirtualTimer(&wearable_sensor_timer, WEARABLE_SENSOR_PERIOD_MS);
      }
      /* USER CODE END Service1Char2_NOTIFY_ENABLED_EVT */
      break;

    case WEARABLE_SENSOR_DATA_NOTIFY_DISABLED_EVT:
      /* USER CODE BEGIN Service1Char2_NOTIFY_DISABLED_EVT */
      WEARABLE_APP_Context.Sensor_data_Notification_Status = Sensor_data_NOTIFICATION_OFF;
      /* USER CODE END Service1Char2_NOTIFY_DISABLED_EVT */
      break;

    case WEARABLE_DEVICE_STATUS_READ_EVT:
      /* USER CODE BEGIN Service1Char3_READ_EVT */

      /* USER CODE END Service1Char3_READ_EVT */
      break;

    case WEARABLE_DEVICE_STATUS_NOTIFY_ENABLED_EVT:
      /* USER CODE BEGIN Service1Char3_NOTIFY_ENABLED_EVT */
      WEARABLE_APP_Context.Device_status_Notification_Status = Device_status_NOTIFICATION_ON;
      WEARABLE_SendStatus();
      /* USER CODE END Service1Char3_NOTIFY_ENABLED_EVT */
      break;

    case WEARABLE_DEVICE_STATUS_NOTIFY_DISABLED_EVT:
      /* USER CODE BEGIN Service1Char3_NOTIFY_DISABLED_EVT */
      WEARABLE_APP_Context.Device_status_Notification_Status = Device_status_NOTIFICATION_OFF;
      /* USER CODE END Service1Char3_NOTIFY_DISABLED_EVT */
      break;

    default:
      /* USER CODE BEGIN Service1_Notification_default */

      /* USER CODE END Service1_Notification_default */
      break;
  }
  /* USER CODE BEGIN Service1_Notification_2 */

  /* USER CODE END Service1_Notification_2 */
  return;
}

void WEARABLE_APP_EvtRx(WEARABLE_APP_ConnHandleNotEvt_t *p_Notification)
{
  /* USER CODE BEGIN Service1_APP_EvtRx_1 */

  /* USER CODE END Service1_APP_EvtRx_1 */

  switch(p_Notification->EvtOpcode)
  {
    /* USER CODE BEGIN Service1_APP_EvtRx_Service1_EvtOpcode */

    /* USER CODE END Service1_APP_EvtRx_Service1_EvtOpcode */
    case WEARABLE_CONN_HANDLE_EVT :
      WEARABLE_APP_Context.ConnectionHandle = p_Notification->ConnectionHandle;
      /* USER CODE BEGIN Service1_APP_CENTR_CONN_HANDLE_EVT */

      /* USER CODE END Service1_APP_CENTR_CONN_HANDLE_EVT */
      break;
    case WEARABLE_DISCON_HANDLE_EVT :
      WEARABLE_APP_Context.ConnectionHandle = 0xFFFF;
      /* USER CODE BEGIN Service1_APP_DISCON_HANDLE_EVT */
      WEARABLE_APP_Context.Sensor_data_Notification_Status = Sensor_data_NOTIFICATION_OFF;
      WEARABLE_APP_Context.Device_status_Notification_Status = Device_status_NOTIFICATION_OFF;
      HAL_RADIO_TIMER_StopVirtualTimer(&wearable_sensor_timer);
      WEARABLE_StopSensorAsyncTask();
      SensorManager_Stop();
      WearableState_Set(WEARABLE_STATE_IDLE);
      /* USER CODE END Service1_APP_DISCON_HANDLE_EVT */
      break;

    default:
      /* USER CODE BEGIN Service1_APP_EvtRx_default */

      /* USER CODE END Service1_APP_EvtRx_default */
      break;
  }

  /* USER CODE BEGIN Service1_APP_EvtRx_2 */

  /* USER CODE END Service1_APP_EvtRx_2 */

  return;
}

void WEARABLE_APP_Init(void)
{
  WEARABLE_APP_Context.ConnectionHandle = 0xFFFF;
  WEARABLE_Init();

  /* USER CODE BEGIN Service1_APP_Init */
  DeviceTime_Init();
  WEARABLE_APP_Context.Sensor_data_Notification_Status = Sensor_data_NOTIFICATION_OFF;
  WEARABLE_APP_Context.Device_status_Notification_Status = Device_status_NOTIFICATION_OFF;
  WEARABLE_APP_Context.ResetCounter = 0U;
  WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_NONE;
  WearableState_Init();
  if (!SensorManager_Init())
  {
    WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_INVALID_COMMAND;
    WearableState_Set(WEARABLE_STATE_ERROR);
  }
  wearable_sensor_timer.callback = WEARABLE_SensorTimerCallback;
  wearable_sensor_async_timer.callback = WEARABLE_SensorAsyncTimerCallback;
  wearable_motion_timer.callback = WEARABLE_MotionTimerCallback;
  UTIL_SEQ_RegTask(1U << CFG_TASK_WEARABLE_SENSOR_ID, UTIL_SEQ_RFU, WEARABLE_SensorTask);
  UTIL_SEQ_RegTask(1U << CFG_TASK_WEARABLE_SENSOR_ASYNC_ID, UTIL_SEQ_RFU, WEARABLE_SensorAsyncTask);
  UTIL_SEQ_RegTask(1U << CFG_TASK_WEARABLE_MOTION_INT_ID, UTIL_SEQ_RFU, WEARABLE_MotionInterruptTask);
  UTIL_SEQ_RegTask(1U << CFG_TASK_WEARABLE_MOTION_TIMEOUT_ID, UTIL_SEQ_RFU, WEARABLE_MotionTimeoutTask);
  WEARABLE_RefreshSensorSnapshot();
  WEARABLE_RefreshStatusSnapshot();
  /* USER CODE END Service1_APP_Init */
  return;
}

/* USER CODE BEGIN FD */
const uint8_t *WEARABLE_APP_GetLatestSensorData(uint16_t *length)
{
  if (length != NULL)
  {
    *length = WEARABLE_SENSOR_PAYLOAD_LENGTH;
  }
  return wearable_sensor_snapshot;
}

const uint8_t *WEARABLE_APP_GetLatestDeviceStatus(uint16_t *length)
{
  WEARABLE_RefreshStatusSnapshot();
  if (length != NULL)
  {
    *length = WEARABLE_STATUS_PAYLOAD_LENGTH;
  }
  return wearable_status_snapshot;
}

void WEARABLE_APP_NotifyMotionInterruptFromISR(void)
{
  /* PB2 ISR only schedules work; the task performs all I2C accesses. */
  UTIL_SEQ_SetTask(1U << CFG_TASK_WEARABLE_MOTION_INT_ID, CFG_SEQ_PRIO_0);
}
/* USER CODE END FD */

/*************************************************************
 *
 * LOCAL FUNCTIONS
 *
 *************************************************************/
__USED void WEARABLE_Sensor_data_SendNotification(void) /* Property Notification */
{
  WEARABLE_APP_SendInformation_t notification_on_off = Sensor_data_NOTIFICATION_OFF;
  WEARABLE_Data_t wearable_notification_data;

  wearable_notification_data.p_Payload = (uint8_t*)a_WEARABLE_UpdateCharData;
  wearable_notification_data.Length = 0;

  /* USER CODE BEGIN Service1Char2_NS_1*/
  memcpy(a_WEARABLE_UpdateCharData, wearable_sensor_snapshot, WEARABLE_SENSOR_PAYLOAD_LENGTH);
  wearable_notification_data.Length = WEARABLE_SENSOR_PAYLOAD_LENGTH;
  if (WEARABLE_APP_Context.Sensor_data_Notification_Status == Sensor_data_NOTIFICATION_ON)
  {
    notification_on_off = Sensor_data_NOTIFICATION_ON;
  }
  /* USER CODE END Service1Char2_NS_1*/

  if (notification_on_off != Sensor_data_NOTIFICATION_OFF && WEARABLE_APP_Context.ConnectionHandle != 0xFFFF)
  {
    WEARABLE_NotifyValue(WEARABLE_SENSOR_DATA, &wearable_notification_data, WEARABLE_APP_Context.ConnectionHandle);
  }

  /* USER CODE BEGIN Service1Char2_NS_Last*/

  /* USER CODE END Service1Char2_NS_Last*/

  return;
}

__USED void WEARABLE_Device_status_SendNotification(void) /* Property Notification */
{
  WEARABLE_APP_SendInformation_t notification_on_off = Device_status_NOTIFICATION_OFF;
  WEARABLE_Data_t wearable_notification_data;

  wearable_notification_data.p_Payload = (uint8_t*)a_WEARABLE_UpdateCharData;
  wearable_notification_data.Length = 0;

  /* USER CODE BEGIN Service1Char3_NS_1*/
  WEARABLE_RefreshStatusSnapshot();
  memcpy(a_WEARABLE_UpdateCharData, wearable_status_snapshot, WEARABLE_STATUS_PAYLOAD_LENGTH);
  wearable_notification_data.Length = WEARABLE_STATUS_PAYLOAD_LENGTH;
  if (WEARABLE_APP_Context.Device_status_Notification_Status == Device_status_NOTIFICATION_ON)
  {
    notification_on_off = Device_status_NOTIFICATION_ON;
  }
  /* USER CODE END Service1Char3_NS_1*/

  if (notification_on_off != Device_status_NOTIFICATION_OFF && WEARABLE_APP_Context.ConnectionHandle != 0xFFFF)
  {
    WEARABLE_NotifyValue(WEARABLE_DEVICE_STATUS, &wearable_notification_data, WEARABLE_APP_Context.ConnectionHandle);
  }

  /* USER CODE BEGIN Service1Char3_NS_Last*/

  /* USER CODE END Service1Char3_NS_Last*/

  return;
}

/* USER CODE BEGIN FD_LOCAL_FUNCTIONS*/
static void WEARABLE_FillSensorPayload(uint8_t *payload)
{
  wearable_sensor_data_t data;
  if (SensorManager_GetLatestData(&data))
  {
    WearableData_EncodeSensor(&data, payload);
  }
  else
  {
    memset(payload, 0, WEARABLE_SENSOR_PAYLOAD_LENGTH);
  }
}

static void WEARABLE_RefreshSensorSnapshot(void)
{
  WEARABLE_FillSensorPayload(wearable_sensor_snapshot);
}

static void WEARABLE_RefreshStatusSnapshot(void)
{
  wearable_sensor_data_t sensor_data;
  wearable_device_status_t status = {0};
  sensor_temperature_status_t temperature_status;
  status.measurement_state = (uint8_t)WearableState_Get();
  status.sensor_ready = SensorManager_GetLatestData(&sensor_data) ? 1U : 0U;
  status.error_code = WEARABLE_APP_Context.ErrorCode;
  temperature_status = SensorManager_GetTemperatureStatus();
  if (status.error_code == WEARABLE_ERROR_NONE)
  {
    if (temperature_status == SENSOR_TEMPERATURE_NOT_PRESENT)
    {
      status.error_code = WEARABLE_ERROR_TEMP_NOT_PRESENT;
    }
    else if (temperature_status == SENSOR_TEMPERATURE_TIMEOUT)
    {
      status.error_code = WEARABLE_ERROR_TEMP_TIMEOUT;
    }
    else if (temperature_status == SENSOR_TEMPERATURE_BUS_ERROR)
    {
      status.error_code = WEARABLE_ERROR_TEMP_BUS;
    }
  }
  if (status.sensor_ready != 0U)
  {
    status.power_state = sensor_data.power_state;
    status.supercap_mv = sensor_data.supercap_mv;
    status.flags = sensor_data.flags;
  }
  status.reset_counter = WEARABLE_APP_Context.ResetCounter;
  WearableData_EncodeStatus(&status, wearable_status_snapshot);
}

static void WEARABLE_SensorTask(void)
{
  if (WearableState_AllowsPeriodicMeasurement())
  {
    SensorManager_Process();
    WEARABLE_RefreshSensorSnapshot();
    WEARABLE_ScheduleSensorAsyncTask();
  }

  if ((WEARABLE_APP_Context.ConnectionHandle != 0xFFFFU) &&
      (WEARABLE_APP_Context.Sensor_data_Notification_Status == Sensor_data_NOTIFICATION_ON))
  {
    WEARABLE_Sensor_data_SendNotification();
  }

  if (WearableState_AllowsPeriodicMeasurement())
  {
    HAL_RADIO_TIMER_StartVirtualTimer(&wearable_sensor_timer, WEARABLE_SENSOR_PERIOD_MS);
  }
}

static void WEARABLE_SensorTimerCallback(void *arg)
{
  UNUSED(arg);
  UTIL_SEQ_SetTask(1U << CFG_TASK_WEARABLE_SENSOR_ID, CFG_SEQ_PRIO_0);
}

static void WEARABLE_SensorAsyncTask(void)
{
  if (WearableState_AllowsPeriodicMeasurement())
  {
    SensorManager_ProcessAsync();
    WEARABLE_RefreshSensorSnapshot();
    WEARABLE_ScheduleSensorAsyncTask();
  }
}

static void WEARABLE_MotionInterruptTask(void)
{
  SensorManager_ProcessMotionInterrupt();
  WEARABLE_ScheduleMotionTimeout();
  WEARABLE_RefreshStatusSnapshot();
}

static void WEARABLE_MotionTimeoutTask(void)
{
  SensorManager_ProcessMotionTimeout();
  WEARABLE_RefreshSensorSnapshot();
  WEARABLE_RefreshStatusSnapshot();
}

static void WEARABLE_MotionTimerCallback(void *arg)
{
  UNUSED(arg);
  UTIL_SEQ_SetTask(1U << CFG_TASK_WEARABLE_MOTION_TIMEOUT_ID,
                   CFG_SEQ_PRIO_0);
}

static void WEARABLE_ScheduleMotionTimeout(void)
{
  uint32_t delay_ms;

  HAL_RADIO_TIMER_StopVirtualTimer(&wearable_motion_timer);
  if (SensorManager_GetMotionDelayMs(&delay_ms))
  {
    HAL_RADIO_TIMER_StartVirtualTimer(&wearable_motion_timer, delay_ms);
  }
}

static void WEARABLE_SensorAsyncTimerCallback(void *arg)
{
  UNUSED(arg);
  UTIL_SEQ_SetTask(1U << CFG_TASK_WEARABLE_SENSOR_ASYNC_ID, CFG_SEQ_PRIO_0);
}

static void WEARABLE_ScheduleSensorAsyncTask(void)
{
  uint32_t delay_ms;

  HAL_RADIO_TIMER_StopVirtualTimer(&wearable_sensor_async_timer);
  if (SensorManager_GetAsyncDelayMs(&delay_ms))
  {
    HAL_RADIO_TIMER_StartVirtualTimer(&wearable_sensor_async_timer, delay_ms);
  }
}

static void WEARABLE_StopSensorAsyncTask(void)
{
  HAL_RADIO_TIMER_StopVirtualTimer(&wearable_sensor_async_timer);
}

static void WEARABLE_SendStatus(void)
{
  WEARABLE_Device_status_SendNotification();
}
/* USER CODE END FD_LOCAL_FUNCTIONS*/
