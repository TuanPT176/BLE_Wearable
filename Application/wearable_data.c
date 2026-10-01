#include "wearable_data.h"

#include <string.h>

void WearableData_EncodeSensor(const wearable_sensor_data_t *data,
                               uint8_t payload[WEARABLE_SENSOR_PAYLOAD_LENGTH])
{
  memset(payload, 0, WEARABLE_SENSOR_PAYLOAD_LENGTH);
  payload[0] = data->heart_rate_bpm;
  payload[1] = data->spo2_percent;
  payload[2] = (uint8_t)((uint16_t)data->temperature_centi_c & 0xFFU);
  payload[3] = (uint8_t)((uint16_t)data->temperature_centi_c >> 8);
  payload[4] = (uint8_t)(data->supercap_mv & 0xFFU);
  payload[5] = (uint8_t)(data->supercap_mv >> 8);
  payload[6] = data->power_state;
  payload[7] = data->flags;
  payload[8] = (uint8_t)((uint16_t)data->accel_x & 0xFFU);
  payload[9] = (uint8_t)((uint16_t)data->accel_x >> 8);
  payload[10] = (uint8_t)((uint16_t)data->accel_y & 0xFFU);
  payload[11] = (uint8_t)((uint16_t)data->accel_y >> 8);
  payload[12] = (uint8_t)((uint16_t)data->accel_z & 0xFFU);
  payload[13] = (uint8_t)((uint16_t)data->accel_z >> 8);
  payload[14] = (uint8_t)((uint16_t)data->qvar_raw & 0xFFU);
  payload[15] = (uint8_t)((uint16_t)data->qvar_raw >> 8);
}

static int16_t WearableData_GetI16(const uint8_t *payload)
{
  return (int16_t)(uint16_t)(payload[0] | ((uint16_t)payload[1] << 8));
}

void WearableData_DecodeSensor(const uint8_t payload[WEARABLE_SENSOR_PAYLOAD_LENGTH],
                               wearable_sensor_data_t *data)
{
  data->heart_rate_bpm = payload[0];
  data->spo2_percent = payload[1];
  data->temperature_centi_c = WearableData_GetI16(&payload[2]);
  data->supercap_mv = (uint16_t)WearableData_GetI16(&payload[4]);
  data->power_state = payload[6];
  data->flags = payload[7];
  data->accel_x = WearableData_GetI16(&payload[8]);
  data->accel_y = WearableData_GetI16(&payload[10]);
  data->accel_z = WearableData_GetI16(&payload[12]);
  data->qvar_raw = WearableData_GetI16(&payload[14]);
}

void WearableData_EncodeStatus(const wearable_device_status_t *status,
                               uint8_t payload[WEARABLE_STATUS_PAYLOAD_LENGTH])
{
  payload[0] = status->measurement_state;
  payload[1] = status->sensor_ready;
  payload[2] = status->error_code;
  payload[3] = status->power_state;
  payload[4] = (uint8_t)(status->supercap_mv & 0xFFU);
  payload[5] = (uint8_t)(status->supercap_mv >> 8);
  payload[6] = status->reset_counter;
  payload[7] = (status->flags & 0xF0U) | (WEARABLE_PROTOCOL_VERSION & 0x0FU);
}

void WearableData_EncodeECG(const wearable_ecg_packet_t *data, uint8_t payload[WEARABLE_ECG_PAYLOAD_LENGTH])
{
  memset(payload, 0, WEARABLE_ECG_PAYLOAD_LENGTH);
  payload[0] = data->sequence_number;
  payload[1] = data->sample_count;
  for (uint8_t i = 0; i < WEARABLE_ECG_SAMPLES_PER_PACKET; i++) {
    payload[2 + i*2] = (uint8_t)((uint16_t)data->samples[i] & 0xFFU);
    payload[3 + i*2] = (uint8_t)((uint16_t)data->samples[i] >> 8);
  }
}

void WearableData_EncodeNFCStatus(const wearable_nfc_status_t *data, uint8_t payload[WEARABLE_NFC_PAYLOAD_LENGTH])
{
  memset(payload, 0, WEARABLE_NFC_PAYLOAD_LENGTH);
  payload[0] = data->nfc_state;
  payload[1] = data->last_event;
  payload[2] = data->ftm_status;
  payload[3] = data->config_result;
  payload[4] = data->recovery_status;
}

void WearableData_EncodeRecovery(const wearable_recovery_packet_t *data, uint8_t payload[WEARABLE_RECOVERY_PAYLOAD_LENGTH])
{
  memset(payload, 0, WEARABLE_RECOVERY_PAYLOAD_LENGTH);
  payload[0] = (uint8_t)(data->sequence_number & 0xFFU);
  payload[1] = (uint8_t)(data->sequence_number >> 8);
  payload[2] = (uint8_t)(data->timestamp & 0xFFU);
  payload[3] = (uint8_t)((data->timestamp >> 8) & 0xFFU);
  payload[4] = (uint8_t)((data->timestamp >> 16) & 0xFFU);
  payload[5] = (uint8_t)((data->timestamp >> 24) & 0xFFU);
  memcpy(&payload[6], data->payload, WEARABLE_SENSOR_PAYLOAD_LENGTH);
  payload[22] = (uint8_t)(data->crc & 0xFFU);
  payload[23] = (uint8_t)(data->crc >> 8);
}

void WearableData_EncodeDebug(const wearable_debug_packet_t *data, uint8_t payload[WEARABLE_DEBUG_PAYLOAD_LENGTH])
{
  memset(payload, 0, WEARABLE_DEBUG_PAYLOAD_LENGTH);
  payload[0] = data->command;
  memcpy(&payload[1], data->params, WEARABLE_DEBUG_PAYLOAD_LENGTH - 1);
}

static void WearableData_PutU16(uint8_t *out, uint16_t value)
{
  out[0] = (uint8_t)(value & 0xFFU);
  out[1] = (uint8_t)(value >> 8);
}

void WearableData_EncodeLoraStatus(const wearable_lora_status_t *data, uint8_t payload[WEARABLE_DEBUG_PAYLOAD_LENGTH])
{
  payload[0] = WEARABLE_DEBUG_LORA_STATUS;
  payload[1] = data->state;
  payload[2] = (uint8_t)data->tx_power_dbm;
  payload[3] = data->last_event;
  payload[4] = data->tx_done_status;
  payload[5] = (uint8_t)data->last_rc;
  payload[6] = data->command_result;
  payload[7] = data->init_stage;
  WearableData_PutU16(&payload[8], data->uplink_count);
  WearableData_PutU16(&payload[10], data->event_count);
  WearableData_PutU16(&payload[12], data->downlink_count);
  WearableData_PutU16(&payload[14], data->panic_count);
  WearableData_PutU16(&payload[16], data->busy_timeouts);
  WearableData_PutU16(&payload[18], data->spi_errors);
}
