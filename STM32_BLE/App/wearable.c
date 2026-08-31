/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    WEARABLE.c
  * @author  MCD Application Team
  * @brief   WEARABLE definition.
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
#include <app_common.h>
#include "ble.h"
#include "wearable.h"
#include "wearable_app.h"
#include "ble_evt.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/

/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

typedef struct{
  uint16_t  WearableSvcHdle;				/**< Wearable Service Handle */
  uint16_t  ControlCharHdle;			/**< CONTROL Characteristic Handle */
  uint16_t  Sensor_DataCharHdle;			/**< SENSOR_DATA Characteristic Handle */
  uint16_t  Device_StatusCharHdle;			/**< DEVICE_STATUS Characteristic Handle */
  uint16_t  Nfc_DataCharHdle;			/**< NFC_DATA Characteristic Handle */
  uint16_t  Ecg_DataCharHdle;			/**< ECG_DATA Characteristic Handle */
  uint16_t  Debug_DataCharHdle;			/**< DEBUG_DATA Characteristic Handle */
  uint16_t  Recovery_DataCharHdle;			/**< RECOVERY_DATA Characteristic Handle */
/* USER CODE BEGIN Context */
/* USER CODE END Context */
}WEARABLE_Context_t;

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* External variables --------------------------------------------------------*/

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private macros ------------------------------------------------------------*/
#define CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET        2
#define CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET             1
#define CONTROL_SIZE        8	/* ControlCommand Characteristic size */
#define SENSOR_DATA_SIZE        16	/* SENSOR DATA Characteristic size */
#define DEVICE_STATUS_SIZE        8	/* DEVICE STATUS Characteristic size */
#define NFC_DATA_SIZE        20	/* NFC DATA Characteristic size */
#define ECG_DATA_SIZE        20	/* ECG DATA Characteristic size */
#define DEBUG_DATA_SIZE        20	/* DEBUG DATA Characteristic size */
#define RECOVERY_DATA_SIZE        24	/* RECOVERY DATA Characteristic size */
/* USER CODE BEGIN PM */
#define NFC_DATA_SIZE 20
#define ECG_DATA_SIZE 20
#define DEBUG_DATA_SIZE 20
#define RECOVERY_DATA_SIZE 24
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

static WEARABLE_Context_t WEARABLE_Context;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/
/* USER CODE BEGIN PFD */

/* USER CODE END PFD */

/* Private functions ----------------------------------------------------------*/

/*
 * UUIDs for WearableHealthService service
 */
#define WEARABLE_UUID			0x8f,0xe5,0xb3,0xd5,0x2e,0x7f,0x4a,0x98,0x2a,0x48,0x7a,0xcc,0x40,0xfe,0x00,0x00
#define CONTROL_UUID			0x19,0xed,0x82,0xae,0xed,0x21,0x4c,0x9d,0x41,0x45,0x22,0x8e,0x41,0xfe,0x00,0x00
#define SENSOR_DATA_UUID			0x19,0xed,0x82,0xae,0xed,0x21,0x4c,0x9d,0x41,0x45,0x22,0x8e,0x42,0xfe,0x00,0x00
#define DEVICE_STATUS_UUID			0x19,0xed,0x82,0xae,0xed,0x21,0x4c,0x9d,0x41,0x45,0x22,0x8e,0x43,0xfe,0x00,0x00
#define NFC_DATA_UUID			0x19,0xed,0x82,0xae,0xed,0x21,0x4c,0x9d,0x41,0x45,0x22,0x8e,0x44,0xfe,0x00,0x00
#define ECG_DATA_UUID			0x19,0xed,0x82,0xae,0xed,0x21,0x4c,0x9d,0x41,0x45,0x22,0x8e,0x45,0xfe,0x00,0x00
#define DEBUG_DATA_UUID			0x19,0xed,0x82,0xae,0xed,0x21,0x4c,0x9d,0x41,0x45,0x22,0x8e,0x46,0xfe,0x00,0x00
#define RECOVERY_DATA_UUID			0x19,0xed,0x82,0xae,0xed,0x21,0x4c,0x9d,0x41,0x45,0x22,0x8e,0x47,0xfe,0x00,0x00

BLE_GATT_SRV_CCCD_DECLARE(sensor_data, CFG_BLE_NUM_RADIO_TASKS, BLE_GATT_SRV_CCCD_PERM_DEFAULT,
                          BLE_GATT_SRV_OP_MODIFIED_EVT_ENABLE_FLAG);
BLE_GATT_SRV_CCCD_DECLARE(device_status, CFG_BLE_NUM_RADIO_TASKS, BLE_GATT_SRV_CCCD_PERM_DEFAULT,
                          BLE_GATT_SRV_OP_MODIFIED_EVT_ENABLE_FLAG);
BLE_GATT_SRV_CCCD_DECLARE(nfc_data, CFG_BLE_NUM_RADIO_TASKS, BLE_GATT_SRV_CCCD_PERM_DEFAULT,
                          BLE_GATT_SRV_OP_MODIFIED_EVT_ENABLE_FLAG);
BLE_GATT_SRV_CCCD_DECLARE(ecg_data, CFG_BLE_NUM_RADIO_TASKS, BLE_GATT_SRV_CCCD_PERM_DEFAULT,
                          BLE_GATT_SRV_OP_MODIFIED_EVT_ENABLE_FLAG);
BLE_GATT_SRV_CCCD_DECLARE(debug_data, CFG_BLE_NUM_RADIO_TASKS, BLE_GATT_SRV_CCCD_PERM_DEFAULT,
                          BLE_GATT_SRV_OP_MODIFIED_EVT_ENABLE_FLAG);
BLE_GATT_SRV_CCCD_DECLARE(recovery_data, CFG_BLE_NUM_RADIO_TASKS, BLE_GATT_SRV_CCCD_PERM_DEFAULT,
                          BLE_GATT_SRV_OP_MODIFIED_EVT_ENABLE_FLAG);

/* USER CODE BEGIN DESCRIPTORS DECLARATION */

/* USER CODE END DESCRIPTORS DECLARATION */

uint8_t control_val_buffer[CONTROL_SIZE];

