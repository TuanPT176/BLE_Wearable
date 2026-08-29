#include "nfc_log.h"
#include "nfc_io.h"
#include "../Drivers/ST25DV/st25dv.h"
#include <string.h>

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

static void NFC_Log_SaveHeader(void)
{
    nfc_log_header.crc16 = Log_Calculate_CRC16((uint8_t*)&nfc_log_header, sizeof(NFC_LogHeader_t) - 4); // CRC calculated over first 12 bytes
    ST25DV_WriteRegister(&st25dv_obj, (const uint8_t*)&nfc_log_header, NFC_LOG_HEADER_ADDR, sizeof(NFC_LogHeader_t));
}

void NFC_Log_Init(void)
{
    if (ST25DV_ReadRegister(&st25dv_obj, (uint8_t*)&nfc_log_header, NFC_LOG_HEADER_ADDR, sizeof(NFC_LogHeader_t)) == 0)
    {
        uint16_t calc_crc = Log_Calculate_CRC16((uint8_t*)&nfc_log_header, sizeof(NFC_LogHeader_t) - 4);
        if (calc_crc != nfc_log_header.crc16)
        {
            // Invalid header, clear log
            NFC_Log_Clear();
        }
    }
    else
    {
        NFC_Log_Clear();
    }
}

void NFC_Log_Clear(void)
{
    memset(&nfc_log_header, 0, sizeof(NFC_LogHeader_t));
    nfc_log_header.interval_s = 60; // default
    NFC_Log_SaveHeader();
}

bool NFC_Log_Add(const NFC_SensorRecord_t *record)
{
    if (!record) return false;

    // Calculate EEPROM address for new record
    uint16_t addr = NFC_LOG_RECORD_ADDR + (nfc_log_header.write_index * NFC_LOG_RECORD_SIZE);

    // Write record to EEPROM
    if (ST25DV_WriteRegister(&st25dv_obj, (const uint8_t*)record, addr, NFC_LOG_RECORD_SIZE) != 0)
    {
        return false;
    }

    // Update header
    nfc_log_header.write_index++;
    if (nfc_log_header.write_index >= NFC_LOG_MAX_RECORDS)
    {
        nfc_log_header.write_index = 0; // Wrap around
    }

    if (nfc_log_header.record_count < NFC_LOG_MAX_RECORDS)
    {
        nfc_log_header.record_count++;
    }

    nfc_log_header.sequence++;
    
    // Save updated header
    NFC_Log_SaveHeader();

    return true;
}

bool NFC_Log_Read(uint16_t index, NFC_SensorRecord_t *record)
{
    if (index >= nfc_log_header.record_count || !record)
    {
        return false;
    }

    // Determine oldest record index
    uint16_t read_idx;
    if (nfc_log_header.record_count < NFC_LOG_MAX_RECORDS)
    {
        read_idx = index;
    }
    else
    {
        read_idx = (nfc_log_header.write_index + index) % NFC_LOG_MAX_RECORDS;
    }

    uint16_t addr = NFC_LOG_RECORD_ADDR + (read_idx * NFC_LOG_RECORD_SIZE);

    if (ST25DV_ReadRegister(&st25dv_obj, (uint8_t*)record, addr, NFC_LOG_RECORD_SIZE) != 0)
    {
        return false;
    }

    return true;
}

uint16_t NFC_Log_Count(void)
{
    return nfc_log_header.record_count;
}
