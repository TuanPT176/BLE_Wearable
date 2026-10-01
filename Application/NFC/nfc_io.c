#include "nfc_io.h"
#include <stdbool.h>
#include <stddef.h>

extern I2C_HandleTypeDef hi2c1;

#define ST25DV_I2C_TIMEOUT 1000

ST25DV_IO_t st25dv_io;
ST25DV_Object_t st25dv_obj;

static int32_t I2C_Init(void)
{
    // I2C is already initialized by MX_I2C1_Init in main.c
    return 0;
}

static int32_t I2C_DeInit(void)
{
    return 0;
}

static uint32_t I2C_GetTick(void)
{
    return HAL_GetTick();
}

static int32_t I2C_Write(uint16_t DevAddr, uint16_t MemAddr, const uint8_t *pData, uint16_t Length)
{
    if (HAL_I2C_Mem_Write(&hi2c1, DevAddr, MemAddr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)pData, Length, ST25DV_I2C_TIMEOUT) == HAL_OK)
    {
        return 0;
    }
    return -1;
}

static int32_t I2C_Read(uint16_t DevAddr, uint16_t MemAddr, uint8_t *pData, uint16_t Length)
{
    if (HAL_I2C_Mem_Read(&hi2c1, DevAddr, MemAddr, I2C_MEMADD_SIZE_16BIT, pData, Length, ST25DV_I2C_TIMEOUT) == HAL_OK)
    {
        return 0;
    }
    return -1;
}

static int32_t I2C_IsReady(uint16_t DevAddr, const uint32_t Trials)
{
    if (HAL_I2C_IsDeviceReady(&hi2c1, DevAddr, Trials, ST25DV_I2C_TIMEOUT) == HAL_OK)
    {
        return 0;
    }
    return -1;
}

int32_t NFC_IO_Init(void)
{
    st25dv_io.Init = I2C_Init;
    st25dv_io.DeInit = I2C_DeInit;
    st25dv_io.IsReady = I2C_IsReady;
    st25dv_io.Write = I2C_Write;
    st25dv_io.Read = I2C_Read;
    st25dv_io.GetTick = I2C_GetTick;

    if (ST25DV_RegisterBusIO(&st25dv_obj, &st25dv_io) != 0) {
        return -1;
    }
    
    if (St25Dv_Drv.Init(&st25dv_obj) != 0) {
        return -1;
    }

    return 0;
}

static bool NFC_IO_UserRangeValid(uint16_t addr, uint16_t length)
{
    return ((uint32_t)addr + length) <= NFC_USER_MEMORY_SIZE;
}

int32_t NFC_IO_ReadUserMemory(uint16_t addr, uint8_t *data, uint16_t length)
{
    if ((data == NULL) || !NFC_IO_UserRangeValid(addr, length))
    {
        return -1;
    }

    return St25Dv_Drv.ReadData(&st25dv_obj, data, addr, length);
}

int32_t NFC_IO_WriteUserMemory(uint16_t addr, const uint8_t *data, uint16_t length)
{
    ST25DV_EN_STATUS mailbox_enabled = ST25DV_DISABLE;
    int32_t ret;

    if ((data == NULL) || !NFC_IO_UserRangeValid(addr, length))
    {
        return -1;
    }

    /* EEPROM writes transit through the fast transfer mode buffer, so the tag
     * NACKs them while the mailbox is enabled (DS10925 section 6.4). Disabling
     * it empties the mailbox: a message not yet read by either side is lost. */
    if (ST25DV_GetMBEN_Dyn(&st25dv_obj, &mailbox_enabled) != 0)
    {
        return -1;
    }
    if ((mailbox_enabled == ST25DV_ENABLE) && (ST25DV_ResetMBEN_Dyn(&st25dv_obj) != 0))
    {
        return -1;
    }

    ret = St25Dv_Drv.WriteData(&st25dv_obj, data, addr, length);

    if (mailbox_enabled == ST25DV_ENABLE)
    {
        ST25DV_SetMBEN_Dyn(&st25dv_obj);
    }

    return ret;
}
