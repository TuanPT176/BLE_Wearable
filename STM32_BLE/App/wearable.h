/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    WEARABLE.h
  * @author  MCD Application Team
  * @brief   Header for WEARABLE.c
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef WEARABLE_H
#define WEARABLE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ble_status.h"
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported defines ----------------------------------------------------------*/
/* USER CODE BEGIN ED */

/* USER CODE END ED */

/* Exported types ------------------------------------------------------------*/
typedef enum
{
  WEARABLE_CONTROL,
  WEARABLE_SENSOR_DATA,
  WEARABLE_DEVICE_STATUS,

  /* USER CODE BEGIN Service1_CharOpcode_t */

  /* USER CODE END Service1_CharOpcode_t */

  WEARABLE_CHAROPCODE_LAST
} WEARABLE_CharOpcode_t;

typedef enum
{
  WEARABLE_CONTROL_WRITE_EVT,
  WEARABLE_SENSOR_DATA_READ_EVT,
  WEARABLE_SENSOR_DATA_NOTIFY_ENABLED_EVT,
  WEARABLE_SENSOR_DATA_NOTIFY_DISABLED_EVT,
  WEARABLE_DEVICE_STATUS_READ_EVT,
  WEARABLE_DEVICE_STATUS_NOTIFY_ENABLED_EVT,
  WEARABLE_DEVICE_STATUS_NOTIFY_DISABLED_EVT,

  /* USER CODE BEGIN Service1_OpcodeEvt_t */

  /* USER CODE END Service1_OpcodeEvt_t */

  WEARABLE_BOOT_REQUEST_EVT
} WEARABLE_OpcodeEvt_t;

typedef struct
{
  uint8_t *p_Payload;
  uint8_t Length;

  /* USER CODE BEGIN Service1_Data_t */

  /* USER CODE END Service1_Data_t */

} WEARABLE_Data_t;

typedef struct
{
  WEARABLE_OpcodeEvt_t       EvtOpcode;
  WEARABLE_Data_t             DataTransfered;
  uint16_t                ConnectionHandle;
  uint16_t                AttributeHandle;
  uint8_t                 ServiceInstance;

  /* USER CODE BEGIN Service1_NotificationEvt_t */

  /* USER CODE END Service1_NotificationEvt_t */

} WEARABLE_NotificationEvt_t;

/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* External variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Exported macros -----------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions ------------------------------------------------------- */
void WEARABLE_Init(void);
void WEARABLE_Notification(WEARABLE_NotificationEvt_t *p_Notification);
tBleStatus WEARABLE_UpdateValue(WEARABLE_CharOpcode_t CharOpcode, WEARABLE_Data_t *pData);
tBleStatus WEARABLE_NotifyValue(WEARABLE_CharOpcode_t CharOpcode, WEARABLE_Data_t *pData, uint16_t ConnectionHandle);
/* USER CODE BEGIN EF */

/* USER CODE END EF */

#ifdef __cplusplus
}
#endif

#endif /*WEARABLE_H */
