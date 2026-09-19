/**
 ******************************************************************************
 * @file    lora_test.c
 * @brief   Standalone SPI bring-up + single-packet TX test for the Ebyte
 *          E22-900M22S (SX1262-based) LoRa module on Nucleo-WB09KE.
 *
 * Wiring (see Core/Inc/main.h, set up by the .ioc - this file does not
 * touch GPIO/SPI init, only main.c's MX_GPIO_Init()/MX_SPI3_Init() do):
 *   SPI3  -> module SCK/MOSI/MISO
 *   PA9   -> SX_NSS   (active-low chip select, software-controlled)
 *   PB15  -> SX_RESET (open-drain NRESET, active-low)
 *   PB14  -> BUSY     (input, chip pulls high while busy)
 *   PA1   -> DIO1     (TxDone/Timeout IRQ line; polled here, not used via EXTI)
 *
 * Deliberately independent of the BLE stack and the cooperative sequencer -
 * this is a blocking, run-once self-test meant to be called from main()
 * before MX_APPE_Init(), so a wiring/config problem shows up before the rest
 * of the firmware (BLE, sensors, NFC) is even brought up. There is currently
 * no UART trace sink on this board (PA1 is DIO1, not USART1_TX) - read
 * results via the g_loraTest* globals in lora_test.h with a debugger, or
 * step through under STM32CubeIDE. A previous attempt at a full SX1262
 * driver integration (Application/LoRaWAN/, Drivers/SX1262/) was removed
 * after failing this same bring-up step - see commit 7a7f36b. This file
 * re-derives the SPI command sequence directly from the SX1262 datasheet
 * instead of reusing that driver, kept intentionally minimal so each step's
 * pass/fail is easy to inspect.
 *
 * CONFIRMED on real hardware (Nucleo-WB09KE + E22-900M22S, wiring verified
 * with a multimeter): full bring-up sequence passes - GetStatus, Calibrate,
 * SetRfFrequency, WriteBuffer and SetTx all succeed, TxDone IRQ observed.
 * E22-900M22S uses a TCXO (confirmed by the user, not just Ebyte's published
 * default) - LORA_TCXO_VOLTAGE/LORA_TCXO_TIMEOUT_MS below worked as-is on
 * this unit, but a different E22-900M22S batch/revision could still differ.
 *
 * STILL UNVERIFIED:
 *   - LORA_FREQ_HZ below defaults to 868.000000 MHz (a common EU LoRa test
 *     frequency); pick whatever is legal to transmit on in your region -
 *     e.g. Vietnam's LPWAN ISM sub-band is around 920-925 MHz per Circular
 *     47/2020/TT-BTTTT (moderate confidence only, re-check current rules
 *     before radiating for real).
 ******************************************************************************
 */

#include "lora_test.h"

#if LORA_TEST_ENABLE

#include "main.h"
#include "app_conf.h"
#include <string.h>
#include <stdbool.h>

/* ---- Configuration ---------------------------------------------------- */

/* 0 = bare XTAL crystal on XTA/XTB (custom wearable PCB as originally
 *     designed; DIO3 not wired to the MCU)
 * 1 = TCXO (Nucleo-WB09KE + Ebyte E22-900M22S rig; also the custom PCB after
 *     the user retrofitted a TCXO onto it to test whether the XTAL path was
 *     the reason the SX1262 never left its post-reset BUSY state) */
#define LORA_USE_TCXO            1
#define LORA_TCXO_VOLTAGE        0x02u   /* 0x02 = 1.8V per SX1262 datasheet (0x00=1.6V, 0x01=1.7V); worked on E22 rig + retrofitted custom PCB */
#define LORA_TCXO_TIMEOUT_MS     5u

#define LORA_USE_DCDC             1      /* SetRegulatorMode: 1=DC-DC+LDO, 0=LDO only (VERIFY) */
#define LORA_USE_DIO2_RF_SWITCH   1      /* module's onboard TX/RX switch is driven by DIO2 internally */

#define LORA_XTAL_HZ              32000000UL
#define LORA_FREQ_HZ              923000000UL   /* VERIFY this is legal in your region before TX */

