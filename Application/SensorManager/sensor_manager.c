#include "sensor_manager.h"

#include <stddef.h>
#include <math.h>

#include "main.h"
#include "../NEH7100/neh7100.h"
#include "../../Drivers/max30208.h"
#include "../../Drivers/supercap_monitor.h"
#include "../../Drivers/Sensors/LIS2DUXS12TR/lis2duxs12_motion.h"
#include "../../Drivers/Sensors/MAX86150/max86150_optical.h"
#include "../NFC/nfc_log.h"
#include "../../STM32_BLE/App/app_ble.h"
#include "../DeviceTime/device_time.h"
#include <string.h>

/*
 * Driver implementation bundle
 * ----------------------------
 * This CubeIDE project keeps application sources as linked resources. Some
 * existing Eclipse workspaces cache the old .project description and silently
 * omit newly-added linked .c files from objects.list. SensorManager is an
 * original, stable build resource, so the mandatory sensor implementations are
 * compiled in this translation unit. Do not also add these driver .c files as
 * standalone build resources.
 */
#include "../../Drivers/max30208.c"
#include "../../Drivers/Sensors/LIS2DUXS12TR/lis2duxs12_reg.c"
#include "../PowerPolicy/power_policy.c"
#include "../DataRecovery/data_recovery_manager.c"
#include "../../Drivers/Sensors/LIS2DUXS12TR/lis2duxs12_platform.c"
#include "../../Drivers/Sensors/LIS2DUXS12TR/lis2duxs12_motion.c"
/* max86150_optical.c is already a standalone CubeIDE build resource
 * (unlike the LIS2DUXS12TR files above) - it must NOT be bundled here too,
 * or it gets compiled twice and the linker reports duplicate symbols. */

#include "../NFC/nfc_config.c"
#include "../NFC/nfc_io.c"
#include "../NFC/nfc_log.c"
#include "../NFC/nfc_manager.c"
#include "../../Drivers/ST25DV/st25dv.c"
#include "../../Drivers/ST25DV/st25dv_reg.c"

/* Same reasoning as above: SX1262/LoRaWAN bring-up code, bundled here
 * rather than added as standalone build resources. */
#include "../../Drivers/SX1262/sx126x_hal.c"
#include "../../Drivers/SX1262/sx126x.c"
#include "../LoRaWAN/lora_radio.c"

#define TEMPERATURE_FIRST_POLL_DELAY_MS   20U
#define TEMPERATURE_RETRY_DELAY_MS         5U
#define TEMPERATURE_CONVERSION_TIMEOUT_MS 60U
/* SensorManager_Process() runs once per WEARABLE_SENSOR_PERIOD_MS (~1s);
 * re-probe the bus for a missing MAX30208 every this many calls instead of
 * every call, so a genuinely absent sensor doesn't waste bus time. */
#define TEMPERATURE_REPROBE_INTERVAL_CALLS 5U
/* Same idea for LIS2DUXS12TR (accel/QVar) and MAX86150 (PPG): unlike the
 * temperature path, neither previously had any recovery once a boot-time
 * probe/config attempt failed, so a transient I2C glitch at startup would
 * leave accel/QVar/HR/SpO2 stuck forever (mock HR, flat SpO2, zeroed accel,
 * QVar wear flag never set). */
#define MOTION_REPROBE_INTERVAL_CALLS 5U
/* Shorter than the motion/temperature reprobe interval: an I2C probe + FIFO
 * config write is cheap, and while diagnosing a MAX86150 that never comes up
 * a faster retry gives quicker feedback on whether it is a transient bus
 * glitch or a persistent (wiring/hardware) failure. */
#define OPTICAL_REPROBE_INTERVAL_CALLS 2U

/*
 * MAX86150 Red/IR optical (PPG) pipeline.
 * ----------------------------------------
 * The FIFO is drained every OPTICAL_DRAIN_INTERVAL_MS via the same
 * SensorManager_ProcessAsync()/GetAsyncDelayMs() recurring async step used
 * for the temperature conversion polling, so no extra sequencer task/timer
 * is needed - see SensorManager_GetAsyncDelayMs(). At the configured 100 Hz
 * PPG ODR (OPTICAL_SAMPLE_PERIOD_MS), 200 ms leaves comfortable headroom
 * under the 32-sample FIFO before it rolls over.
 *
 * HR is a simple adaptive-threshold peak detector on the IR AC component;
 * SpO2 is the standard AC/DC ratio-of-ratios against a generic published
 * calibration curve. Both are heuristics tuned for typical PPG shapes, not
 * a clinical-grade algorithm - OPTICAL_PULSE_SIGN, the threshold fractions
 * and the SpO2 curve coefficients should be re-tuned/calibrated against a
 * reference pulse oximeter on the real enclosure/skin contact.
 */
