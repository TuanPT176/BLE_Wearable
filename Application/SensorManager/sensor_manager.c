#include "sensor_manager.h"

#include <stddef.h>
#include <math.h>

#include "main.h"
#include "../../Drivers/max30208.h"
#include "../../Drivers/supercap_monitor.h"
#include "../../Drivers/Sensors/LIS2DUXS12TR/lis2duxs12_motion.h"
#include "../../Drivers/Sensors/MAX86150/max86150_optical.h"
#include "../NFC/nfc_log.h"
#include "../../STM32_BLE/App/app_ble.h"
#include "../DeviceTime/device_time.h"
#include "ecg_diag.h"
#include "../wearable_config.h"
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

/* Timings, re-probe intervals, PPG and QVar thresholds: wearable_config.h. */

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
#define OPTICAL_SAMPLE_PERIOD_MS          10U   /* fixed by the 100 Hz PPG ODR */

/*
 * QVar (LIS2DUXS12TR AH_QVAR channel) wear detection.
 * ---------------------------------------------------
 * The chip has no threshold interrupt on the QVar channel: only its MLC/FSM
 * can classify QVar on-chip, and no MLC program is loaded yet. Until then
 * the classification runs here. While measuring, the chip pulses RES (PB2)
 * once per output sample (100 Hz ODR, see lis2duxs12_motion.c) and every
 * sample feeds a window; WEARABLE_FLAG_WEAR_DETECTED follows the window's
 * peak-to-peak activity with hysteresis and debounce, so it holds the state
 * instead of only marking touch/release edges. A flag change is returned by
 * SensorManager_ProcessMotionInterrupt() and notified immediately.
 *
 * Skin contact couples body-borne interference and motion artefacts into
 * the electrode, an open electrode stays quiet - that is the assumption
 * behind using activity rather than the DC level, whose sign and offset are
 * board-specific. The thresholds are a first guess (roughly 16 mV / 8 mV at
 * the 0.5x gain, ~37 LSB/mV), not calibrated: capture g_qvarDiag worn and
 * not worn and retune.
 *
 * The stream is off during an ECG session (no extra I2C traffic next to the
 * ECG drain task); the wear flag is frozen and only the 1 Hz raw value keeps
 * updating.
 */
static wearable_sensor_data_t latest_data;
static bool initialized;
static bool running;
static sensor_temperature_status_t temperature_status;
static sensor_temperature_status_t temperature_result;
static sensor_motion_status_t motion_status;
static sensor_optical_status_t optical_status;
static uint32_t temperature_conversion_elapsed_ms;
static uint32_t temperature_async_delay_ms;
static uint32_t temperature_reprobe_calls;
static uint32_t motion_reprobe_calls;
static uint32_t optical_reprobe_calls;
static uint32_t motion_delay_ms;
static uint32_t motion_last_event_sequence;
static bool qvar_stream_active;
static uint32_t qvar_stream_samples;
static uint32_t qvar_samples_at_last_poll;
static int16_t qvar_window_min;
static int16_t qvar_window_max;
static int32_t qvar_window_sum;
static uint16_t qvar_window_count;
static uint8_t qvar_debounce_windows;
volatile sensor_qvar_diag_t g_qvarDiag;

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
/* ECG FIFO rollovers (samples lost because the drain task ran late). */
static uint32_t ecg_overflow_events;

#if ECG_DIAG_DUMP_REGS
/* MAX86150 registers read back after the ECG configuration (datasheet
 * register map): FIFO config, FIFO data control 1/2, system control, PPG
 * config 1/2, prox threshold, LED1/LED2 PA, LED range, pilot PA, ECG config
 * 1/3. */
static const uint8_t ecg_diag_reg_addr[ECG_DIAG_REG_COUNT] = {
  0x08U, 0x09U, 0x0AU, 0x0DU, 0x0EU, 0x0FU, 0x10U,
  0x11U, 0x12U, 0x14U, 0x15U, 0x3CU, 0x3EU
};

static void SensorManager_EcgDumpRegisters(void)
{
  uint8_t i;
  uint8_t value;

  for (i = 0U; i < ECG_DIAG_REG_COUNT; i++)
  {
    if (MAX86150_ReadRegister(&optical_device, ecg_diag_reg_addr[i], &value) ==
        MAX86150_OPTICAL_OK)
    {
      g_ecgDiag.regs[i][0] = ecg_diag_reg_addr[i];
      g_ecgDiag.regs[i][1] = value;
    }
    else
    {
      g_ecgDiag.regs[i][0] = 0xEEU;
      g_ecgDiag.regs[i][1] = 0xEEU;
    }
  }
  g_ecgDiag.regs_valid = 1U;
}
#endif

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
    temperature_result = SENSOR_TEMPERATURE_BUS_ERROR;
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