#define LORA_OUTPUT_POWER_DBM     22            /* matches E22-900M22S's rated +22dBm */
#define LORA_SPREADING_FACTOR     7
#define LORA_BANDWIDTH_REG        0x04u          /* 125 kHz */
#define LORA_CODING_RATE_REG      0x01u          /* 4/5 */

#define LORA_TX_TIMEOUT_MS        3000u

/* Packets sent back to back before the test hands over to the BLE app
 * (1 = single shot). LoRaTest_Run() blocks for roughly count * period. */
#define LORA_TX_REPEAT_COUNT      30u
#define LORA_TX_REPEAT_PERIOD_MS  1000u

/* Trailing "000" is overwritten with the packet counter each transmission. */
static uint8_t s_testPayload[] = "LoRaTest #000";

extern SPI_HandleTypeDef hspi3;

volatile LoRaTest_Step_t g_loraTestStep = LORA_TEST_STEP_NONE;
volatile LoRaTest_Result_t g_loraTestResult = LORA_TEST_RESULT_RUNNING;
volatile uint8_t g_loraTestChipStatus = 0;
volatile uint16_t g_loraTestIrqStatus = 0;
volatile uint8_t g_loraTestBusyAfterReset = 0;
volatile uint32_t g_loraTestResetElapsedMs = 0;
volatile uint32_t g_loraTestTxDoneCount = 0;

/* IRQ status bits (SetDioIrqParams / GetIrqStatus) */
#define SX_IRQ_TX_DONE            0x0001u
#define SX_IRQ_TIMEOUT            0x0200u

/* ---- Low-level SPI/GPIO helpers --------------------------------------- */

static bool SX_WaitOnBusy(uint32_t timeoutMs)
{
  uint32_t start = HAL_GetTick();
  while (HAL_GPIO_ReadPin(BUSY_GPIO_Port, BUSY_Pin) == GPIO_PIN_SET)
  {
    if ((HAL_GetTick() - start) > timeoutMs)
    {
      return false;
    }
  }
  return true;
}

/* Every SPI transaction with the SX1262 must be preceded by BUSY==low. */
static bool SX_Transfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
  if (!SX_WaitOnBusy(1000))
  {
    return false;
  }
  HAL_GPIO_WritePin(SX_NSS_GPIO_Port, SX_NSS_Pin, GPIO_PIN_RESET);
  HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&hspi3, (uint8_t *)tx, rx, len, 1000);
  HAL_GPIO_WritePin(SX_NSS_GPIO_Port, SX_NSS_Pin, GPIO_PIN_SET);
  return st == HAL_OK;
}

static bool SX_WriteCommand(uint8_t opcode, const uint8_t *params, uint8_t len)
{
  uint8_t tx[16] = {0};
  uint8_t rx[16] = {0};
  if ((uint16_t)len + 1u > sizeof(tx))
  {
    return false;
  }
  tx[0] = opcode;
  if (len > 0u)
  {
    memcpy(&tx[1], params, len);
  }
  return SX_Transfer(tx, rx, (uint16_t)len + 1u);
}

/* opcode, then 1 status byte, then `len` data bytes (GetIrqStatus shape). */
static bool SX_ReadCommand(uint8_t opcode, uint8_t *outData, uint8_t len)
{
  uint8_t tx[16] = {0};
  uint8_t rx[16] = {0};
  uint16_t total = (uint16_t)len + 2u;
  if (total > sizeof(tx))
  {
    return false;
  }
  tx[0] = opcode;
  if (!SX_Transfer(tx, rx, total))
  {
    return false;
  }
  memcpy(outData, &rx[2], len);
  return true;
}

static bool SX_GetStatus(uint8_t *status)
{
  uint8_t tx[2] = {0xC0, 0x00};
  uint8_t rx[2] = {0};
  if (!SX_Transfer(tx, rx, 2))
  {
    return false;
  }
  *status = rx[1];
  return true;
}

static bool SX_WriteBuffer(uint8_t offset, const uint8_t *data, uint8_t len)
{
  uint8_t tx[64] = {0};
  uint8_t rx[64] = {0};
  uint16_t total = (uint16_t)len + 2u;
  if (total > sizeof(tx))
  {
    return false;
  }
  tx[0] = 0x0E;
  tx[1] = offset;
  memcpy(&tx[2], data, len);
  return SX_Transfer(tx, rx, total);
}