static ble_gatt_val_buffer_def_t control_val_buffer_def = {
  .op_flags = BLE_GATT_SRV_OP_MODIFIED_EVT_ENABLE_FLAG,
  .val_len = CONTROL_SIZE,
  .buffer_len = sizeof(control_val_buffer),
  .buffer_p = control_val_buffer
};

uint8_t sensor_data_val_buffer[SENSOR_DATA_SIZE];

static ble_gatt_val_buffer_def_t sensor_data_val_buffer_def = {
  .op_flags = BLE_GATT_SRV_OP_MODIFIED_EVT_ENABLE_FLAG,
  .val_len = SENSOR_DATA_SIZE,
  .buffer_len = sizeof(sensor_data_val_buffer),
  .buffer_p = sensor_data_val_buffer
};

uint8_t device_status_val_buffer[DEVICE_STATUS_SIZE];

static ble_gatt_val_buffer_def_t device_status_val_buffer_def = {
  .op_flags = BLE_GATT_SRV_OP_MODIFIED_EVT_ENABLE_FLAG,
  .val_len = DEVICE_STATUS_SIZE,
  .buffer_len = sizeof(device_status_val_buffer),
  .buffer_p = device_status_val_buffer
};

uint8_t nfc_data_val_buffer[NFC_DATA_SIZE];

static ble_gatt_val_buffer_def_t nfc_data_val_buffer_def = {
  .op_flags = BLE_GATT_SRV_OP_MODIFIED_EVT_ENABLE_FLAG,
  .val_len = NFC_DATA_SIZE,
  .buffer_len = sizeof(nfc_data_val_buffer),
  .buffer_p = nfc_data_val_buffer
};

uint8_t ecg_data_val_buffer[ECG_DATA_SIZE];

static ble_gatt_val_buffer_def_t ecg_data_val_buffer_def = {
  .op_flags = BLE_GATT_SRV_OP_MODIFIED_EVT_ENABLE_FLAG,
  .val_len = ECG_DATA_SIZE,
  .buffer_len = sizeof(ecg_data_val_buffer),
  .buffer_p = ecg_data_val_buffer
};

uint8_t debug_data_val_buffer[DEBUG_DATA_SIZE];

static ble_gatt_val_buffer_def_t debug_data_val_buffer_def = {
  .op_flags = BLE_GATT_SRV_OP_MODIFIED_EVT_ENABLE_FLAG,
  .val_len = DEBUG_DATA_SIZE,
  .buffer_len = sizeof(debug_data_val_buffer),
  .buffer_p = debug_data_val_buffer
};

uint8_t recovery_data_val_buffer[RECOVERY_DATA_SIZE];

static ble_gatt_val_buffer_def_t recovery_data_val_buffer_def = {
  .op_flags = BLE_GATT_SRV_OP_MODIFIED_EVT_ENABLE_FLAG,
  .val_len = RECOVERY_DATA_SIZE,
  .buffer_len = sizeof(recovery_data_val_buffer),
  .buffer_p = recovery_data_val_buffer
};

/* WearableHealthService service SENSOR_DATA (notification) DEVICE_STATUS (notification) NFC_DATA (notification) ECG_DATA (notification) DEBUG_DATA (notification) RECOVERY_DATA (notification) characteristics definition */
static const ble_gatt_chr_def_t wearable_chars[] = {
	{
        .properties = BLE_GATT_SRV_CHAR_PROP_WRITE,
        .permissions = BLE_GATT_SRV_PERM_NONE,
        .min_key_size = 0x10,
        .uuid = BLE_UUID_INIT_128(CONTROL_UUID),
        .val_buffer_p = &control_val_buffer_def
    },
	{
        .properties = BLE_GATT_SRV_CHAR_PROP_READ | BLE_GATT_SRV_CHAR_PROP_NOTIFY,
        .permissions = BLE_GATT_SRV_PERM_NONE,
        .min_key_size = 0x10,
        .uuid = BLE_UUID_INIT_128(SENSOR_DATA_UUID),
        .descrs = {
            .descrs_p = &BLE_GATT_SRV_CCCD_DEF_NAME(sensor_data),
            .descr_count = 1U,
        },
        .val_buffer_p = &sensor_data_val_buffer_def
    },
	{
        .properties = BLE_GATT_SRV_CHAR_PROP_READ | BLE_GATT_SRV_CHAR_PROP_NOTIFY,
        .permissions = BLE_GATT_SRV_PERM_NONE,
        .min_key_size = 0x10,
        .uuid = BLE_UUID_INIT_128(DEVICE_STATUS_UUID),
        .descrs = {
            .descrs_p = &BLE_GATT_SRV_CCCD_DEF_NAME(device_status),
            .descr_count = 1U,
        },
        .val_buffer_p = &device_status_val_buffer_def
    },
	{
        .properties = BLE_GATT_SRV_CHAR_PROP_READ | BLE_GATT_SRV_CHAR_PROP_NOTIFY,
        .permissions = BLE_GATT_SRV_PERM_NONE,
        .min_key_size = 0x10,
        .uuid = BLE_UUID_INIT_128(NFC_DATA_UUID),
        .descrs = {
            .descrs_p = &BLE_GATT_SRV_CCCD_DEF_NAME(nfc_data),
            .descr_count = 1U,
        },
        .val_buffer_p = &nfc_data_val_buffer_def
    },
	{
        .properties = BLE_GATT_SRV_CHAR_PROP_READ | BLE_GATT_SRV_CHAR_PROP_NOTIFY,
        .permissions = BLE_GATT_SRV_PERM_NONE,
        .min_key_size = 0x10,
        .uuid = BLE_UUID_INIT_128(ECG_DATA_UUID),
        .descrs = {
            .descrs_p = &BLE_GATT_SRV_CCCD_DEF_NAME(ecg_data),
            .descr_count = 1U,
        },
        .val_buffer_p = &ecg_data_val_buffer_def
    },
	{
        .properties = BLE_GATT_SRV_CHAR_PROP_READ | BLE_GATT_SRV_CHAR_PROP_WRITE | BLE_GATT_SRV_CHAR_PROP_NOTIFY,
        .permissions = BLE_GATT_SRV_PERM_NONE,
        .min_key_size = 0x10,
        .uuid = BLE_UUID_INIT_128(DEBUG_DATA_UUID),
        .descrs = {
            .descrs_p = &BLE_GATT_SRV_CCCD_DEF_NAME(debug_data),
            .descr_count = 1U,
        },
        .val_buffer_p = &debug_data_val_buffer_def
    },
	{
        .properties = BLE_GATT_SRV_CHAR_PROP_READ | BLE_GATT_SRV_CHAR_PROP_NOTIFY,
        .permissions = BLE_GATT_SRV_PERM_NONE,
        .min_key_size = 0x10,
        .uuid = BLE_UUID_INIT_128(RECOVERY_DATA_UUID),
        .descrs = {
            .descrs_p = &BLE_GATT_SRV_CCCD_DEF_NAME(recovery_data),
            .descr_count = 1U,
        },
        .val_buffer_p = &recovery_data_val_buffer_def
    },
};

