#include "lora_radio.h"

#include "../../Drivers/SX1262/sx126x.h"
#include "../../Drivers/SX1262/sx126x_hal.h"
#include "app_conf.h"
#include "stm32_seq.h"

static void LoRaRadio_IrqTask(void);

bool LoRaRadio_Init(void)
{
    UTIL_SEQ_RegTask(1U << CFG_TASK_LORA_RADIO_IRQ_ID, UTIL_SEQ_RFU, LoRaRadio_IrqTask);

    return LoRaRadio_SelfTest();
}

bool LoRaRadio_SelfTest(void)
{
    if (sx126x_hal_reset(NULL) != SX126X_HAL_STATUS_OK)
    {
        return false;
    }

    sx126x_chip_status_t status;
    if (sx126x_get_status(NULL, &status) != SX126X_STATUS_OK)
    {
        return false;
    }

    return (status.chip_mode == SX126X_CHIP_MODE_STBY_RC) ||
           (status.chip_mode == SX126X_CHIP_MODE_STBY_XOSC);
}

void LoRaRadio_NotifyIrqFromISR(void)
{
    /* PA1/DIO1 ISR only schedules work; the task performs the SPI access. */
    UTIL_SEQ_SetTask(1U << CFG_TASK_LORA_RADIO_IRQ_ID, CFG_SEQ_PRIO_0);
}

static void LoRaRadio_IrqTask(void)
{
    sx126x_irq_mask_t irq_status = SX126X_IRQ_NONE;
    sx126x_get_and_clear_irq_status(NULL, &irq_status);

    /* TODO: dispatch irq_status (TxDone/RxDone/Timeout/...) to the LoRaWAN
     * MAC once the Basics Modem (smtc_modem_hal + radio planner) is wired
     * in. For now this only keeps DIO1 from re-triggering. */
    (void)irq_status;
}
