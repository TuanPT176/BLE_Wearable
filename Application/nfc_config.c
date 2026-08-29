#include "nfc_config.h"
#include "nfc_io.h"
#include "../Drivers/ST25DV/st25dv.h"
#include <string.h>

NFC_Config_t nfc_config;

static uint16_t Calculate_CRC16(const uint8_t *data, uint16_t length)
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

void NFC_Config_LoadDefault(void)
{
    memset(&nfc_config, 0, sizeof(NFC_Config_t));
    nfc_config.version = NFC_CONFIG_VERSION;
    nfc_config.hr_interval_s = 60;
    nfc_config.spo2_interval_s = 60;
    nfc_config.temp_interval_s = 60;
    nfc_config.ble_interval_ms = 1000;
    nfc_config.spo2_threshold = 90;
    nfc_config.temp_threshold_centi_c = 3800; // 38.00 C
    nfc_config.power_mode = 0; // Normal
    
    nfc_config.crc16 = Calculate_CRC16((uint8_t*)&nfc_config, sizeof(NFC_Config_t) - 2);
}

bool NFC_Config_Validate(void)
{
    if (nfc_config.version != NFC_CONFIG_VERSION)
    {
        return false;
    }
    
    uint16_t calculated_crc = Calculate_CRC16((uint8_t*)&nfc_config, sizeof(NFC_Config_t) - 2);
    if (calculated_crc != nfc_config.crc16)
    {
        return false;
    }
    
    return true;
}

void NFC_Config_Load(void)
{
    if (ST25DV_ReadRegister(&st25dv_obj, (uint8_t*)&nfc_config, NFC_CONFIG_EEPROM_ADDR, sizeof(NFC_Config_t)) == 0)
    {
        if (!NFC_Config_Validate())
        {
            NFC_Config_LoadDefault();
            NFC_Config_Save();
        }
    }
    else
    {
        NFC_Config_LoadDefault();
    }
}

bool NFC_Config_Save(void)
{
    nfc_config.crc16 = Calculate_CRC16((uint8_t*)&nfc_config, sizeof(NFC_Config_t) - 2);
    
    if (ST25DV_WriteRegister(&st25dv_obj, (const uint8_t*)&nfc_config, NFC_CONFIG_EEPROM_ADDR, sizeof(NFC_Config_t)) == 0)
    {
        return true;
    }
    return false;
}

void NFC_Config_Init(void)
{
    NFC_Config_Load();
}
