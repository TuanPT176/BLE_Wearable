#ifndef WEARABLE_DATA_H
#define WEARABLE_DATA_H

#include <stdint.h>

#define WEARABLE_SENSOR_PAYLOAD_LENGTH  16U
#define WEARABLE_STATUS_PAYLOAD_LENGTH   8U
#define WEARABLE_PROTOCOL_VERSION       0x01U
#define WEARABLE_TEMPERATURE_INVALID_CENTI_C  INT16_MIN

#define WEARABLE_FLAG_EMERGENCY         0x10U
#define WEARABLE_FLAG_ECG_ACTIVE        0x20U
#define WEARABLE_FLAG_FALL_CANDIDATE    0x08U
#define WEARABLE_FLAG_WEAR_DETECTED     0x40U

typedef struct
{
  uint8_t heart_rate_bpm;
  uint8_t spo2_percent;
  int16_t temperature_centi_c;
  uint16_t supercap_mv;
  uint8_t power_state;
  uint8_t flags;
  int16_t accel_x;
  int16_t accel_y;
  int16_t accel_z;
  int16_t qvar_raw;
} wearable_sensor_data_t;

typedef struct
{
  uint8_t measurement_state;
  uint8_t sensor_ready;
  uint8_t error_code;
  uint8_t power_state;
  uint16_t supercap_mv;
  uint8_t reset_counter;
  uint8_t flags;
} wearable_device_status_t;

void WearableData_EncodeSensor(const wearable_sensor_data_t *data,
                               uint8_t payload[WEARABLE_SENSOR_PAYLOAD_LENGTH]);
void WearableData_EncodeStatus(const wearable_device_status_t *status,
                               uint8_t payload[WEARABLE_STATUS_PAYLOAD_LENGTH]);
/* Inverse of WearableData_EncodeSensor(), for payloads read back from the NFC log */
void WearableData_DecodeSensor(const uint8_t payload[WEARABLE_SENSOR_PAYLOAD_LENGTH],
                               wearable_sensor_data_t *data);


#define WEARABLE_ECG_PAYLOAD_LENGTH      20U
#define WEARABLE_NFC_PAYLOAD_LENGTH      20U
#define WEARABLE_RECOVERY_PAYLOAD_LENGTH 24U
#define WEARABLE_DEBUG_PAYLOAD_LENGTH    20U

#define WEARABLE_ECG_SAMPLES_PER_PACKET  9U

typedef struct
{
  uint8_t sequence_number;
  uint8_t sample_count;
  int16_t samples[WEARABLE_ECG_SAMPLES_PER_PACKET];
} wearable_ecg_packet_t;

typedef struct
{
  uint8_t nfc_state;
  uint8_t last_event;
  uint8_t ftm_status;
  uint8_t config_result;
  uint8_t recovery_status;
} wearable_nfc_status_t;

typedef struct
{
  uint16_t sequence_number;
  uint32_t timestamp;
  uint16_t crc;
  uint8_t payload[WEARABLE_SENSOR_PAYLOAD_LENGTH];
} wearable_recovery_packet_t;

typedef struct
{
  uint8_t command;
  uint8_t params[WEARABLE_DEBUG_PAYLOAD_LENGTH - 1];
} wearable_debug_packet_t;

void WearableData_EncodeECG(const wearable_ecg_packet_t *data, uint8_t payload[WEARABLE_ECG_PAYLOAD_LENGTH]);
void WearableData_EncodeNFCStatus(const wearable_nfc_status_t *data, uint8_t payload[WEARABLE_NFC_PAYLOAD_LENGTH]);
void WearableData_EncodeRecovery(const wearable_recovery_packet_t *data, uint8_t payload[WEARABLE_RECOVERY_PAYLOAD_LENGTH]);
void WearableData_EncodeDebug(const wearable_debug_packet_t *data, uint8_t payload[WEARABLE_DEBUG_PAYLOAD_LENGTH]);

/* DEBUG_DATA (FE46) packet 0x20: LoRaWAN test status, sent after every LoRa
 * CONTROL command (0x0F-0x13) and after every LoRa Basics Modem event. */
#define WEARABLE_DEBUG_LORA_STATUS          0x20U
#define WEARABLE_LORA_STATE_NOT_BUILT       0xFFU /* firmware built with LBM_APP_ENABLE = 0 */

#define WEARABLE_LORA_RESULT_OK             0x00U
#define WEARABLE_LORA_RESULT_NOT_BUILT      0x01U
#define WEARABLE_LORA_RESULT_NOT_JOINED     0x02U
#define WEARABLE_LORA_RESULT_MODEM_ERROR    0x03U /* see last_rc */
#define WEARABLE_LORA_RESULT_BAD_PARAM      0x04U
#define WEARABLE_LORA_RESULT_EVENT          0xFFU /* not a command answer: a modem event */

typedef struct
{
  uint8_t state;            /* LBM_State_t, or WEARABLE_LORA_STATE_NOT_BUILT */
  int8_t tx_power_dbm;      /* SX1262 output power cap */
  uint8_t last_event;       /* smtc_modem_event_type_t of the last event */
  uint8_t tx_done_status;   /* 0 not sent, 1 sent, 2 confirmed */
  int8_t last_rc;           /* last smtc_modem_return_code_t that was not OK */
  uint8_t command_result;   /* WEARABLE_LORA_RESULT_xxx */
  uint8_t init_stage;       /* 0 not started, 1 inside smtc_modem_init, 2 started */
  uint16_t uplink_count;
  uint16_t event_count;
  uint16_t downlink_count;
  uint16_t panic_count;
  uint16_t busy_timeouts;
  uint16_t spi_errors;
} wearable_lora_status_t;

void WearableData_EncodeLoraStatus(const wearable_lora_status_t *data, uint8_t payload[WEARABLE_DEBUG_PAYLOAD_LENGTH]);

#endif /* WEARABLE_DATA_H */

