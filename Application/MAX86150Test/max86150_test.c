/**
 ******************************************************************************
 * @file    max86150_test.c
 * @brief   Standalone I2C/hardware bring-up test for the MAX86150 PPG sensor
 *          on the custom wearable PCB.
 *
 * The MAX86150 worked on the earlier test hardware but does not come up on
 * the new PCB. This test walks from the bus up to the optical path and stops
 * at the first thing that is wrong, so the failing step points at the likely
 * hardware cause:
 *
 *   BUS_LINES   SCL/SDA (PB6/PB7) read as plain GPIO inputs, once with the
 *               MCU pull-up (a LOW here = line shorted or held low) and once
 *               with the MCU pull-down (a LOW here = no external pull-up
 *               resistor). A stuck SDA gets the standard 9-clock recovery.
 *   BUS_SCAN    Which 7-bit addresses ACK. Nothing at all -> shared bus
 *               problem; others but not 0x5E -> MAX86150-specific problem.
 *   ACK         20 address probes at 0x5E: intermittent ACK = marginal joint.
 *   PART_ID     Register 0xFF must read 0x1E.
 *   RESET       SYS_CONTROL RESET bit must self-clear.
 *   REG_RW      Write/read back LED1_PA.
 *   FIFO        With LEDs at 0 mA the FIFO must still fill at 100 Hz.
 *   INTERRUPTS  PPG_RDY must pull INTB (PB4, assumed from the CubeMX "INTB"
 *               label) low until INT_STATUS_1 is read; VDD_OOR (VDD_ANA
 *               outside 1.65-2.05 V) is checked with the LEDs off and again
 *               with both LEDs at full current.
 *   LED         LEDs on vs off - needs a finger (or any reflective surface)
 *               on the sensor to show a clear difference. No difference =
 *               VLED rail / LED pads (the red LED should also visibly glow).
 *   DRIVER      Same checks through Drivers/Sensors/MAX86150/max86150_optical.c
 *               so the production code path is exercised too.
 *   STREAM      Live IR/Red values for MAX86150_TEST_STREAM_MS; with a
 *               finger on the sensor irPeakToPeak shows the pulse. VDD_OOR
 *               stays armed and is sampled every 500 ms (vddOorHits/Checks).
 *
 * Register-level steps deliberately bypass max86150_optical.c so a driver
 * bug and a hardware fault can be told apart (see DRIVER_PATH_FAILED).
 * Runs from main() before MX_APPE_Init(), independent of the BLE stack and
 * the sequencer, like Application/LoRaTest. Results: g_max86150Test.
 ******************************************************************************
 */

#include "max86150_test.h"

#if MAX86150_TEST_ENABLE

#include "main.h"
#include <stdbool.h>
#include <string.h>
#include "../../Drivers/Sensors/MAX86150/max86150_optical.h"

/* ---- Configuration ---------------------------------------------------- */

#define MAX86150_TEST_I2C_TIMEOUT_MS      20U
#define MAX86150_TEST_ACK_TRIALS          20U
/* Same LED current as sensor_manager.c's OPTICAL_DEFAULT_LED_CURRENT_CODE
 * (~7 mA at 0.2 mA/LSB) so the test sees what the app will see. */
#define MAX86150_TEST_LED_CURRENT_CODE    0x24U
#define MAX86150_TEST_WINDOW_MS           200U  /* ~20 samples at 100 Hz, under the 32-deep FIFO */
#define MAX86150_TEST_MIN_WINDOW_SAMPLES  10U
#define MAX86150_TEST_LED_MIN_DELTA       2000  /* raw counts; matches OPTICAL_MIN_DC_FOR_VALID */
#define MAX86150_TEST_SATURATION_COUNTS   0x7F000UL /* 19-bit full scale is 0x7FFFF */
#define MAX86150_TEST_STREAM_MS           30000U /* 0 = skip the live stream */
#define MAX86150_TEST_STREAM_POLL_MS      20U
#define MAX86150_TEST_P2P_WINDOW_SAMPLES  200U  /* ~2 s at 100 Hz */
/* Both LEDs at full scale (LED_RANGE 0: 0.2 mA/LSB -> ~51 mA pulses) to
 * load VLED/PGND as hard as the app ever could during the VDD_OOR check. */
