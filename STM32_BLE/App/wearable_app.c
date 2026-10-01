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
#include "../../Application/SensorManager/sensor_manager.h"
#include "../../Application/wearable_data.h"
#include "../../Application/StateManager/wearable_state_manager.h"
#include "../../Application/DeviceTime/device_time.h"
#include "../../Application/DataRecovery/data_recovery_manager.h"
#include "../../Application/SensorManager/ecg_diag.h"
#include "../../Application/LoRaWAN/lbm_app.h"
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
  WEARABLE_CMD_SYNC_TIME         = 0x09,
  WEARABLE_CMD_GET_RECOVERY_INFO = 0x0A,
  WEARABLE_CMD_START_BLE_RECOVERY= 0x0B,
  WEARABLE_CMD_STOP_BLE_RECOVERY = 0x0C,
  WEARABLE_CMD_RECOVERY_ACK      = 0x0D,
  WEARABLE_CMD_RECOVERY_CLEAR    = 0x0E,
  /* LoRaWAN test; answered on DEBUG_DATA (FE46) with packet 0x20. */
  WEARABLE_CMD_LORA_JOIN         = 0x0F,
  WEARABLE_CMD_LORA_UPLINK       = 0x10,
  WEARABLE_CMD_LORA_STATUS       = 0x11,
  WEARABLE_CMD_LORA_TX_POWER     = 0x12,
  WEARABLE_CMD_LORA_LEAVE        = 0x13
} wearable_command_t;

typedef enum
{
  DEBUG_CMD_SET_SENSOR_RATE   = 0x01,
  DEBUG_CMD_SET_BLE_INTERVAL  = 0x02,
  DEBUG_CMD_SET_LOG_INTERVAL  = 0x03,
  DEBUG_CMD_SET_POWER_THRESH  = 0x04,
  DEBUG_CMD_GET_POWER_STATS   = 0x05,
  DEBUG_CMD_GET_LOG_INFO      = 0x06,
  DEBUG_CMD_GET_SENSOR_STATUS = 0x07
} debug_command_t;

/* USER CODE END PTD */

typedef enum
{
  Sensor_data_NOTIFICATION_OFF,
  Sensor_data_NOTIFICATION_ON,
  Device_status_NOTIFICATION_OFF,
  Device_status_NOTIFICATION_ON,
  Nfc_data_NOTIFICATION_OFF,
  Nfc_data_NOTIFICATION_ON,
  Ecg_data_NOTIFICATION_OFF,
  Ecg_data_NOTIFICATION_ON,
  Debug_data_NOTIFICATION_OFF,
  Debug_data_NOTIFICATION_ON,
  Recovery_data_NOTIFICATION_OFF,
  Recovery_data_NOTIFICATION_ON,
  /* USER CODE BEGIN Service1_APP_SendInformation_t */

  /* USER CODE END Service1_APP_SendInformation_t */
  WEARABLE_APP_SENDINFORMATION_LAST
} WEARABLE_APP_SendInformation_t;

