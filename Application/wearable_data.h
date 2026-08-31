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

#endif /* WEARABLE_DATA_H */