/* WearableHealthService service definition */
static const ble_gatt_srv_def_t wearable_service = {
   .type = BLE_GATT_SRV_PRIMARY_SRV_TYPE,
   .uuid = BLE_UUID_INIT_128(WEARABLE_UUID),
   .chrs = {
       .chrs_p = (ble_gatt_chr_def_t *)wearable_chars,
       .chr_count = 7U,
   },
};

/* USER CODE BEGIN PF */

/* USER CODE END PF */

/**
 * @brief  Event handler
 * @param  p_Event: Address of the buffer holding the p_Event
 * @retval Ack: Return whether the p_Event has been managed or not
 */
static BLEEVT_EvtAckStatus_t WEARABLE_EventHandler(aci_blecore_event *p_evt)
{
  BLEEVT_EvtAckStatus_t return_value = BLEEVT_NoAck;
  aci_gatt_srv_attribute_modified_event_rp0 *p_attribute_modified;
  aci_gatt_srv_write_event_rp0   *p_write;
  aci_gatt_srv_read_event_rp0    *p_read;
  WEARABLE_NotificationEvt_t notification;
  /* USER CODE BEGIN Service1_EventHandler_1 */

  /* USER CODE END Service1_EventHandler_1 */

  switch(p_evt->ecode)
  {
    case ACI_GATT_SRV_ATTRIBUTE_MODIFIED_VSEVT_CODE:
    {
      /* USER CODE BEGIN EVT_BLUE_GATT_ATTRIBUTE_MODIFIED_BEGIN */

      /* USER CODE END EVT_BLUE_GATT_ATTRIBUTE_MODIFIED_BEGIN */
      p_attribute_modified = (aci_gatt_srv_attribute_modified_event_rp0*)p_evt->data;
      notification.ConnectionHandle         = p_attribute_modified->Connection_Handle;
      notification.AttributeHandle          = p_attribute_modified->Attr_Handle;
      notification.DataTransfered.Length    = p_attribute_modified->Attr_Data_Length;
      notification.DataTransfered.p_Payload = p_attribute_modified->Attr_Data;
      if(p_attribute_modified->Attr_Handle == (WEARABLE_Context.Sensor_DataCharHdle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET))
      {
        return_value = BLEEVT_Ack;
        /* USER CODE BEGIN Service1_Char_2 */

        /* USER CODE END Service1_Char_2 */
        switch(p_attribute_modified->Attr_Data[0])
		{
          /* USER CODE BEGIN Service1_Char_2_attribute_modified */

          /* USER CODE END Service1_Char_2_attribute_modified */

          /* Disabled Notification management */
        case (!BLE_GATT_SRV_CCCD_NOTIFICATION):
          /* USER CODE BEGIN Service1_Char_2_Disabled_BEGIN */

          /* USER CODE END Service1_Char_2_Disabled_BEGIN */
          notification.EvtOpcode = WEARABLE_SENSOR_DATA_NOTIFY_DISABLED_EVT;
          WEARABLE_Notification(&notification);
          /* USER CODE BEGIN Service1_Char_2_Disabled_END */

          /* USER CODE END Service1_Char_2_Disabled_END */
          break;

          /* Enabled Notification management */
        case BLE_GATT_SRV_CCCD_NOTIFICATION:
          /* USER CODE BEGIN Service1_Char_2_COMSVC_Notification_BEGIN */

          /* USER CODE END Service1_Char_2_COMSVC_Notification_BEGIN */
          notification.EvtOpcode = WEARABLE_SENSOR_DATA_NOTIFY_ENABLED_EVT;
          WEARABLE_Notification(&notification);
          /* USER CODE BEGIN Service1_Char_2_COMSVC_Notification_END */

          /* USER CODE END Service1_Char_2_COMSVC_Notification_END */
          break;

        default:
          /* USER CODE BEGIN Service1_Char_2_default */

          /* USER CODE END Service1_Char_2_default */
          break;
        }
      }  /* if(p_attribute_modified->Attr_Handle == (WEARABLE_Context.Sensor_DataCharHdle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET))*/

      else if(p_attribute_modified->Attr_Handle == (WEARABLE_Context.Device_StatusCharHdle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET))
      {
        return_value = BLEEVT_Ack;
        /* USER CODE BEGIN Service1_Char_3 */

        /* USER CODE END Service1_Char_3 */
        switch(p_attribute_modified->Attr_Data[0])
		{
          /* USER CODE BEGIN Service1_Char_3_attribute_modified */

          /* USER CODE END Service1_Char_3_attribute_modified */

          /* Disabled Notification management */
        case (!BLE_GATT_SRV_CCCD_NOTIFICATION):
          /* USER CODE BEGIN Service1_Char_3_Disabled_BEGIN */

          /* USER CODE END Service1_Char_3_Disabled_BEGIN */
          notification.EvtOpcode = WEARABLE_DEVICE_STATUS_NOTIFY_DISABLED_EVT;
          WEARABLE_Notification(&notification);
          /* USER CODE BEGIN Service1_Char_3_Disabled_END */

          /* USER CODE END Service1_Char_3_Disabled_END */
          break;

          /* Enabled Notification management */
        case BLE_GATT_SRV_CCCD_NOTIFICATION:
          /* USER CODE BEGIN Service1_Char_3_COMSVC_Notification_BEGIN */

          /* USER CODE END Service1_Char_3_COMSVC_Notification_BEGIN */
          notification.EvtOpcode = WEARABLE_DEVICE_STATUS_NOTIFY_ENABLED_EVT;
          WEARABLE_Notification(&notification);
          /* USER CODE BEGIN Service1_Char_3_COMSVC_Notification_END */

          /* USER CODE END Service1_Char_3_COMSVC_Notification_END */
          break;

        default:
          /* USER CODE BEGIN Service1_Char_3_default */

          /* USER CODE END Service1_Char_3_default */
          break;
        }
      }  /* if(p_attribute_modified->Attr_Handle == (WEARABLE_Context.Device_StatusCharHdle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET))*/

      else if(p_attribute_modified->Attr_Handle == (WEARABLE_Context.Nfc_DataCharHdle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET))
      {
        return_value = BLEEVT_Ack;
        /* USER CODE BEGIN Service1_Char_4 */

        /* USER CODE END Service1_Char_4 */
        switch(p_attribute_modified->Attr_Data[0])
		{
          /* USER CODE BEGIN Service1_Char_4_attribute_modified */

          /* USER CODE END Service1_Char_4_attribute_modified */

          /* Disabled Notification management */
        case (!BLE_GATT_SRV_CCCD_NOTIFICATION):
          /* USER CODE BEGIN Service1_Char_4_Disabled_BEGIN */

          /* USER CODE END Service1_Char_4_Disabled_BEGIN */
          notification.EvtOpcode = WEARABLE_NFC_DATA_NOTIFY_DISABLED_EVT;
          WEARABLE_Notification(&notification);
          /* USER CODE BEGIN Service1_Char_4_Disabled_END */

          /* USER CODE END Service1_Char_4_Disabled_END */
          break;

          /* Enabled Notification management */
        case BLE_GATT_SRV_CCCD_NOTIFICATION:
          /* USER CODE BEGIN Service1_Char_4_COMSVC_Notification_BEGIN */

          /* USER CODE END Service1_Char_4_COMSVC_Notification_BEGIN */
          notification.EvtOpcode = WEARABLE_NFC_DATA_NOTIFY_ENABLED_EVT;
          WEARABLE_Notification(&notification);
          /* USER CODE BEGIN Service1_Char_4_COMSVC_Notification_END */

          /* USER CODE END Service1_Char_4_COMSVC_Notification_END */
          break;

        default:
          /* USER CODE BEGIN Service1_Char_4_default */

          /* USER CODE END Service1_Char_4_default */
          break;
        }
      }  /* if(p_attribute_modified->Attr_Handle == (WEARABLE_Context.Nfc_DataCharHdle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET))*/

      else if(p_attribute_modified->Attr_Handle == (WEARABLE_Context.Ecg_DataCharHdle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET))
      {
        return_value = BLEEVT_Ack;
        /* USER CODE BEGIN Service1_Char_5 */

        /* USER CODE END Service1_Char_5 */
        switch(p_attribute_modified->Attr_Data[0])
		{
          /* USER CODE BEGIN Service1_Char_5_attribute_modified */

          /* USER CODE END Service1_Char_5_attribute_modified */

          /* Disabled Notification management */
        case (!BLE_GATT_SRV_CCCD_NOTIFICATION):
          /* USER CODE BEGIN Service1_Char_5_Disabled_BEGIN */

          /* USER CODE END Service1_Char_5_Disabled_BEGIN */
          notification.EvtOpcode = WEARABLE_ECG_DATA_NOTIFY_DISABLED_EVT;
          WEARABLE_Notification(&notification);
          /* USER CODE BEGIN Service1_Char_5_Disabled_END */

          /* USER CODE END Service1_Char_5_Disabled_END */
          break;

          /* Enabled Notification management */
        case BLE_GATT_SRV_CCCD_NOTIFICATION:
          /* USER CODE BEGIN Service1_Char_5_COMSVC_Notification_BEGIN */

          /* USER CODE END Service1_Char_5_COMSVC_Notification_BEGIN */
          notification.EvtOpcode = WEARABLE_ECG_DATA_NOTIFY_ENABLED_EVT;
          WEARABLE_Notification(&notification);
          /* USER CODE BEGIN Service1_Char_5_COMSVC_Notification_END */

          /* USER CODE END Service1_Char_5_COMSVC_Notification_END */
          break;

        default:
          /* USER CODE BEGIN Service1_Char_5_default */

          /* USER CODE END Service1_Char_5_default */
          break;
        }
      }  /* if(p_attribute_modified->Attr_Handle == (WEARABLE_Context.Ecg_DataCharHdle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET))*/

      else if(p_attribute_modified->Attr_Handle == (WEARABLE_Context.Debug_DataCharHdle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET))
      {
        return_value = BLEEVT_Ack;
        /* USER CODE BEGIN Service1_Char_6 */

        /* USER CODE END Service1_Char_6 */
        switch(p_attribute_modified->Attr_Data[0])
		{
          /* USER CODE BEGIN Service1_Char_6_attribute_modified */

          /* USER CODE END Service1_Char_6_attribute_modified */

          /* Disabled Notification management */
        case (!BLE_GATT_SRV_CCCD_NOTIFICATION):
          /* USER CODE BEGIN Service1_Char_6_Disabled_BEGIN */

          /* USER CODE END Service1_Char_6_Disabled_BEGIN */
          notification.EvtOpcode = WEARABLE_DEBUG_DATA_NOTIFY_DISABLED_EVT;
          WEARABLE_Notification(&notification);
          /* USER CODE BEGIN Service1_Char_6_Disabled_END */

          /* USER CODE END Service1_Char_6_Disabled_END */
          break;

          /* Enabled Notification management */
        case BLE_GATT_SRV_CCCD_NOTIFICATION:
          /* USER CODE BEGIN Service1_Char_6_COMSVC_Notification_BEGIN */

          /* USER CODE END Service1_Char_6_COMSVC_Notification_BEGIN */
          notification.EvtOpcode = WEARABLE_DEBUG_DATA_NOTIFY_ENABLED_EVT;
          WEARABLE_Notification(&notification);
          /* USER CODE BEGIN Service1_Char_6_COMSVC_Notification_END */

          /* USER CODE END Service1_Char_6_COMSVC_Notification_END */
          break;

        default:
          /* USER CODE BEGIN Service1_Char_6_default */

          /* USER CODE END Service1_Char_6_default */
          break;
        }
      }  /* if(p_attribute_modified->Attr_Handle == (WEARABLE_Context.Debug_DataCharHdle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET))*/

      else if(p_attribute_modified->Attr_Handle == (WEARABLE_Context.Recovery_DataCharHdle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET))
      {
        return_value = BLEEVT_Ack;
        /* USER CODE BEGIN Service1_Char_7 */

        /* USER CODE END Service1_Char_7 */
        switch(p_attribute_modified->Attr_Data[0])
		{
          /* USER CODE BEGIN Service1_Char_7_attribute_modified */

          /* USER CODE END Service1_Char_7_attribute_modified */

          /* Disabled Notification management */
        case (!BLE_GATT_SRV_CCCD_NOTIFICATION):
          /* USER CODE BEGIN Service1_Char_7_Disabled_BEGIN */

          /* USER CODE END Service1_Char_7_Disabled_BEGIN */
          notification.EvtOpcode = WEARABLE_RECOVERY_DATA_NOTIFY_DISABLED_EVT;
          WEARABLE_Notification(&notification);
          /* USER CODE BEGIN Service1_Char_7_Disabled_END */

          /* USER CODE END Service1_Char_7_Disabled_END */
          break;

          /* Enabled Notification management */
        case BLE_GATT_SRV_CCCD_NOTIFICATION:
          /* USER CODE BEGIN Service1_Char_7_COMSVC_Notification_BEGIN */

          /* USER CODE END Service1_Char_7_COMSVC_Notification_BEGIN */
          notification.EvtOpcode = WEARABLE_RECOVERY_DATA_NOTIFY_ENABLED_EVT;
          WEARABLE_Notification(&notification);
          /* USER CODE BEGIN Service1_Char_7_COMSVC_Notification_END */

          /* USER CODE END Service1_Char_7_COMSVC_Notification_END */
          break;

        default:
          /* USER CODE BEGIN Service1_Char_7_default */

          /* USER CODE END Service1_Char_7_default */
          break;
        }
      }  /* if(p_attribute_modified->Attr_Handle == (WEARABLE_Context.Recovery_DataCharHdle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET))*/

      else if(p_attribute_modified->Attr_Handle == (WEARABLE_Context.ControlCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))
      {
        return_value = BLEEVT_Ack;

        notification.EvtOpcode = WEARABLE_CONTROL_WRITE_EVT;
        /* USER CODE BEGIN Service1_Char_1_ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE */

        /* USER CODE END Service1_Char_1_ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE */
        WEARABLE_Notification(&notification);
      } /* if(p_attribute_modified->Attr_Handle == (WEARABLE_Context.ControlCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))*/
      else if(p_attribute_modified->Attr_Handle == (WEARABLE_Context.Debug_DataCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))
      {
        return_value = BLEEVT_Ack;

        notification.EvtOpcode = WEARABLE_DEBUG_DATA_WRITE_EVT;
        /* USER CODE BEGIN Service1_Char_6_ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE */

        /* USER CODE END Service1_Char_6_ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE */
        WEARABLE_Notification(&notification);
      } /* if(p_attribute_modified->Attr_Handle == (WEARABLE_Context.Debug_DataCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))*/

      /* USER CODE BEGIN EVT_BLUE_GATT_ATTRIBUTE_MODIFIED_END */

      /* USER CODE END EVT_BLUE_GATT_ATTRIBUTE_MODIFIED_END */
      break;/* ACI_GATT_SRV_ATTRIBUTE_MODIFIED_VSEVT_CODE */
    }
    case ACI_GATT_SRV_READ_VSEVT_CODE :
    {
      /* USER CODE BEGIN EVT_BLUE_GATT_SRV_READ_BEGIN */

      /* USER CODE END EVT_BLUE_GATT_SRV_READ_BEGIN */
      p_read = (aci_gatt_srv_read_event_rp0*)p_evt->data;
	  if(p_read->Attribute_Handle == (WEARABLE_Context.Sensor_DataCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))
	  {
		return_value = BLEEVT_Ack;
		/*USER CODE BEGIN Service1_Char_2_ACI_GATT_SRV_READ_VSEVT_CODE_1 */
		uint16_t value_length;
		const uint8_t *value = WEARABLE_APP_GetLatestSensorData(&value_length);
		uint8_t error_code = BLE_ATT_ERR_NONE;
		if (p_read->Data_Offset > value_length)
		{
		  error_code = BLE_ATT_ERR_INVALID_OFFSET;
		  value_length = 0U;
		  value = NULL;
		}
		else
		{
		  value += p_read->Data_Offset;
		  value_length -= p_read->Data_Offset;
		}
		aci_gatt_srv_resp(p_read->Connection_Handle, p_read->CID,
		                  p_read->Attribute_Handle, error_code,
		                  value_length, (uint8_t *)value);
		/*USER CODE END Service1_Char_2_ACI_GATT_SRV_READ_VSEVT_CODE_1 */

		/*USER CODE BEGIN Service1_Char_2_ACI_GATT_SRV_READ_VSEVT_CODE_2 */

		  /*USER CODE END Service1_Char_2_ACI_GATT_SRV_READ_VSEVT_CODE_2 */
	  } /* if(p_read->Attribute_Handle == (WEARABLE_Context.Sensor_DataCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))*/
	  else if(p_read->Attribute_Handle == (WEARABLE_Context.Device_StatusCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))
	  {
		return_value = BLEEVT_Ack;
		/*USER CODE BEGIN Service1_Char_3_ACI_GATT_SRV_READ_VSEVT_CODE_1 */
		uint16_t value_length;
		const uint8_t *value = WEARABLE_APP_GetLatestDeviceStatus(&value_length);
		uint8_t error_code = BLE_ATT_ERR_NONE;
		if (p_read->Data_Offset > value_length)
		{
		  error_code = BLE_ATT_ERR_INVALID_OFFSET;
		  value_length = 0U;
		  value = NULL;
		}
		else
		{
		  value += p_read->Data_Offset;
		  value_length -= p_read->Data_Offset;
		}
		aci_gatt_srv_resp(p_read->Connection_Handle, p_read->CID,
		                  p_read->Attribute_Handle, error_code,
		                  value_length, (uint8_t *)value);
		/*USER CODE END Service1_Char_3_ACI_GATT_SRV_READ_VSEVT_CODE_1 */

		/*USER CODE BEGIN Service1_Char_3_ACI_GATT_SRV_READ_VSEVT_CODE_2 */

		  /*USER CODE END Service1_Char_3_ACI_GATT_SRV_READ_VSEVT_CODE_2 */
	  } /* if(p_read->Attribute_Handle == (WEARABLE_Context.Device_StatusCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))*/
	  else if(p_read->Attribute_Handle == (WEARABLE_Context.Nfc_DataCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))
	  {
		return_value = BLEEVT_Ack;
		/*USER CODE BEGIN Service1_Char_4_ACI_GATT_SRV_READ_VSEVT_CODE_1 */
#warning user shall call aci_gatt_srv_read_resp() function if allowed
		/*USER CODE END Service1_Char_4_ACI_GATT_SRV_READ_VSEVT_CODE_1 */

		/*USER CODE BEGIN Service1_Char_4_ACI_GATT_SRV_READ_VSEVT_CODE_2 */

		  /*USER CODE END Service1_Char_4_ACI_GATT_SRV_READ_VSEVT_CODE_2 */
	  } /* if(p_read->Attribute_Handle == (WEARABLE_Context.Nfc_DataCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))*/
	  else if(p_read->Attribute_Handle == (WEARABLE_Context.Ecg_DataCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))
	  {
		return_value = BLEEVT_Ack;
		/*USER CODE BEGIN Service1_Char_5_ACI_GATT_SRV_READ_VSEVT_CODE_1 */
#warning user shall call aci_gatt_srv_read_resp() function if allowed
		/*USER CODE END Service1_Char_5_ACI_GATT_SRV_READ_VSEVT_CODE_1 */

		/*USER CODE BEGIN Service1_Char_5_ACI_GATT_SRV_READ_VSEVT_CODE_2 */

		  /*USER CODE END Service1_Char_5_ACI_GATT_SRV_READ_VSEVT_CODE_2 */
	  } /* if(p_read->Attribute_Handle == (WEARABLE_Context.Ecg_DataCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))*/
	  else if(p_read->Attribute_Handle == (WEARABLE_Context.Debug_DataCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))
	  {
		return_value = BLEEVT_Ack;
		/*USER CODE BEGIN Service1_Char_6_ACI_GATT_SRV_READ_VSEVT_CODE_1 */
#warning user shall call aci_gatt_srv_read_resp() function if allowed
		/*USER CODE END Service1_Char_6_ACI_GATT_SRV_READ_VSEVT_CODE_1 */

		/*USER CODE BEGIN Service1_Char_6_ACI_GATT_SRV_READ_VSEVT_CODE_2 */

		  /*USER CODE END Service1_Char_6_ACI_GATT_SRV_READ_VSEVT_CODE_2 */
	  } /* if(p_read->Attribute_Handle == (WEARABLE_Context.Debug_DataCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))*/
	  else if(p_read->Attribute_Handle == (WEARABLE_Context.Recovery_DataCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))
	  {
		return_value = BLEEVT_Ack;
		/*USER CODE BEGIN Service1_Char_7_ACI_GATT_SRV_READ_VSEVT_CODE_1 */
#warning user shall call aci_gatt_srv_read_resp() function if allowed
		/*USER CODE END Service1_Char_7_ACI_GATT_SRV_READ_VSEVT_CODE_1 */

		/*USER CODE BEGIN Service1_Char_7_ACI_GATT_SRV_READ_VSEVT_CODE_2 */

		  /*USER CODE END Service1_Char_7_ACI_GATT_SRV_READ_VSEVT_CODE_2 */
	  } /* if(p_read->Attribute_Handle == (WEARABLE_Context.Recovery_DataCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))*/

      /* USER CODE BEGIN EVT_BLUE_GATT_SRV_READ_END */

      /* USER CODE END EVT_EVT_BLUE_GATT_SRV_READ_END */
      break;/* ACI_GATT_SRV_READ_VSEVT_CODE */
    }
    case ACI_GATT_SRV_WRITE_VSEVT_CODE:
    {
      /* USER CODE BEGIN EVT_BLUE_SRV_GATT_BEGIN */

      /* USER CODE END EVT_BLUE_SRV_GATT_BEGIN */
      p_write = (aci_gatt_srv_write_event_rp0*)p_evt->data;
      if(p_write->Attribute_Handle == (WEARABLE_Context.Debug_DataCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))
      {
        return_value = BLEEVT_Ack;
        /*USER CODE BEGIN Service1_Char_6_ACI_GATT_SRV_WRITE_VSEVT_CODE */
#warning user shall call aci_gatt_srv_write_resp() function if allowed
        /*USER CODE END Service1_Char_6_ACI_GATT_SRV_WRITE_VSEVT_CODE*/
      } /*if(p_write->Attribute_Handle == (WEARABLE_Context.Debug_DataCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))*/

      /* USER CODE BEGIN EVT_BLUE_GATT_SRV_WRITE_END */

      /* USER CODE END EVT_BLUE_GATT_SRV_WRITE_END */
      break;/* ACI_GATT_SRV_WRITE_VSEVT_CODE */
    }
    case ACI_GATT_TX_POOL_AVAILABLE_VSEVT_CODE:
    {
      aci_gatt_tx_pool_available_event_rp0 *p_tx_pool_available_event;
      p_tx_pool_available_event = (aci_gatt_tx_pool_available_event_rp0 *) p_evt->data;
      UNUSED(p_tx_pool_available_event);

      /* USER CODE BEGIN ACI_GATT_TX_POOL_AVAILABLE_VSEVT_CODE */

      /* USER CODE END ACI_GATT_TX_POOL_AVAILABLE_VSEVT_CODE */
      break;/* ACI_GATT_TX_POOL_AVAILABLE_VSEVT_CODE*/
    }
    case ACI_ATT_EXCHANGE_MTU_RESP_VSEVT_CODE:
    {
      aci_att_exchange_mtu_resp_event_rp0 *p_exchange_mtu;
      p_exchange_mtu = (aci_att_exchange_mtu_resp_event_rp0 *)  p_evt->data;
      UNUSED(p_exchange_mtu);

      /* USER CODE BEGIN ACI_ATT_EXCHANGE_MTU_RESP_VSEVT_CODE */

      /* USER CODE END ACI_ATT_EXCHANGE_MTU_RESP_VSEVT_CODE */
      break;/* ACI_ATT_EXCHANGE_MTU_RESP_VSEVT_CODE */
    }
    /* USER CODE BEGIN BLECORE_EVT */

    /* USER CODE END BLECORE_EVT */
  default:
    /* USER CODE BEGIN EVT_DEFAULT */

    /* USER CODE END EVT_DEFAULT */
    break;
  }

  /* USER CODE BEGIN Service1_EventHandler_2 */

  /* USER CODE END Service1_EventHandler_2 */

  return(return_value);
}/* end WEARABLE_EventHandler */