static void SensorManager_QvarResetWindow(void)
{
  qvar_window_count = 0U;
  qvar_window_sum = 0;
  qvar_debounce_windows = 0U;
}

/* Any LIS2DUXS12 I2C failure ends up here: Process() re-probes the chip on
 * its MOTION_REPROBE_INTERVAL_CALLS cycle. The wear flag keeps its last
 * state meanwhile, so a bus glitch is not reported as a wear change. */
static void SensorManager_MotionBusError(void)
{
  motion_status = SENSOR_MOTION_BUS_ERROR;
  motion_reprobe_calls = 0U;
  qvar_stream_active = false;
  g_qvarDiag.stream_active = 0U;
  g_qvarDiag.bus_errors++;
}

static void SensorManager_QvarStreamStart(void)
{
  SensorManager_QvarResetWindow();
  if (LIS2DUXS12_MotionSetQvarDataReadyInterrupt(true) == LIS2DUXS12_MOTION_OK)
  {
    qvar_stream_active = true;
    g_qvarDiag.stream_active = 1U;
  }
  else
  {
    SensorManager_MotionBusError();
  }
}

static void SensorManager_QvarStreamStop(void)
{
  if (qvar_stream_active)
  {
    (void)LIS2DUXS12_MotionSetQvarDataReadyInterrupt(false);
  }
  qvar_stream_active = false;
  g_qvarDiag.stream_active = 0U;
}

/* Feeds one stream sample to the wear detector. Returns true when
 * WEARABLE_FLAG_WEAR_DETECTED changed. */
static bool SensorManager_QvarProcessSample(int16_t qvar_raw)
{
  bool worn = (latest_data.flags & WEARABLE_FLAG_WEAR_DETECTED) != 0U;
  uint32_t peak_to_peak;

  latest_data.qvar_raw = qvar_raw;
  if ((qvar_window_count == 0U) || (qvar_raw < qvar_window_min))
  {
    qvar_window_min = qvar_raw;
  }
  if ((qvar_window_count == 0U) || (qvar_raw > qvar_window_max))
  {
    qvar_window_max = qvar_raw;
  }
  qvar_window_sum += qvar_raw;
  qvar_window_count++;
  if (qvar_window_count < QVAR_WINDOW_SAMPLES)
  {
    return false;
  }

  peak_to_peak = (uint32_t)((int32_t)qvar_window_max - (int32_t)qvar_window_min);
  g_qvarDiag.windows++;
  g_qvarDiag.last_peak_to_peak = (uint16_t)peak_to_peak;
  g_qvarDiag.last_mean = (int16_t)(qvar_window_sum / (int32_t)qvar_window_count);
  g_qvarDiag.last_min = qvar_window_min;
  g_qvarDiag.last_max = qvar_window_max;
  qvar_window_count = 0U;
  qvar_window_sum = 0;

  if (worn ? (peak_to_peak <= QVAR_WORN_EXIT_P2P) :
             (peak_to_peak >= QVAR_WORN_ENTER_P2P))
  {
    qvar_debounce_windows++;
  }
  else
  {
    qvar_debounce_windows = 0U;
  }
  if (qvar_debounce_windows < QVAR_DEBOUNCE_WINDOWS)
  {
    return false;
  }

  qvar_debounce_windows = 0U;
  SensorManager_SetFlag(WEARABLE_FLAG_WEAR_DETECTED, !worn);
  g_qvarDiag.worn = worn ? 0U : 1U;
  g_qvarDiag.state_changes++;
  return true;
}

/* Probe + configure the LIS2DUXS12 (boot and re-probe). MotionInit() resets
 * the chip, so QVar and the RES data-ready stream are set up again. */