#define OPTICAL_DRAIN_INTERVAL_MS        200U
#define OPTICAL_SAMPLE_PERIOD_MS          10U   /* fixed by the 100 Hz PPG ODR below */
#define OPTICAL_DEFAULT_LED_CURRENT_CODE 0x24U  /* ~7 mA (0.2 mA/LSB); tune per enclosure */
#define OPTICAL_DC_ALPHA                0.05f   /* baseline EMA smoothing factor */
#define OPTICAL_ENVELOPE_ALPHA          0.03f   /* pulse-amplitude envelope EMA factor */
#define OPTICAL_PULSE_SIGN            (-1.0f)   /* flip if beats aren't detected on your optical path */
#define OPTICAL_THRESHOLD_HIGH_FRAC      0.5f
#define OPTICAL_THRESHOLD_LOW_FRAC      0.25f
#define OPTICAL_MIN_ENVELOPE            50.0f   /* raw ADC counts; below this = noise/no perfusion */
#define OPTICAL_MIN_DC_FOR_VALID      2000.0f   /* raw ADC counts; below this = sensor not worn */
#define OPTICAL_MIN_BPM                   30U
#define OPTICAL_MAX_BPM                  220U
#define OPTICAL_BEAT_HISTORY_LEN           4U
#define OPTICAL_SPO2_WINDOW_DRAINS         5U   /* ~1s of samples before recomputing SpO2 */

/* QVar (LIS2DUXS12TR AH_QVAR channel) wear detection.
 * On real hardware the raw channel previously sat on a large, erratic offset
 * (tens of thousands of LSB) regardless of touch - later traced to
 * lis2duxs12_motion.c using an older register-driver snapshot whose
 * ah_qvar_data_get() read OUT_T_AH_QVAR_L/H directly, instead of priming the
 * read by first reading the preceding OUT_Z_H register in the same I2C
 * burst (what the chip needs to latch a coherent QVar sample - see ST's own
 * lis2duxs12_reg.c). That has since been fixed by switching to ST's current
 * official lis2duxs12-pid driver. This baseline-relative deviation check is
 * kept anyway as a robustness measure (the channel may still carry a
 * nonzero DC offset even when read correctly), but ALPHA/THRESHOLD are a
 * first guess, not calibrated against a real touch/release capture - expect
 * to retune once you have one with clearly marked touch vs release
 * segments. */
#define QVAR_BASELINE_ALPHA              0.05f
#define QVAR_WEAR_DEVIATION_THRESHOLD  500.0f

static wearable_sensor_data_t latest_data;
static bool initialized;
static bool running;
static sensor_temperature_status_t temperature_status;
static sensor_motion_status_t motion_status;
static sensor_optical_status_t optical_status;
static uint32_t temperature_conversion_elapsed_ms;
static uint32_t temperature_async_delay_ms;
static uint32_t temperature_reprobe_calls;
static uint32_t motion_reprobe_calls;
static uint32_t optical_reprobe_calls;
static uint32_t motion_delay_ms;
static float qvar_baseline;
static bool qvar_baseline_ready;

static max86150_optical_t optical_device;
static bool optical_baseline_ready;
static float optical_ir_dc;
static float optical_red_dc;
static float optical_envelope;
static bool optical_above_threshold;
static bool optical_have_last_beat;
static uint32_t optical_last_beat_ms;
static uint32_t optical_sample_index;
static uint32_t optical_beat_intervals_ms[OPTICAL_BEAT_HISTORY_LEN];
static uint8_t optical_beat_history_index;
static uint8_t optical_beat_history_count;
static float optical_ir_ac_sumsq;
static float optical_red_ac_sumsq;
static uint32_t optical_spo2_window_samples;
static uint32_t optical_drain_calls;

static void SensorManager_StartTemperatureConversion(void)
{
  if ((!running) ||
      ((temperature_status != SENSOR_TEMPERATURE_IDLE) &&
       (temperature_status != SENSOR_TEMPERATURE_VALID) &&
       (temperature_status != SENSOR_TEMPERATURE_TIMEOUT) &&
       (temperature_status != SENSOR_TEMPERATURE_BUS_ERROR)))
  {
    return;
  }

  if (TempSensor_TriggerOneShot() == TEMP_SENSOR_RESULT_OK)
  {
    temperature_conversion_elapsed_ms = 0U;
    temperature_async_delay_ms = TEMPERATURE_FIRST_POLL_DELAY_MS;
    temperature_status = SENSOR_TEMPERATURE_CONVERTING;
  }
  else
  {
    temperature_async_delay_ms = 0U;
    temperature_status = SENSOR_TEMPERATURE_BUS_ERROR;
  }
}