/* Public functions ----------------------------------------------------------*/

/**
 * @brief  Service initialization
 * @param  None
 * @retval None
 */
void WEARABLE_Init(void)
{
  tBleStatus ret = BLE_STATUS_INVALID_PARAMS;
  UNUSED(WEARABLE_Context);

  /* USER CODE BEGIN InitService1Svc_1 */

  /* USER CODE END InitService1Svc_1 */

  /**
   *  Register the event handler to the BLE controller
   */
  BLEEVT_RegisterGattEvtHandler(WEARABLE_EventHandler);

  ret = aci_gatt_srv_add_service((ble_gatt_srv_def_t *)&wearable_service);

  if (ret != BLE_STATUS_SUCCESS)
  {
    APP_DBG_MSG("  Fail   : aci_gatt_srv_add_service command: WEARABLE, error code: 0x%x \n", ret);
  }
  else
  {
    APP_DBG_MSG("  Success: aci_gatt_srv_add_service command: WEARABLE \n");
  }

  WEARABLE_Context.WearableSvcHdle = aci_gatt_srv_get_service_handle((ble_gatt_srv_def_t *) &wearable_service);
  WEARABLE_Context.ControlCharHdle = aci_gatt_srv_get_char_decl_handle((ble_gatt_chr_def_t *)&wearable_chars[0]);
  WEARABLE_Context.Sensor_DataCharHdle = aci_gatt_srv_get_char_decl_handle((ble_gatt_chr_def_t *)&wearable_chars[1]);
  WEARABLE_Context.Device_StatusCharHdle = aci_gatt_srv_get_char_decl_handle((ble_gatt_chr_def_t *)&wearable_chars[2]);
  WEARABLE_Context.Nfc_DataCharHdle = aci_gatt_srv_get_char_decl_handle((ble_gatt_chr_def_t *)&wearable_chars[3]);
  WEARABLE_Context.Ecg_DataCharHdle = aci_gatt_srv_get_char_decl_handle((ble_gatt_chr_def_t *)&wearable_chars[4]);
  WEARABLE_Context.Debug_DataCharHdle = aci_gatt_srv_get_char_decl_handle((ble_gatt_chr_def_t *)&wearable_chars[5]);
  WEARABLE_Context.Recovery_DataCharHdle = aci_gatt_srv_get_char_decl_handle((ble_gatt_chr_def_t *)&wearable_chars[6]);

  /* USER CODE BEGIN InitService1Svc_2 */

  /* USER CODE END InitService1Svc_2 */

  if (ret != BLE_STATUS_SUCCESS)
  {
    APP_DBG_MSG("  Fail registering WEARABLE handlers\n");
  }

  return;
}

