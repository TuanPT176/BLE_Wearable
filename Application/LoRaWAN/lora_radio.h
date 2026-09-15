#ifndef LORA_RADIO_H
#define LORA_RADIO_H

#include <stdbool.h>

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