typedef struct
{
  WEARABLE_APP_SendInformation_t     Sensor_data_Notification_Status;
  WEARABLE_APP_SendInformation_t     Device_status_Notification_Status;
  WEARABLE_APP_SendInformation_t     Nfc_data_Notification_Status;
  WEARABLE_APP_SendInformation_t     Ecg_data_Notification_Status;
  WEARABLE_APP_SendInformation_t     Debug_data_Notification_Status;
  WEARABLE_APP_SendInformation_t     Recovery_data_Notification_Status;
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
/* ECG streaming: the MAX86150 FIFO holds 32 samples = 160 ms at 200 sps.
 * Draining every 45 ms yields one 9-sample ECG_DATA packet per drain on
 * average and leaves >100 ms of slack for a late sequencer turn.
 * Production value 45 ms lives in ecg_diag.h (DIAG_READ_PERIOD). */
#define WEARABLE_ECG_DRAIN_PERIOD_MS     ECG_DIAG_DRAIN_PERIOD_MS
#define WEARABLE_ECG_READ_MAX_SAMPLES    32U
/* Packets held back while the BLE TX pool is full (~0.7 s of ECG). */
#define WEARABLE_ECG_QUEUE_LEN           16U
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
/* Last DEVICE_STATUS handed to the notify path, for WEARABLE_SendStatusIfChanged(). */
static uint8_t wearable_status_reported[WEARABLE_STATUS_PAYLOAD_LENGTH];
/* Set by the first BLE connection after a reset; reconnections leave it alone. */
static bool wearable_first_connection_done;
/* LoRa CONTROL command waiting for WEARABLE_LoraTask (0 = none). */
static uint8_t wearable_lora_command;
static int8_t wearable_lora_param;
static uint8_t wearable_debug_snapshot[WEARABLE_DEBUG_PAYLOAD_LENGTH];
static VTIMER_HandleType wearable_ecg_timer;
static wearable_ecg_packet_t wearable_ecg_pending;
static uint8_t wearable_ecg_sequence;
static uint8_t wearable_ecg_queue[WEARABLE_ECG_QUEUE_LEN][WEARABLE_ECG_PAYLOAD_LENGTH];
static uint8_t wearable_ecg_queue_head;
static uint8_t wearable_ecg_queue_tail;
static uint8_t wearable_ecg_queue_count;
/* Packets overwritten in a full queue; the phone sees the gap in sequence. */
static uint32_t wearable_ecg_dropped_packets;
#if (ECG_DIAG_NOTIFY_EVERY_N_DRAINS > 1U)
/* Drains since the queue was last handed to the BLE stack (diag only). */
static uint8_t wearable_ecg_drains_since_flush;
#endif
volatile ecg_diag_info_t g_ecgDiag;
/* Drain timing bookkeeping for g_ecgDiag (observation only). */
static bool wearable_ecg_diag_fired;       /* timer callback ran since the last drain */
static uint32_t wearable_ecg_diag_fire_us;
static bool wearable_ecg_diag_prev_fire_valid;
static uint32_t wearable_ecg_diag_prev_fire_us;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void WEARABLE_Sensor_data_SendNotification(void);
static void WEARABLE_Device_status_SendNotification(void);
static void WEARABLE_Nfc_data_SendNotification(void);
static void WEARABLE_Ecg_data_SendNotification(void);
static void WEARABLE_Debug_data_SendNotification(void);
static void WEARABLE_Recovery_data_SendNotification(void);

/* USER CODE BEGIN PFP */
static void WEARABLE_FirstConnectionTask(void);
static void WEARABLE_LoraTask(void);
#if LBM_APP_ENABLE
static void WEARABLE_LoraEventCallback(void);
#endif
static void WEARABLE_SendLoraStatus(uint8_t command_result);
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
static void WEARABLE_SendStatusIfChanged(void);
static void WEARABLE_EcgTask(void);
static void WEARABLE_EcgTimerCallback(void *arg);
static void WEARABLE_StartEcgStream(void);
static void WEARABLE_StopEcgStream(void);
static bool WEARABLE_EcgNotificationsEnabled(void);
static void WEARABLE_EcgQueuePacket(void);
static void WEARABLE_EcgFlushQueue(void);
static uint32_t WEARABLE_EcgDiagMicros(void);
static void WEARABLE_EcgDiagReset(void);
static void WEARABLE_EcgDiagRecord(uint32_t read_start_us, uint32_t read_done_us, uint8_t count);
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
          WEARABLE_StopEcgStream();
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
          WEARABLE_SendStatus();
          APP_DBG_MSG("-- WEARABLE COMMAND: START MEASUREMENT\n");
          break;

        case WEARABLE_CMD_STOP_MEASUREMENT:
          WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_NONE;
          WEARABLE_StopEcgStream();
          SensorManager_Stop();
          WearableState_Set(WEARABLE_STATE_IDLE);
          HAL_RADIO_TIMER_StopVirtualTimer(&wearable_sensor_timer);
          WEARABLE_StopSensorAsyncTask();
          WEARABLE_SendStatus();
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
          WEARABLE_StopEcgStream();
          SensorManager_Stop();
          SensorManager_SetPowerState(2U);
          WearableState_Set(WEARABLE_STATE_LOW_POWER);
          HAL_RADIO_TIMER_StopVirtualTimer(&wearable_sensor_timer);
          WEARABLE_StopSensorAsyncTask();
          WEARABLE_SendStatus();
          break;

        case WEARABLE_CMD_ECG_START:
          WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_NONE;
          if (!SensorManager_Start())
          {
            WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_INVALID_COMMAND;
            WearableState_Set(WEARABLE_STATE_ERROR);
            WEARABLE_SendStatus();
            break;
          }
          if (SensorManager_StartEcg())
          {
            WearableState_Set(WEARABLE_STATE_ECG_ACTIVE);
            WEARABLE_StartEcgStream();
          }
          else
          {
            /* MAX86150 missing/not responding: fall back to plain measuring. */
            WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_INVALID_COMMAND;
            WearableState_Set(WEARABLE_STATE_MEASURING);
          }
          HAL_RADIO_TIMER_StopVirtualTimer(&wearable_sensor_timer);
          HAL_RADIO_TIMER_StartVirtualTimer(&wearable_sensor_timer, WEARABLE_SENSOR_PERIOD_MS);
          WEARABLE_ScheduleSensorAsyncTask();
          WEARABLE_SendStatus();
          break;

        case WEARABLE_CMD_ECG_STOP:
          WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_NONE;
          WEARABLE_StopEcgStream();
          SensorManager_Stop();
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
          WEARABLE_SendStatus();
          break;

        case WEARABLE_CMD_GET_RECOVERY_INFO:
          WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_NONE;
          // Trigger something to send recovery info if needed
          break;

        case WEARABLE_CMD_START_BLE_RECOVERY:
          if (p_Notification->DataTransfered.Length >= 3) {
             uint16_t start_seq = (uint16_t)p_Notification->DataTransfered.p_Payload[1] |
                                  ((uint16_t)p_Notification->DataTransfered.p_Payload[2] << 8);
             DataRecovery_StartBLERecovery(start_seq);
          }
          break;

        case WEARABLE_CMD_STOP_BLE_RECOVERY:
          DataRecovery_StopBLERecovery();
          break;

        case WEARABLE_CMD_RECOVERY_ACK:
          if (p_Notification->DataTransfered.Length >= 3) {
             uint16_t ack_seq = (uint16_t)p_Notification->DataTransfered.p_Payload[1] |
                                ((uint16_t)p_Notification->DataTransfered.p_Payload[2] << 8);
             DataRecovery_HandleACK(ack_seq);
          }
          break;

        case WEARABLE_CMD_RECOVERY_CLEAR:
          DataRecovery_Clear();
          break;

        case WEARABLE_CMD_LORA_JOIN:
        case WEARABLE_CMD_LORA_UPLINK:
        case WEARABLE_CMD_LORA_STATUS:
        case WEARABLE_CMD_LORA_TX_POWER:
        case WEARABLE_CMD_LORA_LEAVE:
          /* Starting the modem runs smtc_modem_init(): not inside a BLE event. */
          wearable_lora_command = p_Notification->DataTransfered.p_Payload[0];
          wearable_lora_param = (p_Notification->DataTransfered.Length >= 2U) ?
                                (int8_t)p_Notification->DataTransfered.p_Payload[1] : 0;
          UTIL_SEQ_SetTask(1U << CFG_TASK_WEARABLE_LORA_ID, CFG_SEQ_PRIO_1);
          break;

        default:
          WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_INVALID_COMMAND;
          WearableState_Set(WEARABLE_STATE_ERROR);
          WEARABLE_SendStatus();
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

    case WEARABLE_NFC_DATA_READ_EVT:
      /* USER CODE BEGIN Service1Char4_READ_EVT */

      /* USER CODE END Service1Char4_READ_EVT */
      break;

    case WEARABLE_NFC_DATA_NOTIFY_ENABLED_EVT:
      /* USER CODE BEGIN Service1Char4_NOTIFY_ENABLED_EVT */
      WEARABLE_APP_Context.Nfc_data_Notification_Status = Nfc_data_NOTIFICATION_ON;
      /* USER CODE END Service1Char4_NOTIFY_ENABLED_EVT */
      break;

    case WEARABLE_NFC_DATA_NOTIFY_DISABLED_EVT:
      /* USER CODE BEGIN Service1Char4_NOTIFY_DISABLED_EVT */
      WEARABLE_APP_Context.Nfc_data_Notification_Status = Nfc_data_NOTIFICATION_OFF;
      /* USER CODE END Service1Char4_NOTIFY_DISABLED_EVT */
      break;

    case WEARABLE_ECG_DATA_READ_EVT:
      /* USER CODE BEGIN Service1Char5_READ_EVT */

      /* USER CODE END Service1Char5_READ_EVT */
      break;

    case WEARABLE_ECG_DATA_NOTIFY_ENABLED_EVT:
      /* USER CODE BEGIN Service1Char5_NOTIFY_ENABLED_EVT */
      WEARABLE_APP_Context.Ecg_data_Notification_Status = Ecg_data_NOTIFICATION_ON;
      /* USER CODE END Service1Char5_NOTIFY_ENABLED_EVT */
      break;

    case WEARABLE_ECG_DATA_NOTIFY_DISABLED_EVT:
      /* USER CODE BEGIN Service1Char5_NOTIFY_DISABLED_EVT */
      WEARABLE_APP_Context.Ecg_data_Notification_Status = Ecg_data_NOTIFICATION_OFF;
      /* USER CODE END Service1Char5_NOTIFY_DISABLED_EVT */
      break;

    case WEARABLE_DEBUG_DATA_READ_EVT:
      /* USER CODE BEGIN Service1Char6_READ_EVT */

      /* USER CODE END Service1Char6_READ_EVT */
      break;

    case WEARABLE_DEBUG_DATA_WRITE_EVT:
      /* USER CODE BEGIN Service1Char6_WRITE_EVT */

      /* USER CODE END Service1Char6_WRITE_EVT */
      break;

    case WEARABLE_DEBUG_DATA_NOTIFY_ENABLED_EVT:
      /* USER CODE BEGIN Service1Char6_NOTIFY_ENABLED_EVT */
      WEARABLE_APP_Context.Debug_data_Notification_Status = Debug_data_NOTIFICATION_ON;
      /* USER CODE END Service1Char6_NOTIFY_ENABLED_EVT */
      break;

    case WEARABLE_DEBUG_DATA_NOTIFY_DISABLED_EVT:
      /* USER CODE BEGIN Service1Char6_NOTIFY_DISABLED_EVT */
      WEARABLE_APP_Context.Debug_data_Notification_Status = Debug_data_NOTIFICATION_OFF;
      /* USER CODE END Service1Char6_NOTIFY_DISABLED_EVT */
      break;

    case WEARABLE_RECOVERY_DATA_READ_EVT:
      /* USER CODE BEGIN Service1Char7_READ_EVT */

      /* USER CODE END Service1Char7_READ_EVT */
      break;

    case WEARABLE_RECOVERY_DATA_NOTIFY_ENABLED_EVT:
      /* USER CODE BEGIN Service1Char7_NOTIFY_ENABLED_EVT */
      WEARABLE_APP_Context.Recovery_data_Notification_Status = Recovery_data_NOTIFICATION_ON;
      /* USER CODE END Service1Char7_NOTIFY_ENABLED_EVT */
      break;

    case WEARABLE_RECOVERY_DATA_NOTIFY_DISABLED_EVT:
      /* USER CODE BEGIN Service1Char7_NOTIFY_DISABLED_EVT */
      WEARABLE_APP_Context.Recovery_data_Notification_Status = Recovery_data_NOTIFICATION_OFF;
      /* USER CODE END Service1Char7_NOTIFY_DISABLED_EVT */
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
      if (!wearable_first_connection_done)
      {
        /* Nothing but BLE runs before this point. The bring-up is I2C work,
         * so it goes to a task instead of running inside this BLE event. */
        UTIL_SEQ_SetTask(1U << CFG_TASK_WEARABLE_FIRST_CONN_ID, CFG_SEQ_PRIO_0);
      }
      /* USER CODE END Service1_APP_CENTR_CONN_HANDLE_EVT */
      break;
    case WEARABLE_DISCON_HANDLE_EVT :
      WEARABLE_APP_Context.ConnectionHandle = 0xFFFF;
      /* USER CODE BEGIN Service1_APP_DISCON_HANDLE_EVT */
      WEARABLE_APP_Context.Sensor_data_Notification_Status = Sensor_data_NOTIFICATION_OFF;
      WEARABLE_APP_Context.Device_status_Notification_Status = Device_status_NOTIFICATION_OFF;
      WEARABLE_APP_Context.Nfc_data_Notification_Status = Nfc_data_NOTIFICATION_OFF;
      WEARABLE_APP_Context.Ecg_data_Notification_Status = Ecg_data_NOTIFICATION_OFF;
      WEARABLE_APP_Context.Debug_data_Notification_Status = Debug_data_NOTIFICATION_OFF;
      WEARABLE_APP_Context.Recovery_data_Notification_Status = Recovery_data_NOTIFICATION_OFF;
      HAL_RADIO_TIMER_StopVirtualTimer(&wearable_sensor_timer);
      WEARABLE_StopSensorAsyncTask();
      WEARABLE_StopEcgStream();
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
  /* SensorManager_Init() is not called here: see WEARABLE_FirstConnectionTask(). */
  wearable_sensor_timer.callback = WEARABLE_SensorTimerCallback;
  wearable_sensor_async_timer.callback = WEARABLE_SensorAsyncTimerCallback;
  wearable_motion_timer.callback = WEARABLE_MotionTimerCallback;
  wearable_ecg_timer.callback = WEARABLE_EcgTimerCallback;
  g_ecgDiag.conn_req_status = 0xFFU;
  g_ecgDiag.magic = ECG_DIAG_MAGIC;
  UTIL_SEQ_RegTask(1U << CFG_TASK_WEARABLE_FIRST_CONN_ID, UTIL_SEQ_RFU, WEARABLE_FirstConnectionTask);
  UTIL_SEQ_RegTask(1U << CFG_TASK_WEARABLE_LORA_ID, UTIL_SEQ_RFU, WEARABLE_LoraTask);
#if LBM_APP_ENABLE
  LBM_App_SetStatusCallback(WEARABLE_LoraEventCallback);
#endif
  UTIL_SEQ_RegTask(1U << CFG_TASK_WEARABLE_SENSOR_ID, UTIL_SEQ_RFU, WEARABLE_SensorTask);
  UTIL_SEQ_RegTask(1U << CFG_TASK_WEARABLE_SENSOR_ASYNC_ID, UTIL_SEQ_RFU, WEARABLE_SensorAsyncTask);
  UTIL_SEQ_RegTask(1U << CFG_TASK_WEARABLE_MOTION_INT_ID, UTIL_SEQ_RFU, WEARABLE_MotionInterruptTask);
  UTIL_SEQ_RegTask(1U << CFG_TASK_WEARABLE_MOTION_TIMEOUT_ID, UTIL_SEQ_RFU, WEARABLE_MotionTimeoutTask);
  UTIL_SEQ_RegTask(1U << CFG_TASK_WEARABLE_ECG_ID, UTIL_SEQ_RFU, WEARABLE_EcgTask);
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

uint16_t WEARABLE_APP_GetConnectionHandle(void)
{
  return WEARABLE_APP_Context.ConnectionHandle;
}

void WEARABLE_APP_NotifyTxPoolAvailable(void)
{
  /* BLE stack freed TX buffers: retry queued ECG packets now rather than
   * at the next drain. */
#if (ECG_DIAG_NOTIFY_EVERY_N_DRAINS == 1U)
  if (wearable_ecg_queue_count != 0U)
  {
    UTIL_SEQ_SetTask(1U << CFG_TASK_WEARABLE_ECG_ID, CFG_SEQ_PRIO_0);
  }
#endif
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
  memcpy(wearable_status_reported, wearable_status_snapshot, WEARABLE_STATUS_PAYLOAD_LENGTH);
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

__USED void WEARABLE_Nfc_data_SendNotification(void) /* Property Notification */
{
  WEARABLE_APP_SendInformation_t notification_on_off = Nfc_data_NOTIFICATION_OFF;
  WEARABLE_Data_t wearable_notification_data;

  wearable_notification_data.p_Payload = (uint8_t*)a_WEARABLE_UpdateCharData;
  wearable_notification_data.Length = 0;

  /* USER CODE BEGIN Service1Char4_NS_1*/
  if (WEARABLE_APP_Context.Nfc_data_Notification_Status == Nfc_data_NOTIFICATION_ON)
  {
    notification_on_off = Nfc_data_NOTIFICATION_ON;
  }
  /* USER CODE END Service1Char4_NS_1*/

  if (notification_on_off != Nfc_data_NOTIFICATION_OFF && WEARABLE_APP_Context.ConnectionHandle != 0xFFFF)
  {
    WEARABLE_NotifyValue(WEARABLE_NFC_DATA, &wearable_notification_data, WEARABLE_APP_Context.ConnectionHandle);
  }

  /* USER CODE BEGIN Service1Char4_NS_Last*/

  /* USER CODE END Service1Char4_NS_Last*/

  return;
}

__USED void WEARABLE_Ecg_data_SendNotification(void) /* Property Notification */
{
  WEARABLE_APP_SendInformation_t notification_on_off = Ecg_data_NOTIFICATION_OFF;
  WEARABLE_Data_t wearable_notification_data;

  wearable_notification_data.p_Payload = (uint8_t*)a_WEARABLE_UpdateCharData;
  wearable_notification_data.Length = 0;

  /* USER CODE BEGIN Service1Char5_NS_1*/
  if (WEARABLE_APP_Context.Ecg_data_Notification_Status == Ecg_data_NOTIFICATION_ON)
  {
    notification_on_off = Ecg_data_NOTIFICATION_ON;
  }
  /* USER CODE END Service1Char5_NS_1*/

  if (notification_on_off != Ecg_data_NOTIFICATION_OFF && WEARABLE_APP_Context.ConnectionHandle != 0xFFFF)
  {
    WEARABLE_NotifyValue(WEARABLE_ECG_DATA, &wearable_notification_data, WEARABLE_APP_Context.ConnectionHandle);
  }

  /* USER CODE BEGIN Service1Char5_NS_Last*/

  /* USER CODE END Service1Char5_NS_Last*/

  return;
}

__USED void WEARABLE_Debug_data_SendNotification(void) /* Property Notification */
{
  WEARABLE_APP_SendInformation_t notification_on_off = Debug_data_NOTIFICATION_OFF;
  WEARABLE_Data_t wearable_notification_data;

  wearable_notification_data.p_Payload = (uint8_t*)a_WEARABLE_UpdateCharData;
  wearable_notification_data.Length = 0;

  /* USER CODE BEGIN Service1Char6_NS_1*/
  memcpy(a_WEARABLE_UpdateCharData, wearable_debug_snapshot, WEARABLE_DEBUG_PAYLOAD_LENGTH);
  wearable_notification_data.Length = WEARABLE_DEBUG_PAYLOAD_LENGTH;
  if (WEARABLE_APP_Context.Debug_data_Notification_Status == Debug_data_NOTIFICATION_ON)
  {
    notification_on_off = Debug_data_NOTIFICATION_ON;
  }
  /* USER CODE END Service1Char6_NS_1*/

  if (notification_on_off != Debug_data_NOTIFICATION_OFF && WEARABLE_APP_Context.ConnectionHandle != 0xFFFF)
  {
    WEARABLE_NotifyValue(WEARABLE_DEBUG_DATA, &wearable_notification_data, WEARABLE_APP_Context.ConnectionHandle);
  }

  /* USER CODE BEGIN Service1Char6_NS_Last*/

  /* USER CODE END Service1Char6_NS_Last*/

  return;
}

__USED void WEARABLE_Recovery_data_SendNotification(void) /* Property Notification */
{
  WEARABLE_APP_SendInformation_t notification_on_off = Recovery_data_NOTIFICATION_OFF;
  WEARABLE_Data_t wearable_notification_data;

  wearable_notification_data.p_Payload = (uint8_t*)a_WEARABLE_UpdateCharData;
  wearable_notification_data.Length = 0;

  /* USER CODE BEGIN Service1Char7_NS_1*/
  if (WEARABLE_APP_Context.Recovery_data_Notification_Status == Recovery_data_NOTIFICATION_ON)
  {
    notification_on_off = Recovery_data_NOTIFICATION_ON;
  }
  /* USER CODE END Service1Char7_NS_1*/

  if (notification_on_off != Recovery_data_NOTIFICATION_OFF && WEARABLE_APP_Context.ConnectionHandle != 0xFFFF)
  {
    WEARABLE_NotifyValue(WEARABLE_RECOVERY_DATA, &wearable_notification_data, WEARABLE_APP_Context.ConnectionHandle);
  }

  /* USER CODE BEGIN Service1Char7_NS_Last*/

  /* USER CODE END Service1Char7_NS_Last*/

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
  /* The outcome of the last finished conversion: the live status goes back
   * to CONVERTING every second and would hide a timeout/bus error. */
  temperature_status = SensorManager_GetTemperatureResult();
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

/* Runs once, right after the first BLE connection following a reset: only now
 * are the sensors probed and configured (I2C, ADC). They still measure
 * nothing until START_MEASUREMENT / ECG_START arrives. Queued at priority 0,
 * it runs ahead of the BLE stack task (priority 1), i.e. before the central's
 * first request is served. */
static void WEARABLE_FirstConnectionTask(void)
{
  if (wearable_first_connection_done)
  {
    return;
  }
  wearable_first_connection_done = true;

  if (!SensorManager_Init())
  {
    WEARABLE_APP_Context.ErrorCode = WEARABLE_ERROR_INVALID_COMMAND;
    WearableState_Set(WEARABLE_STATE_ERROR);
  }
  WEARABLE_RefreshSensorSnapshot();
  WEARABLE_RefreshStatusSnapshot();
}

/* LoRa CONTROL commands 0x0F-0x13. LoRa never starts on its own: the join
 * only begins with 0x0F. Every command is answered with packet 0x20 on FE46. */
static void WEARABLE_LoraTask(void)
{
  uint8_t command = wearable_lora_command;
  uint8_t result = WEARABLE_LORA_RESULT_OK;

  wearable_lora_command = 0U;
  if (command == 0U)
  {
    return;
  }
#if LBM_APP_ENABLE
  int32_t rc = 0;

  switch ((wearable_command_t)command)
  {
    case WEARABLE_CMD_LORA_JOIN:
      rc = LBM_App_Join();
      break;
    case WEARABLE_CMD_LORA_UPLINK:
      rc = LBM_App_SendTestUplink();
      if (rc == LBM_APP_NOT_JOINED)
      {
        result = WEARABLE_LORA_RESULT_NOT_JOINED;
        rc = 0;
      }
      break;
    case WEARABLE_CMD_LORA_TX_POWER:
      /* SX1262 high-power PA range. */
      if ((wearable_lora_param < -9) || (wearable_lora_param > 22))
      {
        result = WEARABLE_LORA_RESULT_BAD_PARAM;
      }
      else
      {
        LBM_App_SetTxPowerMax(wearable_lora_param);
      }
      break;
    case WEARABLE_CMD_LORA_LEAVE:
      rc = LBM_App_Leave();
      break;
    default: /* WEARABLE_CMD_LORA_STATUS */
      break;
  }
  if (rc != 0)
  {
    result = WEARABLE_LORA_RESULT_MODEM_ERROR;
  }
#else
  result = WEARABLE_LORA_RESULT_NOT_BUILT;
#endif
  WEARABLE_SendLoraStatus(result);
}

#if LBM_APP_ENABLE
/* Modem events (join, TX done, ...), LBM task context. */
static void WEARABLE_LoraEventCallback(void)
{
  WEARABLE_SendLoraStatus(WEARABLE_LORA_RESULT_EVENT);
}
#endif

static void WEARABLE_SendLoraStatus(uint8_t command_result)
{
  wearable_lora_status_t status = {0};

  status.command_result = command_result;
#if LBM_APP_ENABLE
  status.state = (uint8_t)g_lbmState;
  status.tx_power_dbm = LBM_App_GetTxPowerMax();
  status.last_event = g_lbmLastEvent;
  status.tx_done_status = g_lbmTxDoneStatus;
  status.last_rc = (int8_t)g_lbmLastRc;
  status.uplink_count = (uint16_t)g_lbmUplinkCount;
  status.event_count = (uint16_t)g_lbmEventCount;
  status.downlink_count = (uint16_t)g_lbmDownlinkCount;
  if (g_lbmDiag.magic == LBM_DIAG_MAGIC)
  {
    status.init_stage = (uint8_t)g_lbmDiag.init_stage;
    status.panic_count = (uint16_t)g_lbmDiag.panic_count;
    status.busy_timeouts = (uint16_t)g_lbmDiag.busy_timeouts;
    status.spi_errors = (uint16_t)g_lbmDiag.spi_errors;
  }
  if (!LBM_App_IsStarted())
  {
    status.init_stage = 0U;
  }
#else
  status.state = WEARABLE_LORA_STATE_NOT_BUILT;
#endif
  WearableData_EncodeLoraStatus(&status, wearable_debug_snapshot);
  WEARABLE_Debug_data_SendNotification();
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
  WEARABLE_SendStatusIfChanged();

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
    /* A temperature conversion may just have finished or failed. */
    WEARABLE_SendStatusIfChanged();
  }
}

/* PB2 = LIS2DUXS12 RES. Runs once per QVar sample (100 Hz) while measuring,
 * so BLE is only touched when a flag actually changed (wear state, fall). */
static void WEARABLE_MotionInterruptTask(void)
{
  if (!SensorManager_ProcessMotionInterrupt())
  {
    return;
  }

  WEARABLE_ScheduleMotionTimeout();
  WEARABLE_RefreshSensorSnapshot();
  if ((WEARABLE_APP_Context.ConnectionHandle != 0xFFFFU) &&
      (WEARABLE_APP_Context.Sensor_data_Notification_Status == Sensor_data_NOTIFICATION_ON))
  {
    WEARABLE_Sensor_data_SendNotification();
  }
  WEARABLE_SendStatusIfChanged();
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

/* DEVICE_STATUS is otherwise only sent after a command. This pushes it when
 * the state, sensor-ready, error code, power state or flags byte changed on
 * its own (temperature fault appearing/clearing, wear state); the supercap
 * voltage alone does not trigger it. */
static void WEARABLE_SendStatusIfChanged(void)
{
  WEARABLE_RefreshStatusSnapshot();
  if ((memcmp(wearable_status_snapshot, wearable_status_reported, 4U) != 0) ||
      (wearable_status_snapshot[7] != wearable_status_reported[7]))
  {
    WEARABLE_SendStatus();
  }
}

static void WEARABLE_EcgTask(void)
{
  int16_t samples[WEARABLE_ECG_READ_MAX_SAMPLES];
  uint8_t count;
  uint8_t i;
  uint32_t read_start_us;

  /* Also reached from the TX-pool event, so the timer may still be armed. */
  HAL_RADIO_TIMER_StopVirtualTimer(&wearable_ecg_timer);
  if (!SensorManager_IsEcgActive())
  {
    return;
  }

  read_start_us = WEARABLE_EcgDiagMicros();
  count = SensorManager_ReadEcgSamples(samples, WEARABLE_ECG_READ_MAX_SAMPLES);
  WEARABLE_EcgDiagRecord(read_start_us, WEARABLE_EcgDiagMicros(), count);
  for (i = 0U; i < count; i++)
  {
    wearable_ecg_pending.samples[wearable_ecg_pending.sample_count] = samples[i];
    wearable_ecg_pending.sample_count++;
    if (wearable_ecg_pending.sample_count == WEARABLE_ECG_SAMPLES_PER_PACKET)
    {
      WEARABLE_EcgQueuePacket();
    }
  }
#if (ECG_DIAG_NOTIFY_EVERY_N_DRAINS > 1U)
  /* Diag: notify only every N-th drain; packets left by a full TX pool are
   * retried on the next drain (the TX-pool event is ignored in this mode so
   * it cannot trigger an extra FIFO read). */
  wearable_ecg_drains_since_flush++;
  if (wearable_ecg_drains_since_flush >= ECG_DIAG_NOTIFY_EVERY_N_DRAINS)
  {
    WEARABLE_EcgFlushQueue();
    wearable_ecg_drains_since_flush = (wearable_ecg_queue_count != 0U) ?
        (uint8_t)(ECG_DIAG_NOTIFY_EVERY_N_DRAINS - 1U) : 0U;
  }
#else
  WEARABLE_EcgFlushQueue();
#endif

  HAL_RADIO_TIMER_StartVirtualTimer(&wearable_ecg_timer, WEARABLE_ECG_DRAIN_PERIOD_MS);
}

static void WEARABLE_EcgTimerCallback(void *arg)
{
  UNUSED(arg);
  wearable_ecg_diag_fire_us = WEARABLE_EcgDiagMicros();
  wearable_ecg_diag_fired = true;
  UTIL_SEQ_SetTask(1U << CFG_TASK_WEARABLE_ECG_ID, CFG_SEQ_PRIO_0);
}

/* Free-running microseconds (wraps after ~71 min; only differences are used).
 * Task context only: the SysTick interrupt must be able to run. */
static uint32_t WEARABLE_EcgDiagMicros(void)
{
  uint32_t reload = SysTick->LOAD;
  uint32_t ms;
  uint32_t value;

  do
  {
    ms = HAL_GetTick();
    value = SysTick->VAL;
  } while (ms != HAL_GetTick());

  return (ms * 1000U) + (((reload - value) * 1000U) / (reload + 1U));
}

static void WEARABLE_EcgDiagReset(void)
{
  g_ecgDiag.drains = 0U;
  g_ecgDiag.period_us_min = 0xFFFFFFFFUL;
  g_ecgDiag.period_us_max = 0U;
  g_ecgDiag.period_us_sum = 0U;
  g_ecgDiag.period_count = 0U;
  g_ecgDiag.fire_to_read_done_us_min = 0xFFFFFFFFUL;
  g_ecgDiag.fire_to_read_done_us_max = 0U;
  g_ecgDiag.fire_to_read_done_us_sum = 0U;
  g_ecgDiag.read_us_min = 0xFFFFFFFFUL;
  g_ecgDiag.read_us_max = 0U;
  g_ecgDiag.read_us_sum = 0U;
  g_ecgDiag.samples_sum = 0U;
  g_ecgDiag.samples_min = 0xFFU;
  g_ecgDiag.samples_max = 0U;
  g_ecgDiag.queue_max = 0U;
  g_ecgDiag.tx_retry_drains = 0U;
  wearable_ecg_diag_fired = false;
  wearable_ecg_diag_prev_fire_valid = false;
}

/* One drain's numbers. Only timer-triggered drains count: a drain forced by
 * the TX-pool event restarts the timer, so it is tallied separately and the
 * period around it is skipped. */
static void WEARABLE_EcgDiagRecord(uint32_t read_start_us, uint32_t read_done_us, uint8_t count)
{
  uint32_t elapsed;

  if (!wearable_ecg_diag_fired)
  {
    g_ecgDiag.tx_retry_drains++;
    wearable_ecg_diag_prev_fire_valid = false;
    return;
  }
  wearable_ecg_diag_fired = false;

  g_ecgDiag.drains++;
  if (wearable_ecg_diag_prev_fire_valid)
  {
    elapsed = wearable_ecg_diag_fire_us - wearable_ecg_diag_prev_fire_us;
    if (elapsed < g_ecgDiag.period_us_min) { g_ecgDiag.period_us_min = elapsed; }
    if (elapsed > g_ecgDiag.period_us_max) { g_ecgDiag.period_us_max = elapsed; }
    g_ecgDiag.period_us_sum += elapsed;
    g_ecgDiag.period_count++;
  }
  wearable_ecg_diag_prev_fire_us = wearable_ecg_diag_fire_us;
  wearable_ecg_diag_prev_fire_valid = true;

  elapsed = read_done_us - wearable_ecg_diag_fire_us;
  if (elapsed < g_ecgDiag.fire_to_read_done_us_min) { g_ecgDiag.fire_to_read_done_us_min = elapsed; }
  if (elapsed > g_ecgDiag.fire_to_read_done_us_max) { g_ecgDiag.fire_to_read_done_us_max = elapsed; }
  g_ecgDiag.fire_to_read_done_us_sum += elapsed;

  elapsed = read_done_us - read_start_us;
  if (elapsed < g_ecgDiag.read_us_min) { g_ecgDiag.read_us_min = elapsed; }
  if (elapsed > g_ecgDiag.read_us_max) { g_ecgDiag.read_us_max = elapsed; }
  g_ecgDiag.read_us_sum += elapsed;

  g_ecgDiag.samples_sum += count;
  if (count < g_ecgDiag.samples_min) { g_ecgDiag.samples_min = count; }
  if (count > g_ecgDiag.samples_max) { g_ecgDiag.samples_max = count; }
}

static void WEARABLE_StartEcgStream(void)
{
  WEARABLE_StopEcgStream();
  WEARABLE_EcgDiagReset();
  HAL_RADIO_TIMER_StartVirtualTimer(&wearable_ecg_timer, WEARABLE_ECG_DRAIN_PERIOD_MS);
#if (ECG_DIAG_CONN_INTERVAL_MS != 0U)
  /* Diag: move the radio events. Supervision timeout 5 s (0x01F4, as in
   * app_ble.c). The result arrives as HCI_LE_CONNECTION_UPDATE_COMPLETE. */
  if (WEARABLE_APP_Context.ConnectionHandle != 0xFFFFU)
  {
    tBleStatus status = aci_l2cap_connection_parameter_update_req(
        WEARABLE_APP_Context.ConnectionHandle,
        CONN_INT_MS(ECG_DIAG_CONN_INTERVAL_MS),
        CONN_INT_MS(ECG_DIAG_CONN_INTERVAL_MS),
        0x0000U,
        0x01F4U);
    g_ecgDiag.conn_req_status = (uint8_t)status;
  }
#endif
}

static void WEARABLE_StopEcgStream(void)
{
  HAL_RADIO_TIMER_StopVirtualTimer(&wearable_ecg_timer);
  memset(&wearable_ecg_pending, 0, sizeof(wearable_ecg_pending));
  wearable_ecg_sequence = 0U;
  wearable_ecg_queue_head = 0U;
  wearable_ecg_queue_tail = 0U;
  wearable_ecg_queue_count = 0U;
#if (ECG_DIAG_NOTIFY_EVERY_N_DRAINS > 1U)
  wearable_ecg_drains_since_flush = 0U;
#endif
}

static bool WEARABLE_EcgNotificationsEnabled(void)
{
  return (WEARABLE_APP_Context.ConnectionHandle != 0xFFFFU) &&
         (WEARABLE_APP_Context.Ecg_data_Notification_Status == Ecg_data_NOTIFICATION_ON);
}

/* Seals the pending 9-sample packet. The sequence number advances even when
 * nobody is subscribed, so every gap the phone sees is real data loss. */
static void WEARABLE_EcgQueuePacket(void)
{
  wearable_ecg_pending.sequence_number = wearable_ecg_sequence;
  wearable_ecg_sequence++;

  if (WEARABLE_EcgNotificationsEnabled())
  {
    if (wearable_ecg_queue_count == WEARABLE_ECG_QUEUE_LEN)
    {
      wearable_ecg_queue_tail = (uint8_t)((wearable_ecg_queue_tail + 1U) % WEARABLE_ECG_QUEUE_LEN);
      wearable_ecg_queue_count--;
      wearable_ecg_dropped_packets++;
      g_ecgDiag.ecg_dropped_packets++;
    }
    WearableData_EncodeECG(&wearable_ecg_pending, wearable_ecg_queue[wearable_ecg_queue_head]);
    wearable_ecg_queue_head = (uint8_t)((wearable_ecg_queue_head + 1U) % WEARABLE_ECG_QUEUE_LEN);
    wearable_ecg_queue_count++;
    if (wearable_ecg_queue_count > g_ecgDiag.queue_max)
    {
      g_ecgDiag.queue_max = wearable_ecg_queue_count;
    }
  }
  wearable_ecg_pending.sample_count = 0U;
}

static void WEARABLE_EcgFlushQueue(void)
{
  WEARABLE_Data_t data;

  if (!WEARABLE_EcgNotificationsEnabled())
  {
    wearable_ecg_queue_head = 0U;
    wearable_ecg_queue_tail = 0U;
    wearable_ecg_queue_count = 0U;
    return;
  }

  while (wearable_ecg_queue_count != 0U)
  {
    data.p_Payload = wearable_ecg_queue[wearable_ecg_queue_tail];
    data.Length = WEARABLE_ECG_PAYLOAD_LENGTH;
    if (WEARABLE_NotifyValue(WEARABLE_ECG_DATA, &data,
                             WEARABLE_APP_Context.ConnectionHandle) != BLE_STATUS_SUCCESS)
    {
      /* TX pool full: retried on ACI_GATT_TX_POOL_AVAILABLE or next drain. */
      break;
    }
    wearable_ecg_queue_tail = (uint8_t)((wearable_ecg_queue_tail + 1U) % WEARABLE_ECG_QUEUE_LEN);
    wearable_ecg_queue_count--;
  }
}
/* USER CODE END FD_LOCAL_FUNCTIONS*/
