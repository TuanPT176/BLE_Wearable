#ifndef NFC_CONFIG_H
#define NFC_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define NFC_CONFIG_VERSION 1
#define NFC_CONFIG_EEPROM_ADDR 0x0000

typedef struct
{
    uint8_t  version;
    
    uint16_t hr_interval_s;
    uint16_t spo2_interval_s;
    uint16_t temp_interval_s;
    
    uint16_t ble_interval_ms;
    
    uint8_t  spo2_threshold;
    int16_t  temp_threshold_centi_c;
    
    uint8_t  power_mode;
    
    uint8_t  reserved[17]; // Pad to 30 bytes to make struct exactly 32 bytes with CRC
    
    uint16_t crc16;
} NFC_Config_t;

extern NFC_Config_t nfc_config;

void NFC_Config_Init(void);
void NFC_Config_Load(void);
bool NFC_Config_Save(void);
void NFC_Config_LoadDefault(void);
bool NFC_Config_Validate(void);

#endif /* NFC_CONFIG_H */
