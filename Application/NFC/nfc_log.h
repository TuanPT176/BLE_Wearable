#ifndef NFC_LOG_H
#define NFC_LOG_H

#include <stdint.h>
#include <stdbool.h>
#include "../wearable_data.h"

/*
 * EEPROM map (ST25DV04K user memory, 512 bytes, see NFC_USER_MEMORY_SIZE):
 *
 *   0x0000..0x001F  configuration (nfc_config.h)
 *   0x0020..0x002F  log header
 *   0x0030..0x003F  unused
 *   0x0040..0x01EF  NFC_LOG_MAX_RECORDS records of NFC_LOG_RECORD_SIZE bytes
 *   0x01F0..0x01FF  unused
 *
 * Log header, NFC_LOG_HEADER_SIZE bytes, little-endian:
 *   0..3    base_timestamp
 *   4..5    interval_s
 *   6..7    write_index
 *   8..9    record_count
 *   10..11  newest_sequence
 *   12..13  oldest_sequence
 *   14..15  CRC-16/MODBUS of bytes 0..13
 *
 * Record, NFC_LOG_RECORD_SIZE bytes, little-endian. Same bytes as a
 * RECOVERY_DATA packet (WearableData_EncodeRecovery):
 *   0..1    sequence
 *   2..5    timestamp
 *   6..21   sensor payload (WearableData_EncodeSensor)
 *   22..23  CRC-16/MODBUS of bytes 0..21
 *
 * NFC_LogHeader_t and NFC_SensorRecord_t are only the RAM views: their size
 * and padding are up to the compiler, so they must never be copied to the tag
 * or the mailbox directly.
 */
#define NFC_LOG_HEADER_ADDR 0x0020
#define NFC_LOG_HEADER_SIZE 16
#define NFC_LOG_RECORD_ADDR 0x0040
#define NFC_LOG_RECORD_SIZE 24
#define NFC_LOG_MAX_RECORDS 18

typedef struct
{
    uint32_t base_timestamp;
    uint16_t interval_s;

    uint16_t write_index;
    uint16_t record_count;
    uint16_t newest_sequence;
    uint16_t oldest_sequence;
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
/* record->sequence and record->crc16 are ignored: both are assigned here. */
bool NFC_Log_Add(const NFC_SensorRecord_t *record);
/* index 0 is the oldest record. */
bool NFC_Log_Read(uint16_t index, NFC_SensorRecord_t *record);
bool NFC_Log_ReadStored(uint16_t index, uint8_t stored[NFC_LOG_RECORD_SIZE]);
void NFC_Log_EncodeHeader(uint8_t out[NFC_LOG_HEADER_SIZE]);
uint16_t NFC_Log_Count(void);
void NFC_Log_Clear(void);

#endif /* NFC_LOG_H */