static void SensorManager_MotionBringUp(void)
{
  qvar_stream_active = false;
  g_qvarDiag.stream_active = 0U;
  motion_last_event_sequence = 0U;
  if (LIS2DUXS12_MotionInit(&hi2c1) != LIS2DUXS12_MOTION_OK)
  {
    motion_status = SENSOR_MOTION_NOT_PRESENT;
    return;
  }
  motion_status = SENSOR_MOTION_ACCELEROMETER_READY;
  if (LIS2DUXS12_MotionInitQvar() != LIS2DUXS12_MOTION_OK)
  {
    APP_DBG_MSG("-- LIS2DUXS12TR QVar init FAILED\n");
    SensorManager_MotionBusError();
    return;
  }
  if (running && (optical_status != SENSOR_OPTICAL_ECG_ACTIVE))
  {
    SensorManager_QvarStreamStart();
  }
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
  SensorManager_QvarResetWindow();
  qvar_stream_samples = 0U;
  qvar_samples_at_last_poll = 0U;
  running = false;
  initialized = SupercapMonitor_Init();
  if (initialized)
  {
    latest_data.supercap_mv = SupercapMonitor_ReadMillivolts();
  }

  temperature_status = (TempSensor_Init() == TEMP_SENSOR_RESULT_OK) ?
                       SENSOR_TEMPERATURE_IDLE : SENSOR_TEMPERATURE_NOT_PRESENT;
  temperature_result = temperature_status;
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

  SensorManager_MotionBringUp();
  motion_delay_ms = 0U;
  if (motion_status == SENSOR_MOTION_ACCELEROMETER_READY)
  {
    APP_DBG_MSG("-- LIS2DUXS12TR: WHO_AM_I OK, I2C=0x%02x, QVar on INT1, IRQ on RES\n",
                (unsigned int)(LIS2DUXS12_MotionGetHalAddress() >> 1U));
    if (LIS2DUXS12_MotionReadAcceleration(&acceleration) ==
        LIS2DUXS12_MOTION_OK)
    {
      APP_DBG_MSG("-- LIS2DUXS12TR XYZ [mg]: %ld, %ld, %ld\n",
                  (long)acceleration.mg[0],
                  (long)acceleration.mg[1],
                  (long)acceleration.mg[2]);
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

  if ((motion_status == SENSOR_MOTION_ACCELEROMETER_READY) ||
      (motion_status == SENSOR_MOTION_CLASSIFIER_READY))
  {
    SensorManager_QvarStreamStart();
  }

  /* PPG and ECG share the MAX86150: (re)starting PPG ends any ECG session. */
  SensorManager_SetFlag(WEARABLE_FLAG_ECG_ACTIVE, false);
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
    temperature_result = SENSOR_TEMPERATURE_IDLE;
  }
  SensorManager_QvarStreamStop();

  if ((optical_status == SENSOR_OPTICAL_ACTIVE) ||
      (optical_status == SENSOR_OPTICAL_ECG_ACTIVE))
  {
    (void)MAX86150_OpticalShutdown(&optical_device, true);
    optical_status = SENSOR_OPTICAL_IDLE;
  }
  SensorManager_SetFlag(WEARABLE_FLAG_ECG_ACTIVE, false);
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
      temperature_result = temperature_status;
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
      SensorManager_MotionBringUp();
      if (motion_status == SENSOR_MOTION_ACCELEROMETER_READY)
      {
        APP_DBG_MSG("-- LIS2DUXS12TR: recovered, I2C=0x%02x\n",
                    (unsigned int)(LIS2DUXS12_MotionGetHalAddress() >> 1U));
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
    else
    {
      APP_DBG_MSG("-- LIS2DUXS12TR: acceleration read FAILED\n");
      SensorManager_MotionBusError();
    }

    /* The RES stream normally keeps qvar_raw current. Poll here when it is
     * off (ECG session) or delivered nothing since the last call, so the
     * raw value still updates if the PB2 interrupt never arrives; the wear
     * flag is only driven by the stream. */
    if ((motion_status != SENSOR_MOTION_BUS_ERROR) &&
        ((!qvar_stream_active) ||
         (qvar_stream_samples == qvar_samples_at_last_poll)))
    {
      if (LIS2DUXS12_MotionReadQvar(&qvar_raw) == LIS2DUXS12_MOTION_OK)
      {
        latest_data.qvar_raw = qvar_raw;
      }
      else
      {
        APP_DBG_MSG("-- QVar read FAILED\n");
        SensorManager_MotionBusError();
      }
    }
    qvar_samples_at_last_poll = qvar_stream_samples;
  }

  if ((optical_status == SENSOR_OPTICAL_NOT_PRESENT) ||
      (optical_status == SENSOR_OPTICAL_IDLE))
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

  if ((optical_status == SENSOR_OPTICAL_NOT_PRESENT) ||
      (optical_status == SENSOR_OPTICAL_IDLE))
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
      temperature_result = SENSOR_TEMPERATURE_VALID;
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
      temperature_result = temperature_status;
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

sensor_temperature_status_t SensorManager_GetTemperatureResult(void)
{
  return temperature_result;
}

sensor_motion_status_t SensorManager_GetMotionStatus(void)
{
  return motion_status;
}

sensor_optical_status_t SensorManager_GetOpticalStatus(void)
{
  return optical_status;
}

bool SensorManager_StartEcg(void)
{
  if ((!initialized) || (!running) ||
      (optical_status == SENSOR_OPTICAL_NOT_PRESENT))
  {
    return false;
  }

  if (MAX86150_EcgConfigure(&optical_device) != MAX86150_OPTICAL_OK)
  {
    /* The chip was reset; Process() restores PPG on its reprobe cycle. */
    optical_status = SENSOR_OPTICAL_IDLE;
    return false;
  }

  /* No 100 Hz QVar reads on the shared I2C bus during an ECG session. */
  SensorManager_QvarStreamStop();

#if ECG_DIAG_PPG_OFF
  if (MAX86150_LedsOff(&optical_device) != MAX86150_OPTICAL_OK)
  {
    APP_DBG_MSG("-- MAX86150 diag: LEDs-off write FAILED\n");
  }
  else
  {
    APP_DBG_MSG("-- MAX86150 diag: LED1/LED2/pilot PA = 0\n");
  }
#endif
#if ECG_DIAG_DUMP_REGS
  SensorManager_EcgDumpRegisters();
#endif

  optical_status = SENSOR_OPTICAL_ECG_ACTIVE;
  g_ecgDiag.ecg_sessions++;
  SensorManager_SetFlag(WEARABLE_FLAG_ECG_ACTIVE, true);
  APP_DBG_MSG("-- MAX86150: ECG active (200 sps)\n");
  return true;
}

bool SensorManager_IsEcgActive(void)
{
  return optical_status == SENSOR_OPTICAL_ECG_ACTIVE;
}

uint8_t SensorManager_ReadEcgSamples(int16_t *samples, uint8_t max_samples)
{
  int32_t raw[MAX86150_ECG_FIFO_DEPTH];
  uint8_t count = 0U;
  uint8_t i;
  bool overflowed = false;

  if ((samples == NULL) || (optical_status != SENSOR_OPTICAL_ECG_ACTIVE))
  {
    return 0U;
  }
  if (max_samples > MAX86150_ECG_FIFO_DEPTH)
  {
    max_samples = (uint8_t)MAX86150_ECG_FIFO_DEPTH;
  }

  if (MAX86150_EcgReadSamples(&optical_device, raw, max_samples,
                              &count, &overflowed) != MAX86150_OPTICAL_OK)
  {
    return 0U;
  }
  if (overflowed)
  {
    ecg_overflow_events++;
    g_ecgDiag.ecg_overflows++;
  }

  /* 18-bit two's complement -> int16 for the ECG_DATA wire format; the
   * arithmetic shift keeps the sign (GCC on Arm). */
  for (i = 0U; i < count; i++)
  {
    samples[i] = (int16_t)(raw[i] >> 2);
  }
  return count;
}

bool SensorManager_ProcessMotionInterrupt(void)
{
  lis2duxs12_motion_result_t result;
  lis2duxs12_motion_event_t event;
  int16_t qvar_raw;
  bool changed = false;

  if ((motion_status == SENSOR_MOTION_NOT_PRESENT) ||
      (motion_status == SENSOR_MOTION_BUS_ERROR))
  {
    return false;
  }

  /* RES carries both sources: the QVar data-ready pulses (one per sample
   * while the stream is on) and, once an MLC program is armed, its events. */
  if (qvar_stream_active)
  {
    g_qvarDiag.interrupts++;
    if (LIS2DUXS12_MotionReadQvar(&qvar_raw) != LIS2DUXS12_MOTION_OK)
    {
      SensorManager_MotionBusError();
      return false;
    }
    qvar_stream_samples++;
    g_qvarDiag.samples++;
    changed = SensorManager_QvarProcessSample(qvar_raw);
  }

  if (!LIS2DUXS12_MotionIsMlcInterruptArmed())
  {
    return changed;
  }

  result = LIS2DUXS12_MotionProcessInterrupt();
  if (result == LIS2DUXS12_MOTION_OK)
  {
    if ((!LIS2DUXS12_MotionGetLatestEvent(&event)) ||
        (event.sequence == motion_last_event_sequence))
    {
      return changed;
    }
    motion_last_event_sequence = event.sequence;
    motion_status = SENSOR_MOTION_CLASSIFIER_READY;
    if ((event.activity == LIS2DUXS12_ACTIVITY_FALL) &&
        ((latest_data.flags & WEARABLE_FLAG_FALL_CANDIDATE) == 0U))
    {
      SensorManager_SetFlag(WEARABLE_FLAG_FALL_CANDIDATE, true);
      changed = true;
      APP_DBG_MSG("-- LIS2DUXS12TR: MLC fall candidate\n");
    }
  }
  else if (result == LIS2DUXS12_MOTION_BUS_ERROR)
  {
    SensorManager_MotionBusError();
  }
  return changed;
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
