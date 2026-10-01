/**
 ******************************************************************************
 * @file    sensor_test.c
 * @brief   Boot-time overview test of every I2C1 device: ACK, ID register
 *          and one real data read per sensor.
 *
 * Steps, all on I2C1 (PB6 = SCL, PB7 = SDA):
 *
 *   BUS         PB6/PB7 levels, then a scan of 0x08-0x77.
 *   NEH7100     ACK at 0x3C.
 *   ST25DV      ACK at 0x53.
 *   MAX30208    ACK at 0x50, driver probe (PART_ID), one temperature
 *               conversion.
 *   MAX86150    ACK at 0x5E, PART_ID, driver PPG config, FIFO must fill;
 *               the chip is put back into shutdown afterwards.
 *   LIS2DUXS12  Driver init (WHO_AM_I), one acceleration sample, QVar
 *               enabled and sampled 100 times over ~1 s.
 *
 * Every device is tested even if an earlier one fails, so one run gives the
 * whole picture. The production drivers are used for the data reads, so a
 * PASS here means SensorManager can use the chip too. SensorManager_Init()
 * resets and reconfigures every sensor after the first BLE connection, so
 * nothing configured here leaks into normal operation.
 ******************************************************************************
 */
#include "sensor_test.h"

#include <stdbool.h>
#include <string.h>

#include "main.h"
#include "../../Drivers/max30208.h"
#include "../../Drivers/supercap_monitor.h"
#include "../../Drivers/Sensors/MAX86150/max86150_optical.h"
#include "../../Drivers/Sensors/LIS2DUXS12TR/lis2duxs12_motion.h"
#include "../../Drivers/Sensors/LIS2DUXS12TR/lis2duxs12_reg.h"

#define TEST_I2C_TIMEOUT_MS        10U
#define TEST_NEH7100_ADDR          0x3CU
#define TEST_ST25DV_ADDR           0x53U   /* user memory; system area is 0x57 */
#define TEST_MAX30208_ADDR         0x50U
#define TEST_MAX86150_REG_PART_ID  0xFFU
#define TEST_TEMP_TIMEOUT_MS       100U    /* conversion takes ~50 ms */
#define TEST_PPG_FILL_MS           200U    /* ~20 samples at 100 sps */
#define TEST_PPG_LED_CODE          0x24U   /* ~7 mA, same as SensorManager */
#define TEST_QVAR_SAMPLES          100U
#define TEST_QVAR_PERIOD_MS        10U     /* 100 Hz ODR */

volatile SensorTest_Report_t g_sensorTest;

static uint8_t Test_CountAcks(uint8_t address)
{
  uint8_t acks = 0U;
  uint8_t i;

  for (i = 0U; i < SENSOR_TEST_ACK_TRIALS; i++)
  {
    if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)((uint16_t)address << 1U),
                              1U, TEST_I2C_TIMEOUT_MS) == HAL_OK)
    {
      acks++;
    }
  }
  return acks;
}

static bool Test_ReadRegister(uint8_t address, uint8_t reg, uint8_t *value)
{
  return HAL_I2C_Mem_Read(&hi2c1, (uint16_t)((uint16_t)address << 1U), reg,
                          I2C_MEMADD_SIZE_8BIT, value, 1U,
                          TEST_I2C_TIMEOUT_MS) == HAL_OK;
}

/* An otherwise working chip that missed some address probes is reported as
 * ACK_UNSTABLE instead of PASS. */
static SensorTest_Status_t Test_PassOrUnstable(uint8_t acks)
{
  return (acks < SENSOR_TEST_ACK_TRIALS) ? SENSOR_TEST_ACK_UNSTABLE :
                                           SENSOR_TEST_PASS;
}

static SensorTest_Status_t Test_AckOnly(uint8_t address, volatile uint8_t *acks)
{
  *acks = Test_CountAcks(address);
  return (*acks == 0U) ? SENSOR_TEST_NO_ACK : Test_PassOrUnstable(*acks);
}

static void Test_Bus(void)
{
  uint8_t address;

  /* In I2C alternate-function mode the input register still shows the pin. */
  g_sensorTest.sclHigh = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) == GPIO_PIN_SET) ? 1U : 0U;
  g_sensorTest.sdaHigh = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_SET) ? 1U : 0U;

  g_sensorTest.scanCount = 0U;
  for (address = 0x08U; address <= 0x77U; address++)
  {
    if ((HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)((uint16_t)address << 1U),
                               1U, TEST_I2C_TIMEOUT_MS) == HAL_OK) &&
        (g_sensorTest.scanCount < SENSOR_TEST_SCAN_MAX))
    {
      g_sensorTest.scanAddr[g_sensorTest.scanCount] = address;
      g_sensorTest.scanCount++;
    }
  }
}

