#ifndef NFC_LOG_H
#define NFC_LOG_H

#include <stdint.h>
#include <stdbool.h>
#include "wearable_data.h"

#define NFC_LOG_HEADER_ADDR 0x0020
#define NFC_LOG_RECORD_ADDR 0x0040
#define NFC_LOG_RECORD_SIZE 24
#define NFC_LOG_MAX_RECORDS 19

typedef struct
{
    uint32_t base_timestamp;
    uint16_t interval_s;

    uint16_t write_index;
    uint16_t record_count;
    uint16_t newest_sequence;
    uint16_t oldest_sequence;

    uint16_t crc16;
    uint8_t  reserved[2]; // Pad to 16 bytes
} NFC_LogHeader_t;

typedef struct
{
    uint16_t sequence;
    uint32_t timestamp;
    wearable_sensor_data_t sensor_data;
    uint16_t crc16;
} NFC_SensorRecord_t;

extern NFC_LogHeader_t nfc_log_header;

void NFC_Log_Init(void);
bool NFC_Log_Add(const NFC_SensorRecord_t *record);
bool NFC_Log_Read(uint16_t index, NFC_SensorRecord_t *record);
uint16_t NFC_Log_Count(void);
void NFC_Log_Clear(void);

#endif /* NFC_LOG_H */
