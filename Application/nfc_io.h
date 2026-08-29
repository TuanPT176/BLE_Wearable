#ifndef NFC_IO_H
#define NFC_IO_H

#include "stm32wb0x_hal.h"
#include "../Drivers/ST25DV/st25dv.h"

extern ST25DV_IO_t st25dv_io;
extern ST25DV_Object_t st25dv_obj;

int32_t NFC_IO_Init(void);

#endif /* NFC_IO_H */