static bool SX_Reset(void)
{
  HAL_GPIO_WritePin(SX_RESET_GPIO_Port, SX_RESET_Pin, GPIO_PIN_RESET);
  HAL_Delay(1); /* datasheet min is 100us; 1ms is comfortably longer */
  HAL_GPIO_WritePin(SX_RESET_GPIO_Port, SX_RESET_Pin, GPIO_PIN_SET);
  HAL_Delay(1); /* let BUSY assert before polling it */

  /* Diagnostic: log the raw pin level right after release (sanity check
   * the MCU is actually reading the module's BUSY line at all), then poll
   * with a generous timeout and report how long it actually took - this
   * tells "BUSY is just slower than expected" apart from "never clears". */
  GPIO_PinState rawAfterRelease = HAL_GPIO_ReadPin(BUSY_GPIO_Port, BUSY_Pin);
  g_loraTestBusyAfterReset = (rawAfterRelease == GPIO_PIN_SET) ? 1u : 0u;
  APP_DBG_MSG("[LoRaTest] BUSY right after reset release = %s\r\n",
              (rawAfterRelease == GPIO_PIN_SET) ? "HIGH (expected)" : "LOW (unexpected)");

  uint32_t start = HAL_GetTick();
  bool ok = SX_WaitOnBusy(3000);
  uint32_t elapsed = HAL_GetTick() - start;
  g_loraTestResetElapsedMs = elapsed;
  if (ok)
  {
    APP_DBG_MSG("[LoRaTest] BUSY went low after %lu ms\r\n", (unsigned long)elapsed);
  }
  else
  {
    APP_DBG_MSG("[LoRaTest] BUSY still HIGH after %lu ms (gave up)\r\n", (unsigned long)elapsed);
  }
  return ok;
}

/* ---- Test sequence ------------------------------------------------------*/