#define MAX86150_TEST_LED_MAX_CODE        0xFFU
/* VDD_OOR needs VDD_ANA outside 1.65-2.05 V for ~10 ms before it latches;
 * 20 ms gives it time to fire. Reading INT_STATUS_2 clears it. */
#define MAX86150_TEST_VDD_OOR_WAIT_MS     20U
#define MAX86150_TEST_VDD_POLL_MS         500U  /* VDD_OOR sampling period during STREAM */

/* I2C1 pins, hard-coded like stm32wb0x_hal_msp.c (no CubeMX labels). */
#define TEST_I2C_PORT       GPIOB
#define TEST_SCL_PIN        GPIO_PIN_6
#define TEST_SDA_PIN        GPIO_PIN_7
#define TEST_PWR_PINS       (PWR_GPIO_BIT_6 | PWR_GPIO_BIT_7)

/* ---- MAX86150 registers ------------------------------------------------ */

#define REG_INT_STATUS_1    0x00U
#define REG_INT_STATUS_2    0x01U
#define REG_INT_ENABLE_1    0x02U
#define REG_INT_ENABLE_2    0x03U
#define REG_FIFO_WRITE_PTR  0x04U
#define REG_FIFO_OVERFLOW   0x05U
#define REG_FIFO_READ_PTR   0x06U
#define REG_FIFO_DATA       0x07U
#define REG_FIFO_CONFIG     0x08U
#define REG_FIFO_CONTROL_1  0x09U
#define REG_FIFO_CONTROL_2  0x0AU
#define REG_SYSTEM_CONTROL  0x0DU
#define REG_PPG_CONFIG_1    0x0EU
#define REG_PPG_CONFIG_2    0x0FU
#define REG_LED1_PA         0x11U   /* IR */
#define REG_LED2_PA         0x12U   /* Red */
#define REG_LED_RANGE       0x14U
#define REG_PART_ID         0xFFU

#define SYS_RESET           0x01U
#define SYS_FIFO_ENABLE     0x04U
#define INT1_PPG_RDY        0x40U
#define INT2_VDD_OOR        0x80U
#define SAMPLE_MASK         0x0007FFFFUL

#define MAX86150_HAL_ADDR   ((uint16_t)(MAX86150_OPTICAL_I2C_ADDRESS << 1U))

volatile MAX86150Test_Report_t g_max86150Test;

/* ---- Verdict helpers --------------------------------------------------- */

static void Test_Fail(MAX86150Test_Diag_t diag)
{
  g_max86150Test.diagnosis = diag;
  g_max86150Test.result = MAX86150_TEST_RESULT_FAIL;
}

static void Test_Warn(MAX86150Test_Diag_t diag)
{
  if (g_max86150Test.diagnosis == MAX86150_DIAG_OK)
  {
    g_max86150Test.diagnosis = diag;
  }
}

/* ---- Register access (bypasses max86150_optical.c on purpose) ---------- */

static void Test_NoteI2cError(void)
{
  if (g_max86150Test.halI2cError == 0U)
  {
    g_max86150Test.halI2cError = hi2c1.ErrorCode;
  }
}

static bool Test_Read(uint8_t reg, uint8_t *data, uint16_t len)
{
  if (HAL_I2C_Mem_Read(&hi2c1, MAX86150_HAL_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                       data, len, MAX86150_TEST_I2C_TIMEOUT_MS) == HAL_OK)
  {
    return true;
  }
  Test_NoteI2cError();
  return false;
}

static bool Test_Write(uint8_t reg, uint8_t value)
{
  if (HAL_I2C_Mem_Write(&hi2c1, MAX86150_HAL_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                        &value, 1U, MAX86150_TEST_I2C_TIMEOUT_MS) == HAL_OK)
  {
    return true;
  }
  Test_NoteI2cError();
  return false;
}

/* ---- Bus line check ---------------------------------------------------- */

