#include "nfc_log.h"
#include "nfc_config.h"
#include "nfc_io.h"
#include "../../Drivers/ST25DV/st25dv.h"
#include <string.h>

#define NFC_LOG_HEADER_CRC_OFFSET (NFC_LOG_HEADER_SIZE - 2)
#define NFC_LOG_RECORD_CRC_OFFSET (NFC_LOG_RECORD_SIZE - 2)

_Static_assert(NFC_CONFIG_EEPROM_ADDR + NFC_CONFIG_STORED_SIZE <= NFC_LOG_HEADER_ADDR,
               "NFC config overlaps the log header");
_Static_assert(NFC_LOG_HEADER_ADDR + NFC_LOG_HEADER_SIZE <= NFC_LOG_RECORD_ADDR,
               "NFC log header overlaps the records");
_Static_assert(NFC_LOG_RECORD_ADDR + (NFC_LOG_MAX_RECORDS * NFC_LOG_RECORD_SIZE) <= NFC_USER_MEMORY_SIZE,
               "NFC log records exceed the ST25DV04K user memory");
_Static_assert(NFC_LOG_RECORD_SIZE == 2 + 4 + WEARABLE_SENSOR_PAYLOAD_LENGTH + 2,
               "NFC log record layout does not match NFC_LOG_RECORD_SIZE");

NFC_LogHeader_t nfc_log_header;

static uint16_t Log_Calculate_CRC16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++)
    {
        crc ^= (uint16_t)data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
            {
                crc = (crc >> 1) ^ 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

static void Log_PutU16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value & 0xFFU);
    out[1] = (uint8_t)(value >> 8);
}

static void Log_PutU32(uint8_t *out, uint32_t value)
{
    Log_PutU16(&out[0], (uint16_t)(value & 0xFFFFU));
    Log_PutU16(&out[2], (uint16_t)(value >> 16));
}

static uint16_t Log_GetU16(const uint8_t *in)
{
    return (uint16_t)(in[0] | ((uint16_t)in[1] << 8));
}

static uint32_t Log_GetU32(const uint8_t *in)
{
    return (uint32_t)Log_GetU16(&in[0]) | ((uint32_t)Log_GetU16(&in[2]) << 16);
}

static void Log_EncodeHeader(const NFC_LogHeader_t *header, uint8_t out[NFC_LOG_HEADER_SIZE])
{
    Log_PutU32(&out[0], header->base_timestamp);
    Log_PutU16(&out[4], header->interval_s);
    Log_PutU16(&out[6], header->write_index);
    Log_PutU16(&out[8], header->record_count);
    Log_PutU16(&out[10], header->newest_sequence);
    Log_PutU16(&out[12], header->oldest_sequence);
    Log_PutU16(&out[NFC_LOG_HEADER_CRC_OFFSET], Log_Calculate_CRC16(out, NFC_LOG_HEADER_CRC_OFFSET));
}

static bool Log_DecodeHeader(const uint8_t in[NFC_LOG_HEADER_SIZE], NFC_LogHeader_t *header)
{
    if (Log_GetU16(&in[NFC_LOG_HEADER_CRC_OFFSET]) != Log_Calculate_CRC16(in, NFC_LOG_HEADER_CRC_OFFSET))
    {
        return false;
    }

    header->base_timestamp = Log_GetU32(&in[0]);
    header->interval_s = Log_GetU16(&in[4]);
    header->write_index = Log_GetU16(&in[6]);
    header->record_count = Log_GetU16(&in[8]);
    header->newest_sequence = Log_GetU16(&in[10]);
    header->oldest_sequence = Log_GetU16(&in[12]);

    /* A header written for another NFC_LOG_MAX_RECORDS would index outside the record area */
    return (header->write_index < NFC_LOG_MAX_RECORDS) && (header->record_count <= NFC_LOG_MAX_RECORDS);
}

static bool NFC_Log_SaveHeader(const NFC_LogHeader_t *header)
{
    uint8_t stored[NFC_LOG_HEADER_SIZE];

    Log_EncodeHeader(header, stored);
    return NFC_IO_WriteUserMemory(NFC_LOG_HEADER_ADDR, stored, NFC_LOG_HEADER_SIZE) == 0;
}

void NFC_Log_EncodeHeader(uint8_t out[NFC_LOG_HEADER_SIZE])
{
    Log_EncodeHeader(&nfc_log_header, out);
}

