#include "nfc_io.h"

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