/*
 * MAX30208's I2C address only ever has 2 free bits (GPIO1/GPIO0 pin state
 * at the START condition -> Table 3 of the datasheet), so 0x50-0x53 is the
 * complete set - there is no wider MAX30208-specific range to scan. This is
 * a generic diagnostic: it ACK-probes every valid 7-bit address on I2C1 so a
 * boot log can show what actually answers on the shared bus (useful to
 * confirm the part is missing/dead/mis-wired rather than just outside the
 * 0x50-0x53 window).
 */
static void SensorManager_DebugScanI2CBus(void)
{
  uint8_t address;

  APP_DBG_MSG("-- I2C1 bus scan (0x08-0x77):\n");
  for (address = 0x08U; address <= 0x77U; address++)
  {
    if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)((uint16_t)address << 1U),
                              1U, 2U) == HAL_OK)
    {
      APP_DBG_MSG("   ACK at 0x%02x\n", (unsigned int)address);
    }
  }
}

static void SensorManager_QvarResetBaseline(void)
{
  qvar_baseline_ready = false;
}

/*
 * Software-only QVar diagnostic - no multimeter/scope needed, just read the
 * UART debug log. Two checks:
 *
 * 1) AH_QVAR disabled: OUT_T_AH_QVAR_L/H then reports plain ambient
 *    temperature (raw/355.5 + 25 = deg C), a small number - nowhere near the
 *    ~32000 seen with AH_QVAR enabled. If this ALSO reads ~32000, the fault
 *    is upstream of the analog front-end (register write not taking effect,
 *    stuck I2C value, wrong register), not the QVar circuit itself.
 * 2) Gain sweep 0.5x/1x/2x/4x at the lowest (most stable) input impedance:
 *    a genuine analog signal should scale roughly with gain. If the reading
 *    stays flat/unrelated across gains, that points at a stuck value or
 *    hardware fault rather than a real (if noisy) analog signal.
 *
 * Restores the normal operating config (ST's own reference default: gain
 * 0.5x, 520MOhm, enabled - see lis2duxs12_qvar_read_data.c) and resets the
 * wear baseline before returning.
 */
static void SensorManager_QvarSelfTest(void)
{
  int16_t qvar_raw;
  uint8_t i;
  static const lis2duxs12_ah_qvar_gain_t gains[4] = {
    LIS2DUXS12_GAIN_0_5, LIS2DUXS12_GAIN_1, LIS2DUXS12_GAIN_2, LIS2DUXS12_GAIN_4
  };
  static const uint16_t gain_x10[4] = { 5U, 10U, 20U, 40U };
  uint8_t g;

  APP_DBG_MSG("== QVar self-test start ==\n");

  if (LIS2DUXS12_MotionConfigureQvar(false, LIS2DUXS12_GAIN_1, LIS2DUXS12_75MOhm) ==
      LIS2DUXS12_MOTION_OK)
  {
    HAL_Delay(50U);
    for (i = 0U; i < 3U; i++)
    {
      if (LIS2DUXS12_MotionReadQvar(&qvar_raw) == LIS2DUXS12_MOTION_OK)
      {
        APP_DBG_MSG("   AH_QVAR disabled (expect small, temperature-like): raw #%u = %d\n",
                    (unsigned int)i, (int)qvar_raw);
      }
      HAL_Delay(20U);
    }
  }

  for (g = 0U; g < 4U; g++)
  {
    if (LIS2DUXS12_MotionConfigureQvar(true, gains[g], LIS2DUXS12_75MOhm) ==
        LIS2DUXS12_MOTION_OK)
    {
      HAL_Delay(100U);
      if (LIS2DUXS12_MotionReadQvar(&qvar_raw) == LIS2DUXS12_MOTION_OK)
      {
        APP_DBG_MSG("   AH_QVAR gain=%u.%ux: raw = %d\n",
                    (unsigned int)(gain_x10[g] / 10U),
                    (unsigned int)(gain_x10[g] % 10U), (int)qvar_raw);
      }
    }
  }

  APP_DBG_MSG("== QVar self-test end ==\n");

  (void)LIS2DUXS12_MotionInitQvar();
  SensorManager_QvarResetBaseline();
}

