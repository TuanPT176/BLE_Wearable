#ifndef MAX86150_OPTICAL_H
#define MAX86150_OPTICAL_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32wb0x_hal.h"

#define MAX86150_OPTICAL_I2C_ADDRESS       0x5EU
#define MAX86150_OPTICAL_EXPECTED_PART_ID  0x1EU

typedef enum
{
  MAX86150_OPTICAL_OK = 0,
  MAX86150_OPTICAL_NOT_PRESENT,
  MAX86150_OPTICAL_BUS_ERROR,
  MAX86150_OPTICAL_TIMEOUT,
  MAX86150_OPTICAL_INVALID_ARGUMENT
} max86150_optical_result_t;

typedef struct
{
  I2C_HandleTypeDef *i2c;
  uint8_t address;
  uint32_t timeout_ms;
  bool present;
} max86150_optical_t;

/*
 * STM32 HAL port for the MAX86150. Two mutually exclusive modes, each
 * starting with a soft reset: Red/IR PPG (MAX86150_OpticalConfigureRedIr)
 * and single-lead ECG on ECG_P/ECG_N (MAX86150_EcgConfigure).
 */
void MAX86150_OpticalBind(max86150_optical_t *device,
                          I2C_HandleTypeDef *i2c);
max86150_optical_result_t MAX86150_OpticalProbe(max86150_optical_t *device);
max86150_optical_result_t MAX86150_OpticalConfigureRedIr(
    max86150_optical_t *device,
    uint8_t red_current,
    uint8_t ir_current);
/* FIFO_WR_PTR/FIFO_RD_PTR delta, wrapped to the 32-deep FIFO: how many
 * complete IR+RED sample pairs are waiting to be read (0-31). Call this
 * before OpticalReadSample() so a call with an empty FIFO isn't mistaken
 * for a fresh sample (the read pointer does not advance past the write
 * pointer, so re-reading FIFO_DATA on an empty FIFO just repeats the last
 * sample). */
max86150_optical_result_t MAX86150_OpticalGetAvailableSamples(
    max86150_optical_t *device,
    uint8_t *count);
max86150_optical_result_t MAX86150_OpticalReadSample(
    max86150_optical_t *device,
    uint32_t *red,
    uint32_t *ir);
max86150_optical_result_t MAX86150_OpticalShutdown(
    max86150_optical_t *device,
    bool enable);

#define MAX86150_ECG_FIFO_DEPTH  32U

/* ECG-only FIFO (LEDs off) at 200 sps, IA gain 9.5 x PGA gain 8 = 76 V/V.
 * Samples are 18-bit two's complement. */
max86150_optical_result_t MAX86150_EcgConfigure(max86150_optical_t *device);
/* Burst-reads up to max_samples waiting ECG samples, sign-extended. Drain
 * at least every 160 ms (32 samples at 200 sps); *overflowed reports that
 * the FIFO filled and rolled over since the last call, losing samples. */
max86150_optical_result_t MAX86150_EcgReadSamples(
    max86150_optical_t *device,
    int32_t *samples,
    uint8_t max_samples,
    uint8_t *count,
    bool *overflowed);

/* Diagnostics (ECG noise hunt, see Application/SensorManager/ecg_diag.h). */
/* Writes 0 to LED1_PA (0x11), LED2_PA (0x12) and LED_PILOT_PA (0x15). */
max86150_optical_result_t MAX86150_LedsOff(max86150_optical_t *device);
max86150_optical_result_t MAX86150_ReadRegister(max86150_optical_t *device,
                                                uint8_t reg,
                                                uint8_t *value);

#endif /* MAX86150_OPTICAL_H */