static void Test_LinesAsInput(uint32_t pull)
{
  GPIO_InitTypeDef gpio = {0};

  /* WB0 pulls live in PWR_PUCRx/PDCRx as well as GPIO PUPDR; set both,
   * as HAL_I2C_MspInit() does. */
  if (pull == GPIO_PULLDOWN)
  {
    (void)HAL_PWREx_EnableGPIOPullDown(PWR_GPIO_B, TEST_PWR_PINS);
  }
  else
  {
    (void)HAL_PWREx_EnableGPIOPullUp(PWR_GPIO_B, TEST_PWR_PINS);
  }
  gpio.Pin = TEST_SCL_PIN | TEST_SDA_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = pull;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TEST_I2C_PORT, &gpio);
  HAL_Delay(2U);
}

static uint8_t Test_LineHigh(uint16_t pin)
{
  return (HAL_GPIO_ReadPin(TEST_I2C_PORT, pin) == GPIO_PIN_SET) ? 1U : 0U;
}

/* Standard I2C bus clear: clock SCL until the slave releases SDA, then
 * send a STOP. Leaves both pins as open-drain outputs, released HIGH. */
static uint8_t Test_RecoverBus(void)
{
  GPIO_InitTypeDef gpio = {0};
  uint8_t clocks = 0U;

  HAL_GPIO_WritePin(TEST_I2C_PORT, TEST_SCL_PIN | TEST_SDA_PIN, GPIO_PIN_SET);
  gpio.Pin = TEST_SCL_PIN | TEST_SDA_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TEST_I2C_PORT, &gpio);

  while ((clocks < 9U) && (Test_LineHigh(TEST_SDA_PIN) == 0U))
  {
    HAL_GPIO_WritePin(TEST_I2C_PORT, TEST_SCL_PIN, GPIO_PIN_RESET);
    HAL_Delay(1U);
    HAL_GPIO_WritePin(TEST_I2C_PORT, TEST_SCL_PIN, GPIO_PIN_SET);
    HAL_Delay(1U);
    clocks++;
  }

  /* STOP: SDA low -> high while SCL is high. */
  HAL_GPIO_WritePin(TEST_I2C_PORT, TEST_SCL_PIN, GPIO_PIN_RESET);
  HAL_Delay(1U);
  HAL_GPIO_WritePin(TEST_I2C_PORT, TEST_SDA_PIN, GPIO_PIN_RESET);
  HAL_Delay(1U);
  HAL_GPIO_WritePin(TEST_I2C_PORT, TEST_SCL_PIN, GPIO_PIN_SET);
  HAL_Delay(1U);
  HAL_GPIO_WritePin(TEST_I2C_PORT, TEST_SDA_PIN, GPIO_PIN_SET);
  HAL_Delay(1U);
  return clocks;
}

/* Same settings as MX_I2C1_Init(); hi2c1.Init survives HAL_I2C_DeInit(). */
static bool Test_I2cReinit(void)
{
  return (HAL_I2C_Init(&hi2c1) == HAL_OK) &&
         (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) == HAL_OK) &&
         (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0U) == HAL_OK);
}

static bool Test_BusLines(void)
{
  bool lines_ok;

  (void)HAL_I2C_DeInit(&hi2c1);

  Test_LinesAsInput(GPIO_PULLUP);
  if ((Test_LineHigh(TEST_SCL_PIN) == 1U) && (Test_LineHigh(TEST_SDA_PIN) == 0U))
  {
    g_max86150Test.busRecoveryClocks = Test_RecoverBus();
    Test_LinesAsInput(GPIO_PULLUP);
  }
  g_max86150Test.sclHighWithMcuPullUp = Test_LineHigh(TEST_SCL_PIN);
  g_max86150Test.sdaHighWithMcuPullUp = Test_LineHigh(TEST_SDA_PIN);
  lines_ok = (g_max86150Test.sclHighWithMcuPullUp == 1U) &&
             (g_max86150Test.sdaHighWithMcuPullUp == 1U);

  /* Only after recovery: a slave still holding SDA low would otherwise
   * read as "no external pull-up". */
  Test_LinesAsInput(GPIO_PULLDOWN);
  g_max86150Test.sclHighWithMcuPullDown = Test_LineHigh(TEST_SCL_PIN);
  g_max86150Test.sdaHighWithMcuPullDown = Test_LineHigh(TEST_SDA_PIN);

  /* HAL_I2C_Init() -> HAL_I2C_MspInit() restores AF mode and pull-ups. */
  if (!Test_I2cReinit())
  {
    Test_Fail(MAX86150_DIAG_I2C_REINIT_FAILED);
    return false;
  }
  if (!lines_ok)
  {
    Test_Fail(MAX86150_DIAG_BUS_STUCK_LOW);
    return false;
  }
  if ((g_max86150Test.sclHighWithMcuPullDown == 0U) ||
      (g_max86150Test.sdaHighWithMcuPullDown == 0U))
  {
    Test_Warn(MAX86150_DIAG_NO_EXTERNAL_PULLUP);
  }
  return true;
}