static void SensorManager_OpticalResetState(void)
{
  optical_baseline_ready = false;
  optical_envelope = 0.0f;
  optical_above_threshold = false;
  optical_have_last_beat = false;
  optical_last_beat_ms = 0U;
  optical_sample_index = 0U;
  optical_beat_history_index = 0U;
  optical_beat_history_count = 0U;
  optical_ir_ac_sumsq = 0.0f;
  optical_red_ac_sumsq = 0.0f;
  optical_spo2_window_samples = 0U;
  optical_drain_calls = 0U;
}

static void SensorManager_OpticalRegisterBeatInterval(uint32_t interval_ms)
{
  uint32_t sum;
  uint8_t i;

  optical_beat_intervals_ms[optical_beat_history_index] = interval_ms;
  optical_beat_history_index =
      (uint8_t)((optical_beat_history_index + 1U) % OPTICAL_BEAT_HISTORY_LEN);
  if (optical_beat_history_count < OPTICAL_BEAT_HISTORY_LEN)
  {
    optical_beat_history_count++;
  }

  sum = 0U;
  for (i = 0U; i < optical_beat_history_count; i++)
  {
    sum += optical_beat_intervals_ms[i];
  }

  /* bpm = 60000 / average_interval_ms, expanded to avoid a float divide */
  latest_data.heart_rate_bpm =
      (uint8_t)((60000U * (uint32_t)optical_beat_history_count) / sum);
}

static void SensorManager_OpticalProcessSample(uint32_t ir_raw, uint32_t red_raw)
{
  float ir = (float)ir_raw;
  float red = (float)red_raw;
  float ir_ac;
  float red_ac;
  float pulse;
  uint32_t now_ms;

  if (!optical_baseline_ready)
  {
    optical_ir_dc = ir;
    optical_red_dc = red;
    optical_baseline_ready = true;
  }
  else
  {
    optical_ir_dc += OPTICAL_DC_ALPHA * (ir - optical_ir_dc);
    optical_red_dc += OPTICAL_DC_ALPHA * (red - optical_red_dc);
  }

  ir_ac = ir - optical_ir_dc;
  red_ac = red - optical_red_dc;

  optical_ir_ac_sumsq += ir_ac * ir_ac;
  optical_red_ac_sumsq += red_ac * red_ac;
  optical_spo2_window_samples++;

  pulse = OPTICAL_PULSE_SIGN * ir_ac;
  optical_envelope += OPTICAL_ENVELOPE_ALPHA * (fabsf(pulse) - optical_envelope);

  now_ms = optical_sample_index * OPTICAL_SAMPLE_PERIOD_MS;

  if ((!optical_above_threshold) &&
      (optical_envelope > OPTICAL_MIN_ENVELOPE) &&
      (pulse > (optical_envelope * OPTICAL_THRESHOLD_HIGH_FRAC)))
  {
    optical_above_threshold = true;
    if (optical_have_last_beat)
    {
      uint32_t interval_ms = now_ms - optical_last_beat_ms;
      if ((interval_ms >= (60000U / OPTICAL_MAX_BPM)) &&
          (interval_ms <= (60000U / OPTICAL_MIN_BPM)))
      {
        SensorManager_OpticalRegisterBeatInterval(interval_ms);
      }
    }
    optical_last_beat_ms = now_ms;
    optical_have_last_beat = true;
  }
  else if (optical_above_threshold &&
           (pulse < (optical_envelope * OPTICAL_THRESHOLD_LOW_FRAC)))
  {
    optical_above_threshold = false;
  }

  optical_sample_index++;
}

static void SensorManager_OpticalUpdateSpo2(void)
{
  float ir_ac_rms;
  float red_ac_rms;
  float ratio;
  float spo2;

  if ((optical_spo2_window_samples == 0U) ||
      (optical_ir_dc < OPTICAL_MIN_DC_FOR_VALID) ||
      (optical_red_dc < OPTICAL_MIN_DC_FOR_VALID))
  {
    /* No finger/skin contact detected: keep the last known SpO2 rather
     * than reporting a computed value from noise. */
    optical_ir_ac_sumsq = 0.0f;
    optical_red_ac_sumsq = 0.0f;
    optical_spo2_window_samples = 0U;
    return;
  }

  ir_ac_rms = sqrtf(optical_ir_ac_sumsq / (float)optical_spo2_window_samples);
  red_ac_rms = sqrtf(optical_red_ac_sumsq / (float)optical_spo2_window_samples);

  optical_ir_ac_sumsq = 0.0f;
  optical_red_ac_sumsq = 0.0f;
  optical_spo2_window_samples = 0U;

  if (ir_ac_rms < 1.0f)
  {
    return; /* avoid dividing by a flat/noise-only line */
  }

  ratio = (red_ac_rms / optical_red_dc) / (ir_ac_rms / optical_ir_dc);
  /* Generic published empirical curve; replace with a device-specific
   * calibration measured against a reference pulse oximeter. */
  spo2 = 104.0f - (17.0f * ratio);
  if (spo2 > 100.0f)
  {
    spo2 = 100.0f;
  }
  else if (spo2 < 70.0f)
  {
    spo2 = 70.0f;
  }

  latest_data.spo2_percent = (uint8_t)(spo2 + 0.5f);
}

