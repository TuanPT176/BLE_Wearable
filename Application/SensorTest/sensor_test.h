/**
 ******************************************************************************
 * @file    sensor_test.h
 * @brief   Boot-time overview test of every I2C1 device: ACK, ID register
 *          and one real data read per sensor.
 ******************************************************************************
 */
#ifndef SENSOR_TEST_H
#define SENSOR_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 1 = main() runs SensorTest_Run() right after the peripheral init, before
 * NFC/BLE start. It blocks boot for about 1.5 s, then the firmware boots as
 * usual. 0 = compiled out. */
#define SENSOR_TEST_ENABLE 1

/* 1 = never boot: repeat the test every SENSOR_TEST_LOOP_PERIOD_MS so the
 * Live Expressions keep updating (touch the QVar electrode, move the board,
 * put a finger on the MAX86150). No BLE in this mode. */
#define SENSOR_TEST_LOOP 0
#define SENSOR_TEST_LOOP_PERIOD_MS 1000U

/*
 * The board has no UART, so the result lives in `g_sensorTest`: add it as a
 * Live Expression in STM32CubeIDE (Window > Show View > Live Expressions) or
 * break at the end of SensorTest_Run().
 *
 * Start with `failCount`, then the status of each device; the other fields
 * are the evidence behind that status.
 */
typedef enum
{
  SENSOR_TEST_NOT_RUN = 0,
  SENSOR_TEST_PASS,
  SENSOR_TEST_ACK_UNSTABLE,   /* everything works, but the address ACKed only some probes: solder/pull-up/supply */
  SENSOR_TEST_NO_ACK,         /* address never ACKs: VDD/GND/solder of that chip */
  SENSOR_TEST_BAD_ID,         /* ACKs, but the ID register is wrong or unreadable */
  SENSOR_TEST_CONFIG_FAILED,  /* ID fine, the driver's configuration writes failed */
  SENSOR_TEST_NO_DATA         /* configured, but no sample arrived in time */
} SensorTest_Status_t;

#define SENSOR_TEST_ACK_TRIALS 10U
#define SENSOR_TEST_SCAN_MAX   16U

typedef struct
{
  uint32_t runs;
  uint32_t durationMs;
  uint8_t failCount;              /* devices whose status is not PASS/ACK_UNSTABLE (of 5) */

  /* Bus: PB6/PB7 levels before the test, 1 = HIGH (idle bus must be 1/1) */
  uint8_t sclHigh;
  uint8_t sdaHigh;
  uint8_t scanCount;
  uint8_t scanAddr[SENSOR_TEST_SCAN_MAX]; /* 7-bit addresses that ACKed */

  /* Supercap ADC (not I2C, read for the power picture) */
  uint16_t supercapMv;

  /* NEH7100 PMIC, 0x3C: ACK only */
  SensorTest_Status_t neh7100;
  uint8_t neh7100Acks;

  /* ST25DV NFC tag, 0x53: ACK only */
  SensorTest_Status_t st25dv;
  uint8_t st25dvAcks;

  /* MAX30208 temperature, expected at 0x50 (GPIO0/1 floating) */
  SensorTest_Status_t max30208;
  uint8_t max30208Acks;           /* at 0x50 */
  uint8_t max30208Addr;           /* where the driver found it, 0 = not found */
  int16_t temperatureCentiC;      /* 3652 = 36.52 degC, -32768 = no reading */

  /* MAX86150 PPG/ECG, 0x5E */
  SensorTest_Status_t max86150;
  uint8_t max86150Acks;
  uint8_t max86150PartId;         /* expect 0x1E */
  uint8_t ppgSamples;             /* FIFO samples after ~200 ms at 100 sps (expect ~20) */
  uint32_t ppgRed;                /* last sample; finger on the sensor >> no finger */
  uint32_t ppgIr;

  /* LIS2DUXS12TR accelerometer + QVar, 0x18/0x19 */
  SensorTest_Status_t lis2duxs12;
  uint8_t lis2duxs12Acks;
  uint8_t lis2duxs12Addr;         /* 7-bit, 0 = not found */
  uint8_t lis2duxs12WhoAmI;       /* expect 0x47 */
  int32_t accelMg[3];             /* board at rest: magnitude ~1000 mg */
  int16_t qvarMin;                /* 100 samples over ~1 s */
  int16_t qvarMax;
  uint16_t qvarPeakToPeak;        /* wear detector: >= 600 worn, <= 300 not worn */
} SensorTest_Report_t;

extern volatile SensorTest_Report_t g_sensorTest;

void SensorTest_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_TEST_H */
