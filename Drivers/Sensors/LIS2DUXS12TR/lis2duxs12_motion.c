#include "lis2duxs12_motion.h"

#include <string.h>

static stmdev_ctx_t motion_context;
static lis2duxs12_platform_t motion_platform;
static lis2duxs12_priv_t motion_priv;
static lis2duxs12_md_t motion_mode;
static lis2duxs12_motion_event_t latest_event;
static lis2duxs12_class_rule_t class_rules[LIS2DUXS12_MOTION_MAX_CLASS_RULES];
static size_t class_rule_count;
static bool motion_present;
static bool mlc_loaded;
static bool event_valid;

#define LIS2DUXS12_RESET_POLL_MAX_TRIES 50U

static int32_t LIS2DUXS12_MgRound(float_t value)
{
  return (value >= 0.0f) ? (int32_t)(value + 0.5f) :
                           (int32_t)(value - 0.5f);
}

static uint8_t LIS2DUXS12_MlcStatusToMask(
    const lis2duxs12_mlc_status_mainpage_t *status)
{
  return (uint8_t)((status->is_mlc1 << 0U) |
                   (status->is_mlc2 << 1U) |
                   (status->is_mlc3 << 2U) |
                   (status->is_mlc4 << 3U));
}

static lis2duxs12_activity_t LIS2DUXS12_DecodeActivity(
    const lis2duxs12_motion_event_t *event)
{
  size_t index;
  for (index = 0U; index < class_rule_count; index++)
  {
    const lis2duxs12_class_rule_t *rule = &class_rules[index];
    if ((rule->output_index < LIS2DUXS12_MLC_OUTPUT_COUNT) &&
        ((event->mlc_status & (1U << rule->output_index)) != 0U) &&
        (event->mlc_output[rule->output_index] == rule->output_value))
    {
      return rule->activity;
    }
  }
  return LIS2DUXS12_ACTIVITY_UNKNOWN;
}

lis2duxs12_motion_result_t LIS2DUXS12_MotionInit(I2C_HandleTypeDef *i2c)
{
  bool address_high;
  lis2duxs12_pin_conf_t pin_configuration = {0};
  lis2duxs12_status_t reset_status = {0};
  uint8_t poll_tries;
  memset(&latest_event, 0, sizeof(latest_event));
  memset(&motion_priv, 0, sizeof(motion_priv));
  motion_present = false;
  mlc_loaded = false;
  event_valid = false;
  class_rule_count = 0U;
  if (i2c == NULL)
  {
    return LIS2DUXS12_MOTION_INVALID_ARGUMENT;
  }
  for (address_high = false; ; address_high = true)
  {
    LIS2DUXS12_PlatformInit(&motion_context, &motion_platform, &motion_priv,
                            i2c, address_high);
    if (LIS2DUXS12_PlatformProbe(&motion_context) == 0)
    {
      motion_present = true;
      break;
    }
    if (address_high)
    {
      break;
    }
  }
  if (!motion_present)
  {
    return LIS2DUXS12_MOTION_NOT_PRESENT;
  }
  motion_mode.odr = LIS2DUXS12_100Hz_LP;
  motion_mode.fs = LIS2DUXS12_4g;
  motion_mode.bw = LIS2DUXS12_ODR_div_4;
  pin_configuration.int1_int2_push_pull = PROPERTY_ENABLE;

  if (lis2duxs12_sw_reset(&motion_context) != 0)
  {
    motion_present = false;
    return LIS2DUXS12_MOTION_BUS_ERROR;
  }
  /* Wait for the software reset to clear (bounded so a bus glitch can't
   * hang init forever); a fresh reset normally clears in well under 1ms. */
  for (poll_tries = 0U; poll_tries < LIS2DUXS12_RESET_POLL_MAX_TRIES; poll_tries++)
  {
    if (lis2duxs12_status_get(&motion_context, &reset_status) != 0)
    {
      motion_present = false;
      return LIS2DUXS12_MOTION_BUS_ERROR;
    }
    if (reset_status.sw_reset == 0U)
    {
      break;
    }
    motion_context.mdelay(1U);
  }

  if ((lis2duxs12_init_set(&motion_context) != 0) ||
      (lis2duxs12_embedded_state_set(&motion_context, PROPERTY_ENABLE) != 0) ||
      (lis2duxs12_mode_set(&motion_context, &motion_mode) != 0) ||
      (lis2duxs12_pin_conf_set(&motion_context, &pin_configuration) != 0) ||
      (lis2duxs12_int_pin_polarity_set(&motion_context,
                                       LIS2DUXS12_ACTIVE_HIGH) != 0))
  {
    motion_present = false;
    return LIS2DUXS12_MOTION_BUS_ERROR;
  }
  return LIS2DUXS12_MOTION_OK;
}