static void SensorManager_ProcessOpticalAsync(void)
{
  uint8_t available_count;
  uint8_t i;
  uint32_t red;
  uint32_t ir;

  if (optical_status != SENSOR_OPTICAL_ACTIVE)
  {
    return;
  }

  if (MAX86150_OpticalGetAvailableSamples(&optical_device, &available_count) !=
      MAX86150_OPTICAL_OK)
  {
    return;
  }

  for (i = 0U; i < available_count; i++)
  {
    if (MAX86150_OpticalReadSample(&optical_device, &red, &ir) !=
        MAX86150_OPTICAL_OK)
    {
      break;
    }
    SensorManager_OpticalProcessSample(ir, red);
  }

  optical_drain_calls++;
  if (optical_drain_calls >= OPTICAL_SPO2_WINDOW_DRAINS)
  {
    optical_drain_calls = 0U;
    SensorManager_OpticalUpdateSpo2();
  }
}

bool SensorManager_Init(void)
{
  lis2duxs12_acceleration_t acceleration;
  latest_data.heart_rate_bpm = 72U;
  latest_data.spo2_percent = 98U;
  latest_data.temperature_centi_c = WEARABLE_TEMPERATURE_INVALID_CENTI_C;
  latest_data.supercap_mv = 0U;
  latest_data.power_state = 1U;
  latest_data.flags = 0U;
  latest_data.accel_x = 0;
  latest_data.accel_y = 0;
  latest_data.accel_z = 0;
  latest_data.qvar_raw = 0;
  qvar_baseline_ready = false;
  running = false;
  initialized = SupercapMonitor_Init();
  if (initialized)
  {
    latest_data.supercap_mv = SupercapMonitor_ReadMillivolts();
  }

  /* The PMIC is optional for BLE/ADC operation; record presence independently. */
  if (NEH7100_Init())
  {
    (void)NEH7100_EnsureConfig();
  }

  temperature_status = (TempSensor_Init() == TEMP_SENSOR_RESULT_OK) ?
                       SENSOR_TEMPERATURE_IDLE : SENSOR_TEMPERATURE_NOT_PRESENT;
  if (temperature_status == SENSOR_TEMPERATURE_IDLE)
  {
    APP_DBG_MSG("-- MAX30208: found at I2C 7-bit addr 0x%02x\n",
                (unsigned int)TempSensor_GetAddress());
  }
  else
  {
    APP_DBG_MSG("-- MAX30208: not present (scanned 0x%02x-0x%02x)\n",
                (unsigned int)MAX30208_I2C_ADDRESS_MIN,
                (unsigned int)MAX30208_I2C_ADDRESS_MAX);
    SensorManager_DebugScanI2CBus();
  }
  MAX86150_OpticalBind(&optical_device, &hi2c1);
  optical_status = (MAX86150_OpticalProbe(&optical_device) == MAX86150_OPTICAL_OK) ?
                   SENSOR_OPTICAL_IDLE : SENSOR_OPTICAL_NOT_PRESENT;
  SensorManager_OpticalResetState();
  if (optical_status == SENSOR_OPTICAL_IDLE)
  {
    APP_DBG_MSG("-- MAX86150: found (Red/IR optical)\n");
  }
  else
  {
    APP_DBG_MSG("-- MAX86150: not present\n");
  }

  motion_status = (LIS2DUXS12_MotionInit(&hi2c1) == LIS2DUXS12_MOTION_OK) ?
                  SENSOR_MOTION_ACCELEROMETER_READY : SENSOR_MOTION_NOT_PRESENT;
  motion_delay_ms = 0U;
  if (motion_status == SENSOR_MOTION_ACCELEROMETER_READY)
  {
    APP_DBG_MSG("-- LIS2DUXS12TR: WHO_AM_I OK, I2C=0x%02x\n",
                (unsigned int)(LIS2DUXS12_MotionGetHalAddress() >> 1U));
    if (LIS2DUXS12_MotionReadAcceleration(&acceleration) ==
        LIS2DUXS12_MOTION_OK)
    {
      APP_DBG_MSG("-- LIS2DUXS12TR XYZ [mg]: %ld, %ld, %ld\n",
                  (long)acceleration.mg[0],
                  (long)acceleration.mg[1],
                  (long)acceleration.mg[2]);
    }
    
    if (LIS2DUXS12_MotionInitQvar() == LIS2DUXS12_MOTION_OK)
    {
      SensorManager_QvarResetBaseline();
      APP_DBG_MSG("-- LIS2DUXS12TR QVar initialized on INT1\n");
      SensorManager_QvarSelfTest();
    }
    else
    {
      APP_DBG_MSG("-- LIS2DUXS12TR QVar init FAILED\n");
    }
  }
  else
  {
    APP_DBG_MSG("-- LIS2DUXS12TR: not present (optional)\n");
  }
  temperature_conversion_elapsed_ms = 0U;
  temperature_async_delay_ms = 0U;
  temperature_reprobe_calls = 0U;
  motion_reprobe_calls = 0U;
  optical_reprobe_calls = 0U;
  return initialized;
}

