#include "nfc_config.h"
#include "nfc_io.h"
#include "../../Drivers/ST25DV/st25dv.h"
#include <string.h>

#define NFC_CONFIG_CRC_OFFSET (NFC_CONFIG_STORED_SIZE - 2)

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

static void Config_PutU16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value & 0xFFU);
    out[1] = (uint8_t)(value >> 8);
}

static uint16_t Config_GetU16(const uint8_t *in)
{
    return (uint16_t)(in[0] | ((uint16_t)in[1] << 8));
}

void NFC_Config_Encode(const NFC_Config_t *config, uint8_t out[NFC_CONFIG_STORED_SIZE])
{
    memset(out, 0, NFC_CONFIG_STORED_SIZE);
    out[0] = config->version;
    Config_PutU16(&out[2], config->hr_interval_s);
    Config_PutU16(&out[4], config->spo2_interval_s);
    Config_PutU16(&out[6], config->temp_interval_s);
    Config_PutU16(&out[8], config->ble_interval_ms);
    out[10] = config->spo2_threshold;
    Config_PutU16(&out[12], (uint16_t)config->temp_threshold_centi_c);
    out[14] = config->power_mode;
    Config_PutU16(&out[NFC_CONFIG_CRC_OFFSET], Calculate_CRC16(out, NFC_CONFIG_CRC_OFFSET));
}

bool NFC_Config_Decode(const uint8_t in[NFC_CONFIG_STORED_SIZE], NFC_Config_t *config)
{
    config->version = in[0];
    config->hr_interval_s = Config_GetU16(&in[2]);
    config->spo2_interval_s = Config_GetU16(&in[4]);
    config->temp_interval_s = Config_GetU16(&in[6]);
    config->ble_interval_ms = Config_GetU16(&in[8]);
    config->spo2_threshold = in[10];
    config->temp_threshold_centi_c = (int16_t)Config_GetU16(&in[12]);
    config->power_mode = in[14];

    return Config_GetU16(&in[NFC_CONFIG_CRC_OFFSET]) == Calculate_CRC16(in, NFC_CONFIG_CRC_OFFSET);
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
}

bool NFC_Config_Validate(void)
{
    return nfc_config.version == NFC_CONFIG_VERSION;
}

void NFC_Config_Load(void)
{
    uint8_t stored[NFC_CONFIG_STORED_SIZE];

    if (NFC_IO_ReadUserMemory(NFC_CONFIG_EEPROM_ADDR, stored, NFC_CONFIG_STORED_SIZE) == 0)
    {
        if (!NFC_Config_Decode(stored, &nfc_config) || !NFC_Config_Validate())
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
    uint8_t stored[NFC_CONFIG_STORED_SIZE];

    NFC_Config_Encode(&nfc_config, stored);

    if (NFC_IO_WriteUserMemory(NFC_CONFIG_EEPROM_ADDR, stored, NFC_CONFIG_STORED_SIZE) == 0)
    {
        return true;
    }
    return false;
}

void NFC_Config_Init(void)
{
    NFC_Config_Load();
}