/* ---- Bus scan / ACK ---------------------------------------------------- */

static bool Test_BusScan(void)
{
  uint8_t address;
  bool found = false;

  for (address = 0x08U; address <= 0x77U; address++)
  {
    if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)((uint16_t)address << 1U),
                              1U, 2U) != HAL_OK)
    {
      continue;
    }
    if (g_max86150Test.scanCount < MAX86150_TEST_SCAN_MAX)
    {
      g_max86150Test.scanAddr[g_max86150Test.scanCount] = address;
    }
    g_max86150Test.scanCount++;
    if (address == MAX86150_OPTICAL_I2C_ADDRESS)
    {
      found = true;
    }
  }

  if (!found)
  {
    Test_Fail((g_max86150Test.scanCount == 0U) ? MAX86150_DIAG_BUS_NO_DEVICES
                                               : MAX86150_DIAG_NO_ACK_AT_0x5E);
  }
  return found;
}

static void Test_AckTrials(void)
{
  uint8_t i;

  for (i = 0U; i < MAX86150_TEST_ACK_TRIALS; i++)
  {
    if (HAL_I2C_IsDeviceReady(&hi2c1, MAX86150_HAL_ADDR, 1U,
                              MAX86150_TEST_I2C_TIMEOUT_MS) == HAL_OK)
    {
      g_max86150Test.ackCount++;
    }
  }
  if (g_max86150Test.ackCount < MAX86150_TEST_ACK_TRIALS)
  {
    Test_Warn(MAX86150_DIAG_ACK_INTERMITTENT);
  }
}

/* ---- Chip-level checks ------------------------------------------------- */

static bool Test_PartId(void)
{
  uint8_t value = 0U;

  /* Before any reset: PWR_RDY (status 1 bit 0) should be set by power-up. */
  if (Test_Read(REG_INT_STATUS_1, &value, 1U))
  {
    g_max86150Test.intStatus1AtBoot = value;
  }
  if (Test_Read(REG_INT_STATUS_2, &value, 1U))
  {
    g_max86150Test.intStatus2AtBoot = value;
  }

  if (!Test_Read(REG_PART_ID, &value, 1U))
  {
    Test_Fail(MAX86150_DIAG_REGISTER_READ_FAILED);
    return false;
  }
  g_max86150Test.partId = value;
  if (value != MAX86150_OPTICAL_EXPECTED_PART_ID)
  {
    Test_Fail(MAX86150_DIAG_WRONG_PART_ID);
    return false;
  }
  return true;
}

static bool Test_Reset(void)
{
  uint8_t control = SYS_RESET;
  uint32_t started_at;

  if (!Test_Write(REG_SYSTEM_CONTROL, SYS_RESET))
  {
    Test_Fail(MAX86150_DIAG_REGISTER_READ_FAILED);
    return false;
  }
  started_at = HAL_GetTick();
  while ((control & SYS_RESET) != 0U)
  {
    if (!Test_Read(REG_SYSTEM_CONTROL, &control, 1U))
    {
      Test_Fail(MAX86150_DIAG_REGISTER_READ_FAILED);
      return false;
    }
    if ((HAL_GetTick() - started_at) >= 100U)
    {
      Test_Fail(MAX86150_DIAG_RESET_TIMEOUT);
      return false;
    }
  }
  g_max86150Test.resetMs = HAL_GetTick() - started_at;
  return true;
}

static bool Test_RegisterReadBack(void)
{
  static const uint8_t patterns[] = {0x55U, 0xAAU, 0x00U};
  uint8_t i;
  uint8_t value;

  for (i = 0U; i < sizeof(patterns); i++)
  {
    if (!Test_Write(REG_LED1_PA, patterns[i]) ||
        !Test_Read(REG_LED1_PA, &value, 1U))
    {
      Test_Fail(MAX86150_DIAG_REGISTER_READ_FAILED);
      return false;
    }
    if (value != patterns[i])
    {
      Test_Fail(MAX86150_DIAG_REG_RW_MISMATCH);
      return false;
    }
  }
  g_max86150Test.regRwOk = 1U;
  return true;
}