static SensorTest_Status_t Test_Max30208(void)
{
  uint32_t start;
  int16_t centi_c;
  temp_sensor_result_t result;

  g_sensorTest.temperatureCentiC = INT16_MIN;  /* -32768 = no reading, as on BLE */
  g_sensorTest.max30208Acks = Test_CountAcks(TEST_MAX30208_ADDR);

  /* The driver scans 0x50-0x53 and checks PART_ID itself. */
  if (TempSensor_Init() != TEMP_SENSOR_RESULT_OK)
  {
    return (g_sensorTest.max30208Acks == 0U) ? SENSOR_TEST_NO_ACK :
                                               SENSOR_TEST_BAD_ID;
  }
  g_sensorTest.max30208Addr = TempSensor_GetAddress();

  if (TempSensor_TriggerOneShot() != TEMP_SENSOR_RESULT_OK)
  {
    return SENSOR_TEST_CONFIG_FAILED;
  }
  start = HAL_GetTick();
  do
  {
    HAL_Delay(5U);
    result = TempSensor_TryReadCentiC(&centi_c);
  } while ((result == TEMP_SENSOR_RESULT_NOT_READY) &&
           ((HAL_GetTick() - start) < TEST_TEMP_TIMEOUT_MS));

  if (result != TEMP_SENSOR_RESULT_OK)
  {
    return SENSOR_TEST_NO_DATA;
  }
  g_sensorTest.temperatureCentiC = centi_c;
  return Test_PassOrUnstable(g_sensorTest.max30208Acks);
}

static SensorTest_Status_t Test_Max86150(void)
{
  max86150_optical_t device;
  uint8_t available = 0U;
  uint8_t part_id = 0U;
  uint32_t red = 0U;
  uint32_t ir = 0U;
  SensorTest_Status_t status;

  g_sensorTest.max86150Acks = Test_CountAcks(MAX86150_OPTICAL_I2C_ADDRESS);
  if (g_sensorTest.max86150Acks == 0U)
  {
    return SENSOR_TEST_NO_ACK;
  }
  if (!Test_ReadRegister(MAX86150_OPTICAL_I2C_ADDRESS,
                         TEST_MAX86150_REG_PART_ID, &part_id))
  {
    return SENSOR_TEST_BAD_ID;
  }
  g_sensorTest.max86150PartId = part_id;

  MAX86150_OpticalBind(&device, &hi2c1);
  if (MAX86150_OpticalProbe(&device) != MAX86150_OPTICAL_OK)
  {
    return SENSOR_TEST_BAD_ID;
  }
  if (MAX86150_OpticalConfigureRedIr(&device, TEST_PPG_LED_CODE,
                                     TEST_PPG_LED_CODE) != MAX86150_OPTICAL_OK)
  {
    return SENSOR_TEST_CONFIG_FAILED;
  }

  HAL_Delay(TEST_PPG_FILL_MS);
  status = SENSOR_TEST_NO_DATA;
  if ((MAX86150_OpticalGetAvailableSamples(&device, &available) ==
       MAX86150_OPTICAL_OK) && (available > 0U))
  {
    g_sensorTest.ppgSamples = available;
    while ((available > 0U) &&
           (MAX86150_OpticalReadSample(&device, &red, &ir) == MAX86150_OPTICAL_OK))
    {
      available--;
    }
    g_sensorTest.ppgRed = red;
    g_sensorTest.ppgIr = ir;
    status = Test_PassOrUnstable(g_sensorTest.max86150Acks);
  }

  /* LEDs off until SensorManager starts a measurement. */
  (void)MAX86150_OpticalShutdown(&device, true);
  return status;
}

