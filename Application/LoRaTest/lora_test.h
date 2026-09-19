/**
 ******************************************************************************
 * @file    lora_test.h
 * @brief   Standalone SPI bring-up + single-packet TX test for the Ebyte
 *          E22-900M22S (SX1262-based) LoRa module on Nucleo-WB09KE.
 ******************************************************************************
 */
#ifndef LORA_TEST_H
#define LORA_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Set to 0 to compile the test out without removing the call site in main.c.
 * Off by default now: the LoRa Basics Modem (Application/LoRaWAN) owns the
 * SX1262, and the two must not run together. Set to 1 for radio bring-up
 * debugging (and then disable LBM_App_Init() in main.c). */
#define LORA_TEST_ENABLE 0

/*
 * This board has no working trace sink: PA1 is DIO1 (not USART1_TX), and
 * main.c's __io_putchar() is a stub, so APP_DBG_MSG() calls in lora_test.c
 * compile but print nowhere. Read the result from these globals instead,
 * with an STM32CubeIDE Live Expression (Window > Show View > Live
 * Expressions, add g_loraTestStep / g_loraTestResult / g_loraTestChipStatus
 * / g_loraTestIrqStatus) or a breakpoint at the end of LoRaTest_Run().
 */
typedef enum
{
  LORA_TEST_STEP_NONE = 0,
  LORA_TEST_STEP_RESET,
  LORA_TEST_STEP_GET_STATUS,
  LORA_TEST_STEP_CALIBRATE,
  LORA_TEST_STEP_CONFIG,
  LORA_TEST_STEP_TX,
  LORA_TEST_STEP_DONE
} LoRaTest_Step_t;

typedef enum
{
  LORA_TEST_RESULT_RUNNING = 0,
  LORA_TEST_RESULT_PASS,
  LORA_TEST_RESULT_FAIL
} LoRaTest_Result_t;

extern volatile LoRaTest_Step_t g_loraTestStep;
extern volatile LoRaTest_Result_t g_loraTestResult;
extern volatile uint8_t g_loraTestChipStatus;   /* last GetStatus byte */
extern volatile uint16_t g_loraTestIrqStatus;   /* last GetIrqStatus value */
extern volatile uint8_t g_loraTestBusyAfterReset;   /* raw BUSY level right after NRESET release: 1=HIGH (expected), 0=LOW (suspicious) */
extern volatile uint32_t g_loraTestTxDoneCount;     /* packets whose TxDone IRQ was seen so far */
extern volatile uint32_t g_loraTestResetElapsedMs;  /* ms actually spent waiting for BUSY to go low after reset (capped at the 3000ms give-up point) */

/**
 * @brief Blocking hardware bring-up test: reset the SX1262, read its status
 *        over SPI, configure it for LoRa, and transmit one test packet.
 *        Reports each step over APP_DBG_MSG (currently a no-op, see above)
 *        and via the g_loraTest* globals. Safe to call once at startup,
 *        before the BLE stack is brought up - it only touches SPI3 and the
 *        SX_NSS/SX_RESET/BUSY/DIO1 pins.
 */
void LoRaTest_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* LORA_TEST_H */