/* Same Red/IR setup as MAX86150_OpticalConfigureRedIr(), minus the reset. */
static bool Test_ConfigurePpg(uint8_t led_current)
{
  return Test_Write(REG_FIFO_CONFIG, 0x1FU) &&      /* rollover on full */
         Test_Write(REG_FIFO_CONTROL_1, 0x21U) &&   /* slot1 IR, slot2 Red */
         Test_Write(REG_FIFO_CONTROL_2, 0x00U) &&
         Test_Write(REG_PPG_CONFIG_1, 0xD3U) &&     /* 32 uA range, 100 Hz, 400 us */
         Test_Write(REG_PPG_CONFIG_2, 0x18U) &&
         Test_Write(REG_LED_RANGE, 0x00U) &&
         Test_Write(REG_LED1_PA, led_current) &&
         Test_Write(REG_LED2_PA, led_current) &&
         Test_Write(REG_SYSTEM_CONTROL, SYS_FIFO_ENABLE);
}

static bool Test_SetLedCurrent(uint8_t led_current)
{
  return Test_Write(REG_LED1_PA, led_current) &&
         Test_Write(REG_LED2_PA, led_current);
}

/* Arm VDD_OOR, give it time to latch, read and disarm. INTB is released
 * again by the final status read. */
static bool Test_CheckVddOor(uint8_t *out_of_range)
{
  uint8_t status;

  if (!Test_Read(REG_INT_STATUS_2, &status, 1U) ||
      !Test_Write(REG_INT_ENABLE_2, INT2_VDD_OOR))
  {
    return false;
  }
  HAL_Delay(MAX86150_TEST_VDD_OOR_WAIT_MS);
  if (!Test_Read(REG_INT_STATUS_2, &status, 1U) ||
      !Test_Write(REG_INT_ENABLE_2, 0U))
  {
    return false;
  }
  *out_of_range = ((status & INT2_VDD_OOR) != 0U) ? 1U : 0U;
  return Test_Read(REG_INT_STATUS_2, &status, 1U);
}