static SensorTest_Status_t Test_Lis2duxs12(void)
{
  lis2duxs12_acceleration_t acceleration;
  uint8_t who_am_i = 0U;
  int16_t qvar;
  int16_t qvar_min = INT16_MAX;
  int16_t qvar_max = INT16_MIN;
  uint8_t qvar_reads = 0U;
  uint8_t i;

  if (LIS2DUXS12_MotionInit(&hi2c1) != LIS2DUXS12_MOTION_OK)
  {
    /* Count ACKs on both possible addresses to tell NO_ACK from BAD_ID. */
    g_sensorTest.lis2duxs12Acks = Test_CountAcks(LIS2DUXS12_I2C_ADD_L >> 1U);
    if (g_sensorTest.lis2duxs12Acks == 0U)
    {
      g_sensorTest.lis2duxs12Acks = Test_CountAcks(LIS2DUXS12_I2C_ADD_H >> 1U);
    }
    return (g_sensorTest.lis2duxs12Acks == 0U) ? SENSOR_TEST_NO_ACK :
                                                 SENSOR_TEST_BAD_ID;
  }
  g_sensorTest.lis2duxs12Addr = (uint8_t)(LIS2DUXS12_MotionGetHalAddress() >> 1U);
  g_sensorTest.lis2duxs12Acks = Test_CountAcks(g_sensorTest.lis2duxs12Addr);
  if (Test_ReadRegister(g_sensorTest.lis2duxs12Addr, LIS2DUXS12_WHO_AM_I,
                        &who_am_i))
  {
    g_sensorTest.lis2duxs12WhoAmI = who_am_i;
  }

  HAL_Delay(30U);  /* a few samples at the 100 Hz ODR */
  if (LIS2DUXS12_MotionReadAcceleration(&acceleration) != LIS2DUXS12_MOTION_OK)
  {
    return SENSOR_TEST_NO_DATA;
  }
  for (i = 0U; i < 3U; i++)
  {
    g_sensorTest.accelMg[i] = acceleration.mg[i];
  }

  if (LIS2DUXS12_MotionInitQvar() != LIS2DUXS12_MOTION_OK)
  {
    return SENSOR_TEST_CONFIG_FAILED;
  }
  HAL_Delay(30U);
  for (i = 0U; i < TEST_QVAR_SAMPLES; i++)
  {
    if (LIS2DUXS12_MotionReadQvar(&qvar) == LIS2DUXS12_MOTION_OK)
    {
      qvar_reads++;
      qvar_min = (qvar < qvar_min) ? qvar : qvar_min;
      qvar_max = (qvar > qvar_max) ? qvar : qvar_max;
    }
    HAL_Delay(TEST_QVAR_PERIOD_MS);
  }
  if (qvar_reads == 0U)
  {
    return SENSOR_TEST_NO_DATA;
  }
  g_sensorTest.qvarMin = qvar_min;
  g_sensorTest.qvarMax = qvar_max;
  g_sensorTest.qvarPeakToPeak = (uint16_t)((int32_t)qvar_max - (int32_t)qvar_min);

  /* All-zero XYZ means the output registers never updated. */
  if ((acceleration.mg[0] == 0) && (acceleration.mg[1] == 0) &&
      (acceleration.mg[2] == 0))
  {
    return SENSOR_TEST_NO_DATA;
  }
  return Test_PassOrUnstable(g_sensorTest.lis2duxs12Acks);
}

static uint8_t Test_IsFail(SensorTest_Status_t status)
{
  return ((status == SENSOR_TEST_PASS) ||
          (status == SENSOR_TEST_ACK_UNSTABLE)) ? 0U : 1U;
}

static void Test_RunOnce(void)
{
  uint32_t runs = g_sensorTest.runs;
  uint32_t start = HAL_GetTick();

  memset((void *)&g_sensorTest, 0, sizeof(g_sensorTest));
  g_sensorTest.runs = runs + 1U;

  Test_Bus();
  if (SupercapMonitor_Init())
  {
    g_sensorTest.supercapMv = SupercapMonitor_ReadMillivolts();
  }
  g_sensorTest.neh7100 = Test_AckOnly(TEST_NEH7100_ADDR, &g_sensorTest.neh7100Acks);
  g_sensorTest.st25dv = Test_AckOnly(TEST_ST25DV_ADDR, &g_sensorTest.st25dvAcks);
  g_sensorTest.max30208 = Test_Max30208();
  g_sensorTest.max86150 = Test_Max86150();
  g_sensorTest.lis2duxs12 = Test_Lis2duxs12();

  g_sensorTest.failCount = (uint8_t)(Test_IsFail(g_sensorTest.neh7100) +
                                     Test_IsFail(g_sensorTest.st25dv) +
                                     Test_IsFail(g_sensorTest.max30208) +
                                     Test_IsFail(g_sensorTest.max86150) +
                                     Test_IsFail(g_sensorTest.lis2duxs12));
  g_sensorTest.durationMs = HAL_GetTick() - start;
}

void SensorTest_Run(void)
{
  g_sensorTest.runs = 0U;
  Test_RunOnce();
#if SENSOR_TEST_LOOP
  for (;;)
  {
    HAL_Delay(SENSOR_TEST_LOOP_PERIOD_MS);
    Test_RunOnce();
  }
#endif
  /* Breakpoint here to read g_sensorTest once per boot. */
  __NOP();
}