void LoRaTest_Run(void)
{
  g_loraTestResult = LORA_TEST_RESULT_RUNNING;
  g_loraTestStep = LORA_TEST_STEP_RESET;
  APP_DBG_MSG("\r\n[LoRaTest] E22-900M22S bring-up starting...\r\n");

  if (!SX_Reset())
  {
    APP_DBG_MSG("[LoRaTest] FAIL: BUSY never went low after reset - check "
                "wiring/power (VCC, GND, NRESET, BUSY), not a config issue.\r\n");
    g_loraTestResult = LORA_TEST_RESULT_FAIL;
    return;
  }

  g_loraTestStep = LORA_TEST_STEP_GET_STATUS;
  uint8_t status = 0;
  if (!SX_GetStatus(&status))
  {
    APP_DBG_MSG("[LoRaTest] FAIL: SPI transfer error on GetStatus - check "
                "SPI3 SCK/MOSI/MISO wiring and SX_NSS (PA9).\r\n");
    g_loraTestResult = LORA_TEST_RESULT_FAIL;
    return;
  }
  g_loraTestChipStatus = status;
  APP_DBG_MSG("[LoRaTest] GetStatus = 0x%02X\r\n", (unsigned int)status);
  if (status == 0x00u || status == 0xFFu)
  {
    APP_DBG_MSG("[LoRaTest] FAIL: GetStatus returned 0x%02X - MISO likely "
                "stuck or chip not responding at all.\r\n", (unsigned int)status);
    g_loraTestResult = LORA_TEST_RESULT_FAIL;
    return;
  }
  APP_DBG_MSG("[LoRaTest] PASS: SPI link alive, chip responds.\r\n");

  /* SetStandby(STDBY_RC) */
  {
    uint8_t p = 0x00;
    SX_WriteCommand(0x80, &p, 1);
  }

#if LORA_USE_DCDC
  {
    uint8_t p = 0x01; /* REGULATOR_MODE_DCDC */
    SX_WriteCommand(0x96, &p, 1);
  }
#endif

#if LORA_USE_TCXO
  {
    uint32_t delayTicks = LORA_TCXO_TIMEOUT_MS * 64u; /* units of 15.625us */
    uint8_t p[4] = {
      (uint8_t)LORA_TCXO_VOLTAGE,
      (uint8_t)(delayTicks >> 16),
      (uint8_t)(delayTicks >> 8),
      (uint8_t)(delayTicks)
    };
    SX_WriteCommand(0x97, p, 4); /* SetDio3AsTcxoCtrl */
  }
#endif

  /* Calibrate all blocks (RC64k, RC13M, PLL, ADC, image) */
  g_loraTestStep = LORA_TEST_STEP_CALIBRATE;
  {
    uint8_t p = 0x7F;
    SX_WriteCommand(0x89, &p, 1);
  }
  if (!SX_WaitOnBusy(1000))
  {
    APP_DBG_MSG("[LoRaTest] FAIL: Calibrate never completed (BUSY stuck "
                "high) - check TCXO/XTAL config above.\r\n");
    g_loraTestResult = LORA_TEST_RESULT_FAIL;
    return;
  }
  if (SX_GetStatus(&status))
  {
    g_loraTestChipStatus = status;
    APP_DBG_MSG("[LoRaTest] After Calibrate, GetStatus = 0x%02X\r\n", (unsigned int)status);
  }

  g_loraTestStep = LORA_TEST_STEP_CONFIG;
#if LORA_USE_DIO2_RF_SWITCH
  {
    uint8_t p = 0x01;
    SX_WriteCommand(0x9D, &p, 1); /* SetDio2AsRfSwitchCtrl */
  }
#endif

  /* SetPacketType(LoRa) */
  {
    uint8_t p = 0x01;
    SX_WriteCommand(0x8A, &p, 1);
  }

  /* SetRfFrequency */
  {
    uint32_t freqReg = (uint32_t)(((uint64_t)LORA_FREQ_HZ << 25) / LORA_XTAL_HZ);
    uint8_t p[4] = {
      (uint8_t)(freqReg >> 24),
      (uint8_t)(freqReg >> 16),
      (uint8_t)(freqReg >> 8),
      (uint8_t)(freqReg)
    };
    SX_WriteCommand(0x86, p, 4);
    APP_DBG_MSG("[LoRaTest] RF frequency set to %lu Hz (reg=0x%08lX)\r\n",
                (unsigned long)LORA_FREQ_HZ, (unsigned long)freqReg);
  }

  /* SetPaConfig: high-power PA, SX1262, +22dBm profile per datasheet table */
  {
    uint8_t p[4] = {0x04, 0x07, 0x00, 0x01};
    SX_WriteCommand(0x95, p, 4);
  }

  /* SetTxParams: power, ramp time (0x04 = 200us) */
  {
    uint8_t p[2] = {(uint8_t)LORA_OUTPUT_POWER_DBM, 0x04};
    SX_WriteCommand(0x8E, p, 2);
  }

  /* SetBufferBaseAddress: TX/RX both start at 0x00 */
  {
    uint8_t p[2] = {0x00, 0x00};
    SX_WriteCommand(0x8F, p, 2);
  }

  /* SetModulationParams: SF, BW, CR, low-data-rate-optimize off */
  {
    uint8_t p[4] = {(uint8_t)LORA_SPREADING_FACTOR, (uint8_t)LORA_BANDWIDTH_REG,
                     (uint8_t)LORA_CODING_RATE_REG, 0x00};
    SX_WriteCommand(0x8B, p, 4);
  }

  /* SetPacketParams: preamble=8, explicit header, payload length, CRC on, standard IQ */
  uint8_t payloadLen = (uint8_t)(sizeof(s_testPayload) - 1u); /* drop the NUL */
  {
    uint8_t p[6] = {0x00, 0x08, 0x00, payloadLen, 0x01, 0x00};
    SX_WriteCommand(0x8C, p, 6);
  }

  /* SetDioIrqParams: route TxDone|Timeout to DIO1, mirror into the IRQ mask */
  {
    uint16_t mask = SX_IRQ_TX_DONE | SX_IRQ_TIMEOUT;
    uint8_t p[8] = {
      (uint8_t)(mask >> 8), (uint8_t)(mask),
      (uint8_t)(mask >> 8), (uint8_t)(mask),
      0x00, 0x00,
      0x00, 0x00
    };
    SX_WriteCommand(0x08, p, 8);
  }

  g_loraTestStep = LORA_TEST_STEP_TX;
  bool txDone = false;
  bool txTimeout = false;
  for (uint32_t n = 0; n < LORA_TX_REPEAT_COUNT; n++)
  {
    uint32_t packetStart = HAL_GetTick();
    txDone = false;
    txTimeout = false;

    /* Last 3 payload bytes are a decimal packet counter so a receiver can
     * tell packets apart and spot drops. */
    s_testPayload[payloadLen - 3u] = (uint8_t)('0' + ((n / 100u) % 10u));
    s_testPayload[payloadLen - 2u] = (uint8_t)('0' + ((n / 10u) % 10u));
    s_testPayload[payloadLen - 1u] = (uint8_t)('0' + (n % 10u));

    /* ClearIrqStatus(all) before TX */
    {
      uint8_t p[2] = {0xFF, 0xFF};
      SX_WriteCommand(0x02, p, 2);
    }

    if (!SX_WriteBuffer(0x00, s_testPayload, payloadLen))
    {
      APP_DBG_MSG("[LoRaTest] FAIL: WriteBuffer SPI error.\r\n");
      g_loraTestResult = LORA_TEST_RESULT_FAIL;
      return;
    }

    /* SetTx: 24-bit timeout in units of 15.625us (here: LORA_TX_TIMEOUT_MS) */
    {
      uint32_t ticks = LORA_TX_TIMEOUT_MS * 64u;
      uint8_t p[3] = {(uint8_t)(ticks >> 16), (uint8_t)(ticks >> 8), (uint8_t)(ticks)};
      SX_WriteCommand(0x83, p, 3);
    }
    APP_DBG_MSG("[LoRaTest] TX #%lu: \"%s\"\r\n", (unsigned long)n, (const char *)s_testPayload);

    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < (LORA_TX_TIMEOUT_MS + 500u))
    {
      uint8_t irq[2] = {0};
      if (!SX_ReadCommand(0x12, irq, 2))
      {
        APP_DBG_MSG("[LoRaTest] FAIL: SPI error while polling GetIrqStatus.\r\n");
        g_loraTestResult = LORA_TEST_RESULT_FAIL;
        return;
      }
      uint16_t irqStatus = ((uint16_t)irq[0] << 8) | irq[1];
      g_loraTestIrqStatus = irqStatus;
      if (irqStatus & SX_IRQ_TX_DONE)
      {
        txDone = true;
        break;
      }
      if (irqStatus & SX_IRQ_TIMEOUT)
      {
        txTimeout = true;
        break;
      }
      HAL_Delay(10);
    }

    /* ClearIrqStatus(all) */
    {
      uint8_t p[2] = {0xFF, 0xFF};
      SX_WriteCommand(0x02, p, 2);
    }

    if (!txDone)
    {
      break;
    }
    g_loraTestTxDoneCount++;

    while ((HAL_GetTick() - packetStart) < LORA_TX_REPEAT_PERIOD_MS)
    {
    }
  }

  g_loraTestStep = LORA_TEST_STEP_DONE;
  if (txDone)
  {
    APP_DBG_MSG("[LoRaTest] PASS: TxDone IRQ observed - packet transmitted.\r\n");
    g_loraTestResult = LORA_TEST_RESULT_PASS;
  }
  else if (txTimeout)
  {
    APP_DBG_MSG("[LoRaTest] FAIL: chip reported Timeout IRQ - TX never "
                "completed (check antenna/PA config, TCXO settings above).\r\n");
    g_loraTestResult = LORA_TEST_RESULT_FAIL;
  }
  else
  {
    APP_DBG_MSG("[LoRaTest] FAIL: no TxDone/Timeout IRQ within %lums - chip "
                "may be stuck; re-check TCXO/XTAL and PA config.\r\n",
                (unsigned long)(LORA_TX_TIMEOUT_MS + 500u));
    g_loraTestResult = LORA_TEST_RESULT_FAIL;
  }
}

#endif /* LORA_TEST_ENABLE */