bool SensorManager_Start(void)
{
  if (!initialized)
  {
    return false;
  }
  running = true;
  SensorManager_StartTemperatureConversion();

  if (optical_status != SENSOR_OPTICAL_NOT_PRESENT)
  {
    SensorManager_OpticalResetState();
    if (MAX86150_OpticalConfigureRedIr(&optical_device,
                                       OPTICAL_DEFAULT_LED_CURRENT_CODE,
                                       OPTICAL_DEFAULT_LED_CURRENT_CODE) ==
        MAX86150_OPTICAL_OK)
    {
      optical_status = SENSOR_OPTICAL_ACTIVE;
    }
    else
    {
      optical_status = SENSOR_OPTICAL_IDLE;
    }
  }
  return true;
}

bool SensorManager_Stop(void)
{
  running = false;
  temperature_async_delay_ms = 0U;
  if (temperature_status != SENSOR_TEMPERATURE_NOT_PRESENT)
  {
    (void)TempSensor_Sleep();
    temperature_status = SENSOR_TEMPERATURE_IDLE;
  }

  if (optical_status == SENSOR_OPTICAL_ACTIVE)
  {
    (void)MAX86150_OpticalShutdown(&optical_device, true);
    optical_status = SENSOR_OPTICAL_IDLE;
  }
  return initialized;
}

bool SensorManager_GetLatestData(wearable_sensor_data_t *data)
{
  if ((!initialized) || (data == NULL))
  {
    return false;
  }
  *data = latest_data;
  return true;
}

