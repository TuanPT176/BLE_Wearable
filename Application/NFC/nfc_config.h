#ifndef NFC_CONFIG_H
#define NFC_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define NFC_CONFIG_VERSION 1
#define NFC_CONFIG_EEPROM_ADDR 0x0000
#define NFC_CONFIG_STORED_SIZE 32

/*
 * Stored layout, used both in the EEPROM and in the NFC mailbox
 * (NFC_CONFIG_STORED_SIZE bytes, little-endian):
 *
 *   0       version
 *   1       reserved (0)
 *   2..3    hr_interval_s
 *   4..5    spo2_interval_s
 *   6..7    temp_interval_s
 *   8..9    ble_interval_ms
 *   10      spo2_threshold
 *   11      reserved (0)
 *   12..13  temp_threshold_centi_c
 *   14      power_mode
 *   15..29  reserved (0)
 *   30..31  CRC-16/MODBUS of bytes 0..29
 *
 * NFC_Config_t is only the RAM view: its size and padding are up to the
 * compiler, so it must never be copied to the tag or the mailbox directly.
 * Go through NFC_Config_Encode()/NFC_Config_Decode().
 */
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
} NFC_Config_t;

extern NFC_Config_t nfc_config;

void NFC_Config_Init(void);
void NFC_Config_Load(void);
bool NFC_Config_Save(void);
void NFC_Config_LoadDefault(void);
bool NFC_Config_Validate(void);

void NFC_Config_Encode(const NFC_Config_t *config, uint8_t out[NFC_CONFIG_STORED_SIZE]);
/* Always fills *config; returns whether the stored CRC matches. */
bool NFC_Config_Decode(const uint8_t in[NFC_CONFIG_STORED_SIZE], NFC_Config_t *config);

#endif /* NFC_CONFIG_H */