static bool Test_Interrupts(void)
{
  uint8_t oor_led_off = 0U;
  uint8_t oor_led_max = 0U;
  uint8_t status;

  if (!Test_Write(REG_INT_ENABLE_2, 0U) ||
      !Test_Read(REG_INT_STATUS_1, &status, 1U) ||
      !Test_Read(REG_INT_STATUS_2, &status, 1U) ||
      !Test_Write(REG_INT_ENABLE_1, INT1_PPG_RDY))
  {
    Test_Fail(MAX86150_DIAG_REGISTER_READ_FAILED);
    return false;
  }

  /* INTB is open-drain, active low, held until INT_STATUS_1 is read. The
   * next PPG_RDY is 10 ms away, so the pin must read HIGH right after. */
  HAL_Delay(30U);
  g_max86150Test.intbLowWhenPending =
      (HAL_GPIO_ReadPin(INTB_GPIO_Port, INTB_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
  if (!Test_Read(REG_INT_STATUS_1, &status, 1U))
  {
    Test_Fail(MAX86150_DIAG_REGISTER_READ_FAILED);
    return false;
  }
  g_max86150Test.intbHighAfterClear =
      (HAL_GPIO_ReadPin(INTB_GPIO_Port, INTB_Pin) == GPIO_PIN_SET) ? 1U : 0U;

  /* VDD_ANA with the LEDs idle, then with both LEDs pulsing at full
   * current: only the second tripping points at VLED/PGND coupling into
   * the 1.8 V rail (no decoupling on this PCB) rather than a static error. */
  if (!Test_Write(REG_INT_ENABLE_1, 0U) ||
      !Test_Read(REG_INT_STATUS_1, &status, 1U) ||
      !Test_CheckVddOor(&oor_led_off) ||
      !Test_SetLedCurrent(MAX86150_TEST_LED_MAX_CODE) ||
      !Test_CheckVddOor(&oor_led_max) ||
      !Test_SetLedCurrent(0U))
  {
    Test_Fail(MAX86150_DIAG_REGISTER_READ_FAILED);
    return false;
  }
  g_max86150Test.vddOorLedOff = oor_led_off;
  g_max86150Test.vddOorLedMax = oor_led_max;

  if ((oor_led_off != 0U) || (oor_led_max != 0U))
  {
    Test_Warn(MAX86150_DIAG_VDD_OUT_OF_RANGE);
  }
  if ((g_max86150Test.intbLowWhenPending == 0U) ||
      (g_max86150Test.intbHighAfterClear == 0U))
  {
    Test_Warn(MAX86150_DIAG_INTB_NOT_TOGGLING);
  }
  return true;
}

/* Empty the FIFO, wait one window, then average what arrived. */
static bool Test_SampleWindow(uint8_t *count, uint32_t *ir_avg,
                              uint32_t *red_avg, uint32_t *peak,
                              volatile uint8_t *raw_ptrs)
{
  uint8_t write_ptr;
  uint8_t read_ptr;
  uint8_t overflow;
  uint8_t sample[6];
  uint8_t n;
  uint8_t i;
  uint32_t ir;
  uint32_t red;
  uint32_t ir_sum = 0U;
  uint32_t red_sum = 0U;

  *peak = 0U;
  if (!Test_Write(REG_FIFO_WRITE_PTR, 0U) ||
      !Test_Write(REG_FIFO_OVERFLOW, 0U) ||
      !Test_Write(REG_FIFO_READ_PTR, 0U))
  {
    return false;
  }
  HAL_Delay(MAX86150_TEST_WINDOW_MS);
  if (!Test_Read(REG_FIFO_WRITE_PTR, &write_ptr, 1U) ||
      !Test_Read(REG_FIFO_OVERFLOW, &overflow, 1U) ||
      !Test_Read(REG_FIFO_READ_PTR, &read_ptr, 1U))
  {
    return false;
  }
  if (raw_ptrs != NULL)
  {
    raw_ptrs[0] = write_ptr;
    raw_ptrs[1] = overflow;
    raw_ptrs[2] = read_ptr;
  }
  /* Full FIFO wraps write_ptr back onto read_ptr; the overflow counter
   * tells that apart from empty. */
  n = (overflow != 0U) ? 32U : (uint8_t)((write_ptr - read_ptr) & 0x1FU);

  for (i = 0U; i < n; i++)
  {
    if (!Test_Read(REG_FIFO_DATA, sample, sizeof(sample)))
    {
      return false;
    }
    ir = (((uint32_t)sample[0] << 16U) | ((uint32_t)sample[1] << 8U) |
          sample[2]) & SAMPLE_MASK;
    red = (((uint32_t)sample[3] << 16U) | ((uint32_t)sample[4] << 8U) |
           sample[5]) & SAMPLE_MASK;
    ir_sum += ir;
    red_sum += red;
    *peak = (ir > *peak) ? ir : *peak;
    *peak = (red > *peak) ? red : *peak;
  }

  *count = n;
  *ir_avg = (n > 0U) ? (ir_sum / n) : 0U;
  *red_avg = (n > 0U) ? (red_sum / n) : 0U;
  return true;
}

static bool Test_Fifo(void)
{
  uint8_t count = 0U;
  uint32_t ir = 0U;
  uint32_t red = 0U;
  uint32_t peak;

  if (!Test_ConfigurePpg(0U) || !Test_SampleWindow(&count, &ir, &red, &peak,
                                                g_max86150Test.fifoRawLedOff))
  {
    Test_Fail(MAX86150_DIAG_REGISTER_READ_FAILED);
    return false;
  }
  g_max86150Test.fifoSamplesLedOff = count;
  g_max86150Test.irLedOff = ir;
  g_max86150Test.redLedOff = red;
  if (count < MAX86150_TEST_MIN_WINDOW_SAMPLES)
  {
    Test_Fail(MAX86150_DIAG_FIFO_NOT_RUNNING);
    return false;
  }
  return true;
}

static bool Test_Led(void)
{
  uint8_t count = 0U;
  uint32_t ir = 0U;
  uint32_t red = 0U;
  uint32_t peak = 0U;

  if (!Test_SetLedCurrent(MAX86150_TEST_LED_CURRENT_CODE) ||
      !Test_SampleWindow(&count, &ir, &red, &peak, NULL) ||
      !Test_SetLedCurrent(0U))
  {
    Test_Fail(MAX86150_DIAG_REGISTER_READ_FAILED);
    return false;
  }
  g_max86150Test.fifoSamplesLedOn = count;
  g_max86150Test.irLedOn = ir;
  g_max86150Test.redLedOn = red;
  g_max86150Test.irLedDelta = (int32_t)ir - (int32_t)g_max86150Test.irLedOff;
  g_max86150Test.redLedDelta = (int32_t)red - (int32_t)g_max86150Test.redLedOff;

  if (peak >= MAX86150_TEST_SATURATION_COUNTS)
  {
    Test_Warn(MAX86150_DIAG_ADC_SATURATED);
  }
  if ((g_max86150Test.irLedDelta < MAX86150_TEST_LED_MIN_DELTA) &&
      (g_max86150Test.redLedDelta < MAX86150_TEST_LED_MIN_DELTA))
  {
    Test_Warn(MAX86150_DIAG_LED_NO_RESPONSE);
  }
  return true;
}

/* ---- Production driver path + live stream ------------------------------ */

static bool Test_Driver(max86150_optical_t *device)
{
  max86150_optical_result_t status;

  MAX86150_OpticalBind(device, &hi2c1);
  status = MAX86150_OpticalProbe(device);
  if (status == MAX86150_OPTICAL_OK)
  {
    status = MAX86150_OpticalConfigureRedIr(device,
                                            MAX86150_TEST_LED_CURRENT_CODE,
                                            MAX86150_TEST_LED_CURRENT_CODE);
  }
  if (status != MAX86150_OPTICAL_OK)
  {
    g_max86150Test.driverResult = (uint8_t)status;
    Test_Fail(MAX86150_DIAG_DRIVER_PATH_FAILED);
    return false;
  }
  return true;
}

static void Test_Stream(max86150_optical_t *device)
{
  uint32_t started_at = HAL_GetTick();
  uint32_t window_count = 0U;
  uint32_t ir_min = SAMPLE_MASK;
  uint32_t ir_max = 0U;
  uint32_t red_min = SAMPLE_MASK;
  uint32_t red_max = 0U;
  uint32_t vdd_polled_at = started_at;
  uint8_t available;
  uint8_t status;
  uint32_t red;
  uint32_t ir;

  /* ConfigureRedIr() reset the chip, so VDD_OOR is disarmed again. It
   * stays armed for the whole stream (LEDs pulsing at the app's current)
   * and is sampled every MAX86150_TEST_VDD_POLL_MS: hits == checks means
   * a steady offset, occasional hits mean dips. */
  (void)Test_Read(REG_INT_STATUS_2, &status, 1U);
  (void)Test_Write(REG_INT_ENABLE_2, INT2_VDD_OOR);

  while ((HAL_GetTick() - started_at) < MAX86150_TEST_STREAM_MS)
  {
    if ((HAL_GetTick() - vdd_polled_at) >= MAX86150_TEST_VDD_POLL_MS)
    {
      vdd_polled_at = HAL_GetTick();
      if (Test_Read(REG_INT_STATUS_2, &status, 1U))
      {
        g_max86150Test.vddOorChecks++;
        if ((status & INT2_VDD_OOR) != 0U)
        {
          g_max86150Test.vddOorHits++;
        }
      }
    }

    if (MAX86150_OpticalGetAvailableSamples(device, &available) !=
        MAX86150_OPTICAL_OK)
    {
      available = 0U;
    }
    while (available > 0U)
    {
      available--;
      if (MAX86150_OpticalReadSample(device, &red, &ir) != MAX86150_OPTICAL_OK)
      {
        break;
      }
      g_max86150Test.ir = ir;
      g_max86150Test.red = red;
      g_max86150Test.streamSamples++;

      ir_min = (ir < ir_min) ? ir : ir_min;
      ir_max = (ir > ir_max) ? ir : ir_max;
      red_min = (red < red_min) ? red : red_min;
      red_max = (red > red_max) ? red : red_max;
      if (++window_count >= MAX86150_TEST_P2P_WINDOW_SAMPLES)
      {
        g_max86150Test.irPeakToPeak = ir_max - ir_min;
        g_max86150Test.redPeakToPeak = red_max - red_min;
        window_count = 0U;
        ir_min = SAMPLE_MASK;
        ir_max = 0U;
        red_min = SAMPLE_MASK;
        red_max = 0U;
      }
    }
    HAL_Delay(MAX86150_TEST_STREAM_POLL_MS);
  }

  (void)Test_Write(REG_INT_ENABLE_2, 0U);
  (void)Test_Read(REG_INT_STATUS_2, &status, 1U);
  if (g_max86150Test.vddOorHits != 0U)
  {
    Test_Warn(MAX86150_DIAG_VDD_OUT_OF_RANGE);
  }
}

/* ---- Multimeter wiring check (MAX86150_TEST_WIRE_CHECK) ----------------- */

#if MAX86150_TEST_WIRE_CHECK
/* Push-pull on both lines is safe here: nothing else on the bus drives
 * SCL, and an idle I2C slave never drives SDA. Never returns. */
static void Test_WireCheck(void)
{
  GPIO_InitTypeDef gpio = {0};

  (void)HAL_I2C_DeInit(&hi2c1);
  HAL_GPIO_WritePin(TEST_I2C_PORT, TEST_SCL_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TEST_I2C_PORT, TEST_SDA_PIN, GPIO_PIN_RESET);
  gpio.Pin = TEST_SCL_PIN | TEST_SDA_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TEST_I2C_PORT, &gpio);

  while (1)
  {
    HAL_Delay(1000U);
    HAL_GPIO_TogglePin(TEST_I2C_PORT, TEST_SDA_PIN);
  }
}
#endif

/* ---- Entry point -------------------------------------------------------- */

void MAX86150Test_Run(void)
{
  max86150_optical_t device;

  memset((void *)&g_max86150Test, 0, sizeof(g_max86150Test));
  g_max86150Test.result = MAX86150_TEST_RESULT_RUNNING;

#if MAX86150_TEST_WIRE_CHECK
  g_max86150Test.step = MAX86150_TEST_STEP_WIRE_CHECK;
  Test_WireCheck();
#endif

  g_max86150Test.step = MAX86150_TEST_STEP_BUS_LINES;
  if (!Test_BusLines())
  {
    return;
  }

  g_max86150Test.step = MAX86150_TEST_STEP_BUS_SCAN;
  if (!Test_BusScan())
  {
    return;
  }

  g_max86150Test.step = MAX86150_TEST_STEP_ACK;
  Test_AckTrials();

  g_max86150Test.step = MAX86150_TEST_STEP_PART_ID;
  if (!Test_PartId())
  {
    return;
  }

  g_max86150Test.step = MAX86150_TEST_STEP_RESET;
  if (!Test_Reset())
  {
    return;
  }

  g_max86150Test.step = MAX86150_TEST_STEP_REG_RW;
  if (!Test_RegisterReadBack())
  {
    return;
  }

  /* FIFO before INTERRUPTS: PPG_RDY needs the ADC already converting. */
  g_max86150Test.step = MAX86150_TEST_STEP_FIFO;
  if (!Test_Fifo())
  {
    return;
  }

  g_max86150Test.step = MAX86150_TEST_STEP_INTERRUPTS;
  if (!Test_Interrupts())
  {
    return;
  }

  g_max86150Test.step = MAX86150_TEST_STEP_LED;
  if (!Test_Led())
  {
    return;
  }

  g_max86150Test.step = MAX86150_TEST_STEP_DRIVER;
  if (!Test_Driver(&device))
  {
    return;
  }

  /* Hardware verdict is known here; publish it before the long stream. */
  g_max86150Test.result = (g_max86150Test.diagnosis == MAX86150_DIAG_OK)
                              ? MAX86150_TEST_RESULT_PASS
                              : MAX86150_TEST_RESULT_WARN;

  g_max86150Test.step = MAX86150_TEST_STEP_STREAM;
  Test_Stream(&device);

  /* Hand the chip back idle; SensorManager_Start() resets and reconfigures. */
  (void)MAX86150_OpticalShutdown(&device, true);
  g_max86150Test.step = MAX86150_TEST_STEP_DONE;
}

#endif /* MAX86150_TEST_ENABLE */