void NFC_Log_Init(void)
{
    uint8_t stored[NFC_LOG_HEADER_SIZE];

    if ((NFC_IO_ReadUserMemory(NFC_LOG_HEADER_ADDR, stored, NFC_LOG_HEADER_SIZE) != 0) ||
        !Log_DecodeHeader(stored, &nfc_log_header))
    {
        NFC_Log_Clear();
    }
}

void NFC_Log_Clear(void)
{
    memset(&nfc_log_header, 0, sizeof(NFC_LogHeader_t));
    nfc_log_header.interval_s = 60; // default
    NFC_Log_SaveHeader(&nfc_log_header);
}

bool NFC_Log_Add(const NFC_SensorRecord_t *record)
{
    if (!record) return false;

    uint8_t stored[NFC_LOG_RECORD_SIZE];
    NFC_LogHeader_t new_header = nfc_log_header;

    // Sequence starts at 1 after a clear
    new_header.newest_sequence = (uint16_t)(nfc_log_header.newest_sequence + 1U);

    Log_PutU16(&stored[0], new_header.newest_sequence);
    Log_PutU32(&stored[2], record->timestamp);
    WearableData_EncodeSensor(&record->sensor_data, &stored[6]);
    Log_PutU16(&stored[NFC_LOG_RECORD_CRC_OFFSET], Log_Calculate_CRC16(stored, NFC_LOG_RECORD_CRC_OFFSET));

    uint16_t addr = NFC_LOG_RECORD_ADDR + (nfc_log_header.write_index * NFC_LOG_RECORD_SIZE);

    if (NFC_IO_WriteUserMemory(addr, stored, NFC_LOG_RECORD_SIZE) != 0)
    {
        return false;
    }

    new_header.write_index++;
    if (new_header.write_index >= NFC_LOG_MAX_RECORDS)
    {
        new_header.write_index = 0;
    }

    if (new_header.record_count < NFC_LOG_MAX_RECORDS)
    {
        new_header.record_count++;
    }

    new_header.oldest_sequence = (uint16_t)(new_header.newest_sequence - new_header.record_count + 1U);

    // The RAM header only advances once the tag holds it, so both stay in step
    if (!NFC_Log_SaveHeader(&new_header))
    {
        return false;
    }
    nfc_log_header = new_header;

    return true;
}

bool NFC_Log_ReadStored(uint16_t index, uint8_t stored[NFC_LOG_RECORD_SIZE])
{
    if (index >= nfc_log_header.record_count || !stored)
    {
        return false;
    }

    // The oldest record sits record_count slots behind the write position
    uint16_t read_idx = (uint16_t)((nfc_log_header.write_index + NFC_LOG_MAX_RECORDS
                                    - nfc_log_header.record_count + index) % NFC_LOG_MAX_RECORDS);

    uint16_t addr = NFC_LOG_RECORD_ADDR + (read_idx * NFC_LOG_RECORD_SIZE);

    if (NFC_IO_ReadUserMemory(addr, stored, NFC_LOG_RECORD_SIZE) != 0)
    {
        return false;
    }

    // Check CRC
    if (Log_GetU16(&stored[NFC_LOG_RECORD_CRC_OFFSET]) != Log_Calculate_CRC16(stored, NFC_LOG_RECORD_CRC_OFFSET))
    {
        return false;
    }

    // A slot rewritten by an NFC_Log_Add() whose header update failed holds another sequence
    if (Log_GetU16(&stored[0]) != (uint16_t)(nfc_log_header.oldest_sequence + index))
    {
        return false;
    }

    return true;
}

bool NFC_Log_Read(uint16_t index, NFC_SensorRecord_t *record)
{
    uint8_t stored[NFC_LOG_RECORD_SIZE];

    if (!record || !NFC_Log_ReadStored(index, stored))
    {
        return false;
    }

    record->sequence = Log_GetU16(&stored[0]);
    record->timestamp = Log_GetU32(&stored[2]);
    WearableData_DecodeSensor(&stored[6], &record->sensor_data);
    record->crc16 = Log_GetU16(&stored[NFC_LOG_RECORD_CRC_OFFSET]);

    return true;
}

uint16_t NFC_Log_Count(void)
{
    return nfc_log_header.record_count;
}