/**
 * @brief  Characteristic update
 * @param  CharOpcode: Characteristic identifier
 * @param  pData: pointer to the new data to be written in the characteristic
 *
 */
tBleStatus WEARABLE_UpdateValue(WEARABLE_CharOpcode_t CharOpcode, WEARABLE_Data_t *pData)
{
  tBleStatus ret = BLE_STATUS_SUCCESS;

  /* USER CODE BEGIN Service1_App_Update_Char_1 */

  /* USER CODE END Service1_App_Update_Char_1 */

  switch(CharOpcode)
  {
    case WEARABLE_CONTROL:
      memcpy(control_val_buffer, pData->p_Payload, MIN(pData->Length, sizeof(control_val_buffer)));
      /* USER CODE BEGIN Service1_Char_Value_1*/

      /* USER CODE END Service1_Char_Value_1*/
      break;

    default:
      break;
  }

  /* USER CODE BEGIN Service1_App_Update_Char_2 */

  /* USER CODE END Service1_App_Update_Char_2 */

  return ret;
}

/**
 * @brief  Characteristic notification
 * @param  CharOpcode: Characteristic identifier
 * @param  pData: pointer to the data to be notified to the client
 * @param  ConnectionHandle: connection handle identifying the client to be notified.
 *
 */
tBleStatus WEARABLE_NotifyValue(WEARABLE_CharOpcode_t CharOpcode, WEARABLE_Data_t *pData, uint16_t ConnectionHandle)
{
  tBleStatus ret = BLE_STATUS_INVALID_PARAMS;
  /* USER CODE BEGIN Service1_App_Notify_Char_1 */

  /* USER CODE END Service1_App_Notify_Char_1 */

  switch(CharOpcode)
  {

    case WEARABLE_SENSOR_DATA:
      memcpy(sensor_data_val_buffer, pData->p_Payload, MIN(pData->Length, sizeof(sensor_data_val_buffer)));
      ret = aci_gatt_srv_notify(ConnectionHandle,
                                BLE_GATT_UNENHANCED_ATT_L2CAP_CID,
                                WEARABLE_Context.Sensor_DataCharHdle + 1,
                                GATT_NOTIFICATION,
                                pData->Length, /* charValueLen */
                                (uint8_t *)pData->p_Payload);
      if (ret != BLE_STATUS_SUCCESS)
      {
        APP_DBG_MSG("  Fail   : aci_gatt_srv_notify SENSOR_DATA command, error code: 0x%2X\n", ret);
      }
      else
      {
        APP_DBG_MSG("  Success: aci_gatt_srv_notify SENSOR_DATA command\n");
      }
      /* USER CODE BEGIN Service1_Char_Value_2*/

      /* USER CODE END Service1_Char_Value_2*/
      break;

    case WEARABLE_DEVICE_STATUS:
      memcpy(device_status_val_buffer, pData->p_Payload, MIN(pData->Length, sizeof(device_status_val_buffer)));
      ret = aci_gatt_srv_notify(ConnectionHandle,
                                BLE_GATT_UNENHANCED_ATT_L2CAP_CID,
                                WEARABLE_Context.Device_StatusCharHdle + 1,
                                GATT_NOTIFICATION,
                                pData->Length, /* charValueLen */
                                (uint8_t *)pData->p_Payload);
      if (ret != BLE_STATUS_SUCCESS)
      {
        APP_DBG_MSG("  Fail   : aci_gatt_srv_notify DEVICE_STATUS command, error code: 0x%2X\n", ret);
      }
      else
      {
        APP_DBG_MSG("  Success: aci_gatt_srv_notify DEVICE_STATUS command\n");
      }
      /* USER CODE BEGIN Service1_Char_Value_3*/

      /* USER CODE END Service1_Char_Value_3*/
      break;

    case WEARABLE_NFC_DATA:
      memcpy(nfc_data_val_buffer, pData->p_Payload, MIN(pData->Length, sizeof(nfc_data_val_buffer)));
      ret = aci_gatt_srv_notify(ConnectionHandle,
                                BLE_GATT_UNENHANCED_ATT_L2CAP_CID,
                                WEARABLE_Context.Nfc_DataCharHdle + 1,
                                GATT_NOTIFICATION,
                                pData->Length, /* charValueLen */
                                (uint8_t *)pData->p_Payload);
      if (ret != BLE_STATUS_SUCCESS)
      {
        APP_DBG_MSG("  Fail   : aci_gatt_srv_notify NFC_DATA command, error code: 0x%2X\n", ret);
      }
      else
      {
        APP_DBG_MSG("  Success: aci_gatt_srv_notify NFC_DATA command\n");
      }
      /* USER CODE BEGIN Service1_Char_Value_4*/

      /* USER CODE END Service1_Char_Value_4*/
      break;

    case WEARABLE_ECG_DATA:
      memcpy(ecg_data_val_buffer, pData->p_Payload, MIN(pData->Length, sizeof(ecg_data_val_buffer)));
      ret = aci_gatt_srv_notify(ConnectionHandle,
                                BLE_GATT_UNENHANCED_ATT_L2CAP_CID,
                                WEARABLE_Context.Ecg_DataCharHdle + 1,
                                GATT_NOTIFICATION,
                                pData->Length, /* charValueLen */
                                (uint8_t *)pData->p_Payload);
      if (ret != BLE_STATUS_SUCCESS)
      {
        APP_DBG_MSG("  Fail   : aci_gatt_srv_notify ECG_DATA command, error code: 0x%2X\n", ret);
      }
      else
      {
        APP_DBG_MSG("  Success: aci_gatt_srv_notify ECG_DATA command\n");
      }
      /* USER CODE BEGIN Service1_Char_Value_5*/

      /* USER CODE END Service1_Char_Value_5*/
      break;

    case WEARABLE_DEBUG_DATA:
      memcpy(debug_data_val_buffer, pData->p_Payload, MIN(pData->Length, sizeof(debug_data_val_buffer)));
      ret = aci_gatt_srv_notify(ConnectionHandle,
                                BLE_GATT_UNENHANCED_ATT_L2CAP_CID,
                                WEARABLE_Context.Debug_DataCharHdle + 1,
                                GATT_NOTIFICATION,
                                pData->Length, /* charValueLen */
                                (uint8_t *)pData->p_Payload);
      if (ret != BLE_STATUS_SUCCESS)
      {
        APP_DBG_MSG("  Fail   : aci_gatt_srv_notify DEBUG_DATA command, error code: 0x%2X\n", ret);
      }
      else
      {
        APP_DBG_MSG("  Success: aci_gatt_srv_notify DEBUG_DATA command\n");
      }
      /* USER CODE BEGIN Service1_Char_Value_6*/

      /* USER CODE END Service1_Char_Value_6*/
      break;

    case WEARABLE_RECOVERY_DATA:
      memcpy(recovery_data_val_buffer, pData->p_Payload, MIN(pData->Length, sizeof(recovery_data_val_buffer)));
      ret = aci_gatt_srv_notify(ConnectionHandle,
                                BLE_GATT_UNENHANCED_ATT_L2CAP_CID,
                                WEARABLE_Context.Recovery_DataCharHdle + 1,
                                GATT_NOTIFICATION,
                                pData->Length, /* charValueLen */
                                (uint8_t *)pData->p_Payload);
      if (ret != BLE_STATUS_SUCCESS)
      {
        APP_DBG_MSG("  Fail   : aci_gatt_srv_notify RECOVERY_DATA command, error code: 0x%2X\n", ret);
      }
      else
      {
        APP_DBG_MSG("  Success: aci_gatt_srv_notify RECOVERY_DATA command\n");
      }
      /* USER CODE BEGIN Service1_Char_Value_7*/

      /* USER CODE END Service1_Char_Value_7*/
      break;

    default:
      break;
  }

  /* USER CODE BEGIN Service1_App_Notify_Char_2 */

  /* USER CODE END Service1_App_Notify_Char_2 */

  return ret;
}
