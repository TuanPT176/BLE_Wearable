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
