#ifndef LORA_RADIO_H
#define LORA_RADIO_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Live Expressions targets (CubeIDE: Window -> Show View -> Live Expressions)
 * for reading the SPI/GPIO bring-up result without stopping the CPU.
 * g_lora_chip_mode/g_lora_cmd_status are 0xFF until the first successful
 * sx126x_get_status() read; afterwards they hold the raw
 * sx126x_chip_modes_t / sx126x_cmd_status_t values (STBY_RC = 2).
 */
extern volatile bool    g_lora_reset_ok;
extern volatile bool    g_lora_radio_ok;
extern volatile uint8_t g_lora_chip_mode;
extern volatile uint8_t g_lora_cmd_status;

/**
 * Register the radio IRQ sequencer task and run a SPI bring-up self-test
 * (reset the SX1262, read its status). This is the current bring-up
 * milestone; the LoRaWAN MAC (join/uplink scheduling, region parameters)
 * is not wired in yet — HAL_GPIO_EXTI_Callback -> LoRaRadio_NotifyIrqFromISR
 * only gets as far as clearing the IRQ status register for now.
 */
bool LoRaRadio_Init(void);

/**
 * Reset the SX1262 and read back its chip status over SPI.
 * @return true if the chip responds and reports STBY_RC/STBY_XOSC.
 */
bool LoRaRadio_SelfTest(void);

/**
 * Call from stm32wb0x_it.c's HAL_GPIO_EXTI_Callback when DIO1_Pin fires.
 * ISR context only: schedules CFG_TASK_LORA_RADIO_IRQ_ID, does no SPI I/O.
 */
void LoRaRadio_NotifyIrqFromISR(void);

#endif /* LORA_RADIO_H */