uint16_t LIS2DUXS12_MotionGetHalAddress(void)
{
  return motion_platform.hal_address;
}

lis2duxs12_motion_result_t LIS2DUXS12_MotionReadAcceleration(
    lis2duxs12_acceleration_t *sample)
{
  lis2duxs12_xl_data_t data = {0};
  uint8_t axis;
  if (sample == NULL)
  {
    return LIS2DUXS12_MOTION_INVALID_ARGUMENT;
  }
  if (!motion_present)
  {
    return LIS2DUXS12_MOTION_NOT_PRESENT;
  }
  if (lis2duxs12_xl_data_get(&motion_context, &motion_mode, &data) != 0)
  {
    return LIS2DUXS12_MOTION_BUS_ERROR;
  }
  for (axis = 0U; axis < 3U; axis++)
  {
    sample->raw[axis] = data.raw[axis];
    sample->mg[axis] = LIS2DUXS12_MgRound(data.mg[axis]);
  }
  return LIS2DUXS12_MOTION_OK;
}

lis2duxs12_motion_result_t LIS2DUXS12_MotionLoadUcf(
    const ucf_line_t *configuration, size_t line_count)
{
  size_t line;
  if ((configuration == NULL) || (line_count == 0U))
  {
    return LIS2DUXS12_MOTION_INVALID_ARGUMENT;
  }
  if (!motion_present)
  {
    return LIS2DUXS12_MOTION_NOT_PRESENT;
  }
  mlc_loaded = false;
  for (line = 0U; line < line_count; line++)
  {
    uint8_t value = configuration[line].data;
    if (lis2duxs12_write_reg(&motion_context, configuration[line].address,
                             &value, 1U) != 0)
    {
      (void)lis2duxs12_mem_bank_set(&motion_context, LIS2DUXS12_MAIN_MEM_BANK);
      return LIS2DUXS12_MOTION_BUS_ERROR;
    }
  }
  if (lis2duxs12_mem_bank_set(&motion_context, LIS2DUXS12_MAIN_MEM_BANK) != 0)
  {
    return LIS2DUXS12_MOTION_BUS_ERROR;
  }
  mlc_loaded = true;
  return LIS2DUXS12_MOTION_OK;
}

lis2duxs12_motion_result_t LIS2DUXS12_MotionArmMlcInterrupt(void)
{
  lis2duxs12_pin_int_route_t route = {0};
  if (!motion_present)
  {
    return LIS2DUXS12_MOTION_NOT_PRESENT;
  }
  if (!mlc_loaded)
  {
    return LIS2DUXS12_MOTION_INVALID_ARGUMENT;
  }
  route.emb_function = PROPERTY_ENABLE;
  return (lis2duxs12_pin_int1_route_set(&motion_context, &route) == 0) ?
         LIS2DUXS12_MOTION_OK : LIS2DUXS12_MOTION_BUS_ERROR;
}

lis2duxs12_motion_result_t LIS2DUXS12_MotionSetClassRules(
    const lis2duxs12_class_rule_t *rules, size_t rule_count)
{
  if ((rule_count > LIS2DUXS12_MOTION_MAX_CLASS_RULES) ||
      ((rule_count != 0U) && (rules == NULL)))
  {
    return LIS2DUXS12_MOTION_INVALID_ARGUMENT;
  }
  if (rule_count != 0U)
  {
    memcpy(class_rules, rules, rule_count * sizeof(class_rules[0]));
  }
  class_rule_count = rule_count;
  return LIS2DUXS12_MOTION_OK;
}

