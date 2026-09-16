/**
 * STM32WB09 SPI3/GPIO glue implementing Semtech's sx126x_hal.h interface
 * (see Drivers/SX1262/sx126x_hal.h for the contract this file must satisfy).
 *
 * Pin map (BLE_Wearable_GATT.ioc): NSS=PA9(SX_NSS), SCK=PB3, MOSI=PA11,
 * MISO=PA8, BUSY=PB14(input, polled), NRESET=PB15(SX_RESET, open-drain),
 * DIO1=PA1 serviced by GPIOA_IRQHandler (see stm32wb0x_it.c).
 */

#include "sx126x_hal.h"
#include "main.h"

extern SPI_HandleTypeDef hspi3;

#define SX1262_SPI_TIMEOUT_MS   100U
#define SX1262_BUSY_TIMEOUT_MS  10U

static sx126x_hal_status_t sx1262_wait_on_busy(void)
{
    uint32_t start = HAL_GetTick();

    while (HAL_GPIO_ReadPin(BUSY_GPIO_Port, BUSY_Pin) == GPIO_PIN_SET)
    {
        if ((HAL_GetTick() - start) > SX1262_BUSY_TIMEOUT_MS)
        {
            return SX126X_HAL_STATUS_ERROR;
        }
    }
    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_write(const void* context, const uint8_t* command, const uint16_t command_length,
                                      const uint8_t* data, const uint16_t data_length)
{
    (void)context;

    if (sx1262_wait_on_busy() != SX126X_HAL_STATUS_OK)
    {
        return SX126X_HAL_STATUS_ERROR;
    }

    HAL_GPIO_WritePin(SX_NSS_GPIO_Port, SX_NSS_Pin, GPIO_PIN_RESET);

    HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi3, (uint8_t*)command, command_length, SX1262_SPI_TIMEOUT_MS);
    if ((status == HAL_OK) && (data_length > 0))
    {
        status = HAL_SPI_Transmit(&hspi3, (uint8_t*)data, data_length, SX1262_SPI_TIMEOUT_MS);
    }

    HAL_GPIO_WritePin(SX_NSS_GPIO_Port, SX_NSS_Pin, GPIO_PIN_SET);

    return (status == HAL_OK) ? SX126X_HAL_STATUS_OK : SX126X_HAL_STATUS_ERROR;
}

sx126x_hal_status_t sx126x_hal_read(const void* context, const uint8_t* command, const uint16_t command_length,
                                     uint8_t* data, const uint16_t data_length)
{
    (void)context;

    if (sx1262_wait_on_busy() != SX126X_HAL_STATUS_OK)
    {
        return SX126X_HAL_STATUS_ERROR;
    }

    HAL_GPIO_WritePin(SX_NSS_GPIO_Port, SX_NSS_Pin, GPIO_PIN_RESET);

    HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi3, (uint8_t*)command, command_length, SX1262_SPI_TIMEOUT_MS);

    for (uint16_t i = 0; (status == HAL_OK) && (i < data_length); i++)
    {
        uint8_t nop = SX126X_NOP;
        status = HAL_SPI_TransmitReceive(&hspi3, &nop, &data[i], 1, SX1262_SPI_TIMEOUT_MS);
    }

    HAL_GPIO_WritePin(SX_NSS_GPIO_Port, SX_NSS_Pin, GPIO_PIN_SET);

    return (status == HAL_OK) ? SX126X_HAL_STATUS_OK : SX126X_HAL_STATUS_ERROR;
}

sx126x_hal_status_t sx126x_hal_reset(const void* context)
{
    (void)context;

    HAL_GPIO_WritePin(SX_RESET_GPIO_Port, SX_RESET_Pin, GPIO_PIN_RESET);
    HAL_Delay(1); /* datasheet minimum NRESET low pulse is 100us */
    HAL_GPIO_WritePin(SX_RESET_GPIO_Port, SX_RESET_Pin, GPIO_PIN_SET);

    return sx1262_wait_on_busy();
}

sx126x_hal_status_t sx126x_hal_wakeup(const void* context)
{
    (void)context;

    /* A falling edge on NSS pulls the radio out of Sleep mode (datasheet
     * 8.2.2 "SPI Timing When the Transceiver Leaves Sleep Mode"). Hold NSS
     * low briefly so the edge is unambiguous before releasing it. */
    HAL_GPIO_WritePin(SX_NSS_GPIO_Port, SX_NSS_Pin, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(SX_NSS_GPIO_Port, SX_NSS_Pin, GPIO_PIN_SET);

    return sx1262_wait_on_busy();
}
