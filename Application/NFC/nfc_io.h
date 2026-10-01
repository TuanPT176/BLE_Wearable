#ifndef NFC_IO_H
#define NFC_IO_H

#include "stm32wb0x_hal.h"
#include "../../Drivers/ST25DV/st25dv.h"

/* User memory (EEPROM) of the ST25DV04K: I2C byte addresses 0x0000..0x01FF
 * (DS10925 Table 3). The 64K part ST25DV_Init() also accepts is larger, so
 * everything below this limit exists on either IC. */
#define NFC_USER_MEMORY_SIZE 512U

extern ST25DV_IO_t st25dv_io;
extern ST25DV_Object_t st25dv_obj;

int32_t NFC_IO_Init(void);

/* User memory access. ST25DV_ReadRegister()/ST25DV_WriteRegister() must not be
 * used for application data: they address the system configuration area. */
int32_t NFC_IO_ReadUserMemory(uint16_t addr, uint8_t *data, uint16_t length);
int32_t NFC_IO_WriteUserMemory(uint16_t addr, const uint8_t *data, uint16_t length);

#endif /* NFC_IO_H */
