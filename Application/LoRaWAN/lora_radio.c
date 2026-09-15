#include "lora_radio.h"

#include "../../Drivers/SX1262/sx126x.h"
#include "../../Drivers/SX1262/sx126x_hal.h"
#include "app_conf.h"
#include "stm32_seq.h"

static void LoRaRadio_IrqTask(void);

/* Live Expressions targets (CubeIDE: Window -> Show View -> Live Expressions)
 * so the self-test result can be read while the program runs freely, with
 * no breakpoint needed. 0xFF means "never successfully read" - it is not a
 * value sx126x_chip_modes_t/sx126x_cmd_status_t ever take, so it is
 * distinguishable from a real (if wrong) 0x00 chip response. */
volatile bool    g_lora_reset_ok   = false;
volatile bool    g_lora_radio_ok   = false;
volatile uint8_t g_lora_chip_mode  = 0xFFU;
volatile uint8_t g_lora_cmd_status = 0xFFU;

bool LoRaRadio_Init(void)
{
    UTIL_SEQ_RegTask(1U << CFG_TASK_LORA_RADIO_IRQ_ID, UTIL_SEQ_RFU, LoRaRadio_IrqTask);

    return LoRaRadio_SelfTest();
}

bool LoRaRadio_SelfTest(void)
{
    g_lora_reset_ok = (sx126x_hal_reset(NULL) == SX126X_HAL_STATUS_OK);
    if (!g_lora_reset_ok)
    {
        return false;
    }

    sx126x_chip_status_t status;
    if (sx126x_get_status(NULL, &status) != SX126X_STATUS_OK)
    {
        return false;
    }

    g_lora_chip_mode  = (uint8_t)status.chip_mode;
    g_lora_cmd_status = (uint8_t)status.cmd_status;

    g_lora_radio_ok = (status.chip_mode == SX126X_CHIP_MODE_STBY_RC) ||
                       (status.chip_mode == SX126X_CHIP_MODE_STBY_XOSC);
    return g_lora_radio_ok;
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
