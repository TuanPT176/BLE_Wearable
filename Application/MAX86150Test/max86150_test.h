/**
 ******************************************************************************
 * @file    max86150_test.h
 * @brief   Standalone I2C/hardware bring-up test for the MAX86150 PPG sensor
 *          on the custom wearable PCB.
 ******************************************************************************
 */
#ifndef MAX86150_TEST_H
#define MAX86150_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Set to 0 to compile the test out without removing the call site in main.c. */
#define MAX86150_TEST_ENABLE 1

/* While enabled, MAX86150Test_Run() blocks boot for roughly
 * MAX86150_TEST_STREAM_MS (PPG, finger on the sensor) followed by
 * MAX86150_TEST_ECG_MS (ECG, both hands on ECG_P/ECG_N) - see
 * max86150_test.c. */

/* 1 = replace the whole test with a multimeter wiring check that never
 * returns: PB6/SCL is driven steadily HIGH (3.3V) and PB7/SDA toggles
 * 0V <-> 3.3V every second. Measured at the MAX86150 module pins: steady
 * 3.3V on SCL + blinking SDA = wired right; swapped readings = SCL/SDA
 * crossed; ~0V steady = that wire never reaches the MCU pin. */
#define MAX86150_TEST_WIRE_CHECK 0

/*
 * Same reporting approach as LoRaTest: this board has no trace sink (PA1 is
 * SX1262 DIO1, __io_putchar() is a stub), so the result lives in one global
 * struct. In STM32CubeIDE add a Live Expression for `g_max86150Test` and
 * expand it (Window > Show View > Live Expressions), or break at the end of
 * MAX86150Test_Run().
 *
 * Reading the report: `result` + `diagnosis` give the verdict; `step` is the
 * step that was running when the test stopped (DONE on success). The other
 * fields are the raw evidence behind the diagnosis.
 */
typedef enum
{
  MAX86150_TEST_STEP_NONE = 0,
  MAX86150_TEST_STEP_WIRE_CHECK,   /* only with MAX86150_TEST_WIRE_CHECK = 1 */
  MAX86150_TEST_STEP_BUS_LINES,    /* raw SCL/SDA levels, pull-up check, bus recovery */
  MAX86150_TEST_STEP_BUS_SCAN,     /* who ACKs on I2C1 (0x08-0x77) */
  MAX86150_TEST_STEP_ACK,          /* repeated address ACK at 0x5E */
  MAX86150_TEST_STEP_PART_ID,      /* register 0xFF == 0x1E */
  MAX86150_TEST_STEP_RESET,        /* soft reset bit self-clears */
  MAX86150_TEST_STEP_REG_RW,       /* write/read-back pattern */
  MAX86150_TEST_STEP_FIFO,         /* FIFO fills at the 100 Hz PPG rate, LEDs off */
  MAX86150_TEST_STEP_INTERRUPTS,   /* INTB (PB4) toggling + VDD_OOR flag */
  MAX86150_TEST_STEP_LED,          /* LEDs on vs off: optical path responds */
  MAX86150_TEST_STEP_DRIVER,       /* production max86150_optical driver path */
  MAX86150_TEST_STEP_STREAM,       /* live IR/Red samples for Live Expressions */
  MAX86150_TEST_STEP_ECG,          /* ECG mode via the driver: rate, overflow, live signal */
  MAX86150_TEST_STEP_DONE
} MAX86150Test_Step_t;

typedef enum
{
  MAX86150_TEST_RESULT_RUNNING = 0,
  MAX86150_TEST_RESULT_PASS,
  MAX86150_TEST_RESULT_WARN,       /* chip works, but see `diagnosis` */
  MAX86150_TEST_RESULT_FAIL
} MAX86150Test_Result_t;

/* First failure (overrides any warning) or, with no failure, first warning. */
typedef enum
{
  MAX86150_DIAG_OK = 0,
  /* FAIL */
  MAX86150_DIAG_BUS_STUCK_LOW,        /* SCL/SDA read LOW even with MCU pull-up + 9-clock recovery: short to GND or a device holding the bus */
  MAX86150_DIAG_I2C_REINIT_FAILED,    /* HAL_I2C_Init() failed after the line check */
  MAX86150_DIAG_BUS_NO_DEVICES,       /* nothing ACKs on I2C1: shared bus problem (pull-ups, SCL/SDA routing, sensor rail off) */
  MAX86150_DIAG_NO_ACK_AT_0x5E,       /* other devices ACK, MAX86150 does not: its VDD (1.8V)/GND/solder/SDA-SCL pads */
  MAX86150_DIAG_REGISTER_READ_FAILED, /* address ACKs but a register read fails */
  MAX86150_DIAG_WRONG_PART_ID,        /* something at 0x5E, but PART_ID != 0x1E */
  MAX86150_DIAG_RESET_TIMEOUT,        /* RESET bit never self-cleared */
  MAX86150_DIAG_REG_RW_MISMATCH,      /* written value not read back */
  MAX86150_DIAG_FIFO_NOT_RUNNING,     /* FIFO does not fill: PPG ADC not converting */
  MAX86150_DIAG_DRIVER_PATH_FAILED,   /* raw test passed, max86150_optical.c failed - see driverResult */
  MAX86150_DIAG_ECG_CONFIG_FAILED,    /* MAX86150_EcgConfigure() failed - see ecgDriverResult */
  MAX86150_DIAG_ECG_NOT_RUNNING,      /* ECG FIFO delivers < half the expected 200 sps */
  /* WARN */
  MAX86150_DIAG_ACK_INTERMITTENT,     /* 0x5E ACKs only some of the time: marginal solder/pull-up */
  MAX86150_DIAG_NO_EXTERNAL_PULLUP,   /* bus only held up by the MCU's weak internal pull-ups */
  MAX86150_DIAG_VDD_OUT_OF_RANGE,     /* chip's own VDD_OOR flag set: check its 1.8V VDD rail */
  MAX86150_DIAG_INTB_NOT_TOGGLING,    /* PB4 does not follow the PPG_RDY interrupt */
  MAX86150_DIAG_ADC_SATURATED,        /* samples at full scale: lower LED current */
  MAX86150_DIAG_LED_NO_RESPONSE,      /* LEDs on ~= LEDs off: VLED rail/LED pads, or nothing over the sensor */
  MAX86150_DIAG_ECG_RATE_OFF,         /* ECG runs, but not at 180-220 sps: ECG_CONFIG1 assumption wrong */
  MAX86150_DIAG_ECG_OVERFLOW,         /* FIFO rolled over although drained every 20 ms */
  MAX86150_DIAG_ECG_FLAT,             /* ECG value never changes: front-end not converting */
  MAX86150_DIAG_ECG_SATURATED         /* ECG pinned at full scale most of the time: electrodes not touched / lead off */
} MAX86150Test_Diag_t;

