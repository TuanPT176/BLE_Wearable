/**
 ******************************************************************************
 * @file    lbm_hal_wb09.h
 * @brief   Hooks between the WB09 interrupt handlers / sequencer and the LBM
 *          HAL implementation in smtc_modem_hal_wb09.c.
 ******************************************************************************
 */
#ifndef LBM_HAL_WB09_H
#define LBM_HAL_WB09_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Call from SysTick_Handler right after HAL_IncTick() (ISR context, 1 ms). */
void LBM_HAL_TimerTick(void);

/* Call from the DIO1 EXTI callback (ISR context). */
void LBM_HAL_RadioIrq(void);

/* Ask for the LBM sequencer task to run again after `ms` milliseconds. */
void LBM_HAL_ArmEngineWake(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* LBM_HAL_WB09_H */