void SensorManager_Process(void)
{
  if (!running)
  {
    return;
  }

  latest_data.supercap_mv = SupercapMonitor_ReadMillivolts();

  if (temperature_status == SENSOR_TEMPERATURE_NOT_PRESENT)
  {
    temperature_reprobe_calls++;
    if (temperature_reprobe_calls >= TEMPERATURE_REPROBE_INTERVAL_CALLS)
    {
      temperature_reprobe_calls = 0U;
      temperature_status = (TempSensor_Init() == TEMP_SENSOR_RESULT_OK) ?
                           SENSOR_TEMPERATURE_IDLE : SENSOR_TEMPERATURE_NOT_PRESENT;
      if (temperature_status == SENSOR_TEMPERATURE_IDLE)
      {
        APP_DBG_MSG("-- MAX30208: recovered at I2C 7-bit addr 0x%02x\n",
                    (unsigned int)TempSensor_GetAddress());
      }
    }
  }

  SensorManager_StartTemperatureConversion();

  if ((motion_status == SENSOR_MOTION_NOT_PRESENT) ||
      (motion_status == SENSOR_MOTION_BUS_ERROR))
  {
    motion_reprobe_calls++;
    if (motion_reprobe_calls >= MOTION_REPROBE_INTERVAL_CALLS)
    {
      motion_reprobe_calls = 0U;
      motion_status = (LIS2DUXS12_MotionInit(&hi2c1) == LIS2DUXS12_MOTION_OK) ?
                      SENSOR_MOTION_ACCELEROMETER_READY : SENSOR_MOTION_NOT_PRESENT;
      if (motion_status == SENSOR_MOTION_ACCELEROMETER_READY)
      {
        APP_DBG_MSG("-- LIS2DUXS12TR: recovered, I2C=0x%02x\n",
                    (unsigned int)(LIS2DUXS12_MotionGetHalAddress() >> 1U));
        if (LIS2DUXS12_MotionInitQvar() == LIS2DUXS12_MOTION_OK)
        {
          SensorManager_QvarResetBaseline();
        }
        else
        {
          APP_DBG_MSG("-- LIS2DUXS12TR: QVar init FAILED after recovery\n");
        }
      }
    }
  }

  if (motion_status == SENSOR_MOTION_ACCELEROMETER_READY || motion_status == SENSOR_MOTION_CLASSIFIER_READY)
  {
    lis2duxs12_acceleration_t acceleration;
    int16_t qvar_raw = 0;
    
    if (LIS2DUXS12_MotionReadAcceleration(&acceleration) == LIS2DUXS12_MOTION_OK)
    {
      latest_data.accel_x = (int16_t)acceleration.mg[0];
      latest_data.accel_y = (int16_t)acceleration.mg[1];
      latest_data.accel_z = (int16_t)acceleration.mg[2];
    }
    
    if (LIS2DUXS12_MotionReadQvar(&qvar_raw) == LIS2DUXS12_MOTION_OK)
    {
       float qvar_deviation;

       APP_DBG_MSG("-- QVar raw = %d\n", (int)qvar_raw);
       latest_data.qvar_raw = qvar_raw;

       /* Wear/no-wear from deviation off a slow-moving baseline, not an
        * absolute value - see QVAR_BASELINE_ALPHA/QVAR_WEAR_DEVIATION_THRESHOLD
        * comment above for why (raw sits on a large hardware-specific DC
        * offset, not centered near 0). */
       if (!qvar_baseline_ready)
       {
          qvar_baseline = (float)qvar_raw;
          qvar_baseline_ready = true;
       }
       else
       {
          qvar_baseline += QVAR_BASELINE_ALPHA * ((float)qvar_raw - qvar_baseline);
       }

       qvar_deviation = (float)qvar_raw - qvar_baseline;
       APP_DBG_MSG("-- QVar baseline = %d, deviation = %d\n",
                   (int)qvar_baseline, (int)qvar_deviation);
       if (fabsf(qvar_deviation) > QVAR_WEAR_DEVIATION_THRESHOLD)
       {
          latest_data.flags |= 0x40; /* Wear detected */
       }
       else
       {
          latest_data.flags &= ~0x40; /* No wear detected */
       }
    }
    else
    {
       APP_DBG_MSG("-- QVar read FAILED\n");
    }
  }

  if (optical_status != SENSOR_OPTICAL_ACTIVE)
  {
    optical_reprobe_calls++;
    if (optical_reprobe_calls >= OPTICAL_REPROBE_INTERVAL_CALLS)
    {
      optical_reprobe_calls = 0U;
      if (optical_status == SENSOR_OPTICAL_NOT_PRESENT)
      {
        if (MAX86150_OpticalProbe(&optical_device) == MAX86150_OPTICAL_OK)
        {
          optical_status = SENSOR_OPTICAL_IDLE;
          APP_DBG_MSG("-- MAX86150: recovered (Red/IR optical)\n");
        }
      }
      if (optical_status == SENSOR_OPTICAL_IDLE)
      {
        SensorManager_OpticalResetState();
        if (MAX86150_OpticalConfigureRedIr(&optical_device,
                                           OPTICAL_DEFAULT_LED_CURRENT_CODE,
                                           OPTICAL_DEFAULT_LED_CURRENT_CODE) ==
            MAX86150_OPTICAL_OK)
        {
          optical_status = SENSOR_OPTICAL_ACTIVE;
          APP_DBG_MSG("-- MAX86150: PPG active\n");
        }
      }
    }
  }

  if (optical_status != SENSOR_OPTICAL_ACTIVE)
  {
    /* No PPG sensor available: keep a gently varying mock HR so the BLE
     * notification path stays exercisable without hardware attached.
     * SpO2 has no such mock and stays at its last/init value. */
    latest_data.heart_rate_bpm++;
    if (latest_data.heart_rate_bpm > 82U)
    {
      latest_data.heart_rate_bpm = 68U;
    }
  }

  /* Log to NFC EEPROM if BLE is disconnected */
  APP_BLE_ConnStatus_t ble_status = APP_BLE_Get_Server_Connection_Status();
  static uint16_t log_timer_s = 0;

  if (ble_status != APP_BLE_CONNECTED_SERVER)
  {
    log_timer_s++;
    /* nfc_config is accessible here because we included nfc_config.c above */
    if (log_timer_s >= nfc_config.hr_interval_s)
    {
      NFC_SensorRecord_t record;
      memset(&record, 0, sizeof(record));
      record.timestamp = DeviceTime_GetUnixSeconds();
      memcpy(&record.sensor_data, &latest_data, sizeof(wearable_sensor_data_t));
      record.sequence = 0; // Handled internally by NFC_Log_Add
      NFC_Log_Add(&record);
      
      log_timer_s = 0;
    }
  }
  else
  {
    // Ready to log immediately when connection is lost
    log_timer_s = nfc_config.hr_interval_s; 
  }
}