lis2duxs12_motion_result_t LIS2DUXS12_MotionProcessInterrupt(void)
{
  lis2duxs12_mlc_status_mainpage_t status = {0};
  lis2duxs12_acceleration_t acceleration;
  uint8_t axis;
  if (!motion_present)
  {
    return LIS2DUXS12_MOTION_NOT_PRESENT;
  }
  if ((lis2duxs12_mlc_status_get(&motion_context, &status) != 0) ||
      (LIS2DUXS12_MotionReadAcceleration(&acceleration) != LIS2DUXS12_MOTION_OK))
  {
    return LIS2DUXS12_MOTION_BUS_ERROR;
  }
  latest_event.mlc_status = LIS2DUXS12_MlcStatusToMask(&status);
  if ((latest_event.mlc_status != 0U) &&
      (lis2duxs12_mlc_out_get(&motion_context, latest_event.mlc_output) != 0))
  {
    return LIS2DUXS12_MOTION_BUS_ERROR;
  }
  for (axis = 0U; axis < 3U; axis++)
  {
    latest_event.acceleration_raw[axis] = acceleration.raw[axis];
    latest_event.acceleration_mg[axis] = acceleration.mg[axis];
  }
  latest_event.activity = LIS2DUXS12_DecodeActivity(&latest_event);
  latest_event.sequence++;
  event_valid = true;
  return LIS2DUXS12_MOTION_OK;
}

bool LIS2DUXS12_MotionGetLatestEvent(lis2duxs12_motion_event_t *event)
{
  if ((event == NULL) || (!event_valid))
  {
    return false;
  }
  *event = latest_event;
  return true;
}

/* QVar (electrometer) wear/no-wear detection - uses the official
 * AH_QVAR_CFG/OUT_T_AH_QVAR register API from the current lis2duxs12-pid
 * driver (lis2duxs12_ah_qvar_data_get() internally primes the read with the
 * preceding OUT_Z_H register in the same burst, which an older driver
 * snapshot previously bundled in this project did not do - that missing
 * priming read is the most likely cause of the erratic/stuck raw values
 * seen on real hardware, not a hardware fault). */
lis2duxs12_motion_result_t LIS2DUXS12_MotionConfigureQvar(
    bool enable, lis2duxs12_ah_qvar_gain_t gain, lis2duxs12_ah_qvar_zin_t zin)
{
  lis2duxs12_ah_qvar_mode_t qvar_mode = {0};

  if (!motion_present)
  {
    return LIS2DUXS12_MOTION_NOT_PRESENT;
  }

  qvar_mode.ah_qvar_en = enable ? PROPERTY_ENABLE : PROPERTY_DISABLE;
  qvar_mode.ah_qvar_notch_en = PROPERTY_DISABLE;
  qvar_mode.ah_qvar_zin = zin;
  qvar_mode.ah_qvar_gain = gain;

  return (lis2duxs12_ah_qvar_mode_set(&motion_context, qvar_mode) == 0) ?
         LIS2DUXS12_MOTION_OK : LIS2DUXS12_MOTION_BUS_ERROR;
}

lis2duxs12_motion_result_t LIS2DUXS12_MotionInitQvar(void)
{
  /* Only one AH_QVAR electrode pin is wired on this board (the other,
   * unrelated INT2, is intentionally left floating). ST's own example
   * (lis2duxs12_qvar_read_data.c) defaults to 520MOhm/0.5x gain; kept here
   * to match that known-good reference starting point. */
  return LIS2DUXS12_MotionConfigureQvar(true, LIS2DUXS12_GAIN_0_5, LIS2DUXS12_520MOhm);
}

lis2duxs12_motion_result_t LIS2DUXS12_MotionReadQvar(int16_t *qvar_value)
{
  lis2duxs12_ah_qvar_data_t data = {0};

  if (qvar_value == NULL)
  {
    return LIS2DUXS12_MOTION_INVALID_ARGUMENT;
  }
  if (!motion_present)
  {
    return LIS2DUXS12_MOTION_NOT_PRESENT;
  }

  if (lis2duxs12_ah_qvar_data_get(&motion_context, &data) != 0)
  {
    return LIS2DUXS12_MOTION_BUS_ERROR;
  }

  *qvar_value = data.raw;
  return LIS2DUXS12_MOTION_OK;
}