#define MAX86150_TEST_SCAN_MAX 16U

typedef struct
{
  MAX86150Test_Step_t step;
  MAX86150Test_Result_t result;
  MAX86150Test_Diag_t diagnosis;
  uint32_t halI2cError;          /* hi2c1.ErrorCode at the first failed register access (HAL_I2C_ERROR_AF=0x04 is a NACK) */

  /* BUS_LINES: 1 = line read HIGH */
  uint8_t sclHighWithMcuPullUp;  /* expect 1; 0 = line held/shorted low */
  uint8_t sdaHighWithMcuPullUp;
  uint8_t sclHighWithMcuPullDown; /* expect 1; 0 = no external pull-up resistor */
  uint8_t sdaHighWithMcuPullDown;
  uint8_t busRecoveryClocks;     /* SCL pulses needed to release a stuck SDA (0 = not needed) */

  /* BUS_SCAN */
  uint8_t scanCount;
  uint8_t scanAddr[MAX86150_TEST_SCAN_MAX]; /* 7-bit addresses that ACKed */

  /* ACK / PART_ID / RESET / REG_RW */
  uint8_t ackCount;              /* out of MAX86150_TEST_ACK_TRIALS */
  uint8_t partId;                /* expect 0x1E */
  uint8_t intStatus1AtBoot;      /* bit0 PWR_RDY = chip saw a clean power-up */
  uint8_t intStatus2AtBoot;
  uint32_t resetMs;
  uint8_t regRwOk;

  /* INTERRUPTS */
  uint8_t intbLowWhenPending;    /* expect 1 */
  uint8_t intbHighAfterClear;    /* expect 1 */
  uint8_t vddOorLedOff;          /* VDD_OOR latched with LEDs at 0 mA; expect 0 */
  uint8_t vddOorLedMax;          /* same with both LEDs at ~51 mA (loads VLED/PGND); expect 0 */

  /* FIFO / LED: averages of one ~200 ms window (raw 19-bit ADC counts) */
  uint8_t fifoSamplesLedOff;     /* expect ~20 at 100 Hz */
  uint8_t fifoSamplesLedOn;
  uint8_t fifoRawLedOff[3];      /* FIFO_WR_PTR, OVF_COUNTER, FIFO_RD_PTR at the end of the LED-off window */
  uint32_t irLedOff;
  uint32_t redLedOff;
  uint32_t irLedOn;
  uint32_t redLedOn;
  int32_t irLedDelta;            /* LED on - LED off; large with a finger on the sensor */
  int32_t redLedDelta;

  /* DRIVER */
  uint8_t driverResult;          /* max86150_optical_result_t of the first failing driver call */

  /* STREAM: updated live for MAX86150_TEST_STREAM_MS */
  uint32_t ir;
  uint32_t red;
  uint32_t irPeakToPeak;         /* over the last ~2 s: pulse amplitude with a finger on */
  uint32_t redPeakToPeak;
  uint32_t streamSamples;        /* expect ~100/s */
  uint32_t vddOorChecks;         /* VDD_OOR polls during the stream (every 500 ms, LEDs on) */
  uint32_t vddOorHits;           /* polls that found VDD_OOR latched: 0 = rail OK, == checks = steady offset, in between = dips */

  /* ECG: MAX86150_TEST_ECG_MS through the production driver, raw 18-bit
   * values (+/-131071). Touch ECG_P and ECG_N (one per hand) to see the
   * heartbeat in ecgPeakToPeak. */
  uint8_t ecgDriverResult;       /* max86150_optical_result_t of the failing ECG driver call */
  uint32_t ecgSamples;
  uint32_t ecgRateHz;            /* expect ~200 */
  uint32_t ecgOverflows;         /* expect 0 */
  int32_t ecg;                   /* latest sample */
  uint32_t ecgPeakToPeak;        /* over the last ~1 s */
  uint32_t ecgSaturatedSamples;  /* samples at +/- full scale */
} MAX86150Test_Report_t;

extern volatile MAX86150Test_Report_t g_max86150Test;

/**
 * @brief Blocking hardware bring-up test for the MAX86150 on I2C1. Call once
 *        from main() after MX_I2C1_Init() and before anything else uses the
 *        bus. Leaves I2C1 initialised and the MAX86150 in shutdown, so the
 *        normal SensorManager start-up still works afterwards.
 */
void MAX86150Test_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* MAX86150_TEST_H */