void SensorManager_ProcessAsync(void)
{
  temp_sensor_result_t result;
  int16_t temperature_centi_c;

  if (!running)
  {
    temperature_async_delay_ms = 0U;
    return;
  }

  if (temperature_status == SENSOR_TEMPERATURE_CONVERTING)
  {
    temperature_conversion_elapsed_ms += temperature_async_delay_ms;
    result = TempSensor_TryReadCentiC(&temperature_centi_c);
    if (result == TEMP_SENSOR_RESULT_OK)
    {
      latest_data.temperature_centi_c = temperature_centi_c;
      temperature_async_delay_ms = 0U;
      temperature_status = SENSOR_TEMPERATURE_VALID;
    }
    else if ((result == TEMP_SENSOR_RESULT_NOT_READY) &&
             (temperature_conversion_elapsed_ms < TEMPERATURE_CONVERSION_TIMEOUT_MS))
    {
      temperature_async_delay_ms = TEMPERATURE_RETRY_DELAY_MS;
    }
    else
    {
      temperature_async_delay_ms = 0U;
      temperature_status = (result == TEMP_SENSOR_RESULT_NOT_READY) ?
                           SENSOR_TEMPERATURE_TIMEOUT : SENSOR_TEMPERATURE_BUS_ERROR;
    }
  }
  else
  {
    temperature_async_delay_ms = 0U;
  }

  SensorManager_ProcessOpticalAsync();
}

bool SensorManager_GetAsyncDelayMs(uint32_t *delay_ms)
{
  bool temp_due = (temperature_status == SENSOR_TEMPERATURE_CONVERTING) &&
                  (temperature_async_delay_ms != 0U);
  bool optical_due = (optical_status == SENSOR_OPTICAL_ACTIVE);

  if ((delay_ms == NULL) || (!temp_due && !optical_due))
  {
    return false;
  }

  if (temp_due && ((!optical_due) ||
                    (temperature_async_delay_ms < OPTICAL_DRAIN_INTERVAL_MS)))
  {
    *delay_ms = temperature_async_delay_ms;
  }
  else
  {
    *delay_ms = OPTICAL_DRAIN_INTERVAL_MS;
  }
  return true;
}

sensor_temperature_status_t SensorManager_GetTemperatureStatus(void)
{
  return temperature_status;
}

sensor_motion_status_t SensorManager_GetMotionStatus(void)
{
  return motion_status;
}

sensor_optical_status_t SensorManager_GetOpticalStatus(void)
{
  return optical_status;
}

void SensorManager_ProcessMotionInterrupt(void)
{
  lis2duxs12_motion_result_t result;
  lis2duxs12_motion_event_t event;

  if ((motion_status == SENSOR_MOTION_NOT_PRESENT) ||
      (motion_status == SENSOR_MOTION_BUS_ERROR))
  {
    return;
  }

  result = LIS2DUXS12_MotionProcessInterrupt();
  if (result == LIS2DUXS12_MOTION_OK)
  {
    if (!LIS2DUXS12_MotionGetLatestEvent(&event))
    {
      return;
    }
    if (event.mlc_status != 0U)
    {
      motion_status = SENSOR_MOTION_CLASSIFIER_READY;
    }
    if (event.activity == LIS2DUXS12_ACTIVITY_FALL)
    {
      SensorManager_SetFlag(WEARABLE_FLAG_FALL_CANDIDATE, true);
      APP_DBG_MSG("-- LIS2DUXS12TR: MLC fall candidate\n");
    }
  }
  else if (result == LIS2DUXS12_MOTION_BUS_ERROR)
  {
    motion_status = SENSOR_MOTION_BUS_ERROR;
  }
}

void SensorManager_ProcessMotionTimeout(void)
{
  /* MLC classification is delivered directly through the sensor interrupt. */
  motion_delay_ms = 0U;
}

bool SensorManager_GetMotionDelayMs(uint32_t *delay_ms)
{
  if ((delay_ms == NULL) || (motion_delay_ms == 0U))
  {
    return false;
  }
  *delay_ms = motion_delay_ms;
  return true;
}

void SensorManager_SetPowerState(uint8_t power_state)
{
  latest_data.power_state = power_state;
}

void SensorManager_SetFlag(uint8_t flag, bool enabled)
{
  if (enabled)
  {
    latest_data.flags |= flag;
  }
  else
  {
    latest_data.flags &= (uint8_t)~flag;
  }
}
