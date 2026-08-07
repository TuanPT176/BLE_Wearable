#include "lis2dux12_motion.h"

#include <string.h>

static stmdev_ctx_t motion_context;
static lis2dux12_platform_t motion_platform;
static lis2dux12_md_t motion_mode;
static lis2dux12_motion_event_t latest_event;
static lis2dux12_class_rule_t class_rules[LIS2DUX12_MOTION_MAX_CLASS_RULES];
static size_t class_rule_count;
static bool motion_present;
static bool mlc_loaded;
static bool event_valid;

static int32_t LIS2DUX12_MgRound(float_t value)
{
  return (value >= 0.0f) ? (int32_t)(value + 0.5f) :
                           (int32_t)(value - 0.5f);
}

static uint8_t LIS2DUX12_MlcStatusToMask(
    const lis2dux12_mlc_status_mainpage_t *status)
{
  return (uint8_t)((status->is_mlc1 << 0U) |
                   (status->is_mlc2 << 1U) |
                   (status->is_mlc3 << 2U) |
                   (status->is_mlc4 << 3U));
}

static lis2dux12_activity_t LIS2DUX12_DecodeActivity(
    const lis2dux12_motion_event_t *event)
{
  size_t index;
  for (index = 0U; index < class_rule_count; index++)
  {
    const lis2dux12_class_rule_t *rule = &class_rules[index];
    if ((rule->output_index < LIS2DUX12_MLC_OUTPUT_COUNT) &&
        ((event->mlc_status & (1U << rule->output_index)) != 0U) &&
        (event->mlc_output[rule->output_index] == rule->output_value))
    {
      return rule->activity;
    }
  }
  return LIS2DUX12_ACTIVITY_UNKNOWN;
}

lis2dux12_motion_result_t LIS2DUX12_MotionInit(I2C_HandleTypeDef *i2c)
{
  bool address_high;
  lis2dux12_pin_conf_t pin_configuration = {0};
  memset(&latest_event, 0, sizeof(latest_event));
  motion_present = false;
  mlc_loaded = false;
  event_valid = false;
  class_rule_count = 0U;
  if (i2c == NULL)
  {
    return LIS2DUX12_MOTION_INVALID_ARGUMENT;
  }
  for (address_high = false; ; address_high = true)
  {
    LIS2DUX12_PlatformInit(&motion_context, &motion_platform, i2c, address_high);
    if (LIS2DUX12_PlatformProbe(&motion_context) == 0)
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
    return LIS2DUX12_MOTION_NOT_PRESENT;
  }
  motion_mode.odr = LIS2DUX12_100Hz_LP;
  motion_mode.fs = LIS2DUX12_4g;
  motion_mode.bw = LIS2DUX12_ODR_div_4;
  pin_configuration.int1_int2_push_pull = PROPERTY_ENABLE;
  if ((lis2dux12_sw_reset(&motion_context) != 0) ||
      (lis2dux12_init_set(&motion_context) != 0) ||
      (lis2dux12_mode_set(&motion_context, &motion_mode) != 0) ||
      (lis2dux12_pin_conf_set(&motion_context, &pin_configuration) != 0) ||
      (lis2dux12_int_pin_polarity_set(&motion_context,
                                      LIS2DUX12_ACTIVE_HIGH) != 0))
  {
    motion_present = false;
    return LIS2DUX12_MOTION_BUS_ERROR;
  }
  return LIS2DUX12_MOTION_OK;
}

uint16_t LIS2DUX12_MotionGetHalAddress(void)
{
  return motion_platform.hal_address;
}

lis2dux12_motion_result_t LIS2DUX12_MotionReadAcceleration(
    lis2dux12_acceleration_t *sample)
{
  lis2dux12_xl_data_t data = {0};
  uint8_t axis;
  if (sample == NULL)
  {
    return LIS2DUX12_MOTION_INVALID_ARGUMENT;
  }
  if (!motion_present)
  {
    return LIS2DUX12_MOTION_NOT_PRESENT;
  }
  if (lis2dux12_xl_data_get(&motion_context, &motion_mode, &data) != 0)
  {
    return LIS2DUX12_MOTION_BUS_ERROR;
  }
  for (axis = 0U; axis < 3U; axis++)
  {
    sample->raw[axis] = data.raw[axis];
    sample->mg[axis] = LIS2DUX12_MgRound(data.mg[axis]);
  }
  return LIS2DUX12_MOTION_OK;
}

lis2dux12_motion_result_t LIS2DUX12_MotionLoadUcf(
    const ucf_line_t *configuration, size_t line_count)
{
  size_t line;
  if ((configuration == NULL) || (line_count == 0U))
  {
    return LIS2DUX12_MOTION_INVALID_ARGUMENT;
  }
  if (!motion_present)
  {
    return LIS2DUX12_MOTION_NOT_PRESENT;
  }
  mlc_loaded = false;
  for (line = 0U; line < line_count; line++)
  {
    uint8_t value = configuration[line].data;
    if (lis2dux12_write_reg(&motion_context, configuration[line].address,
                            &value, 1U) != 0)
    {
      (void)lis2dux12_mem_bank_set(&motion_context, LIS2DUX12_MAIN_MEM_BANK);
      return LIS2DUX12_MOTION_BUS_ERROR;
    }
  }
  if (lis2dux12_mem_bank_set(&motion_context, LIS2DUX12_MAIN_MEM_BANK) != 0)
  {
    return LIS2DUX12_MOTION_BUS_ERROR;
  }
  mlc_loaded = true;
  return LIS2DUX12_MOTION_OK;
}

lis2dux12_motion_result_t LIS2DUX12_MotionArmMlcInterrupt(void)
{
  lis2dux12_pin_int_route_t route = {0};
  if (!motion_present)
  {
    return LIS2DUX12_MOTION_NOT_PRESENT;
  }
  if (!mlc_loaded)
  {
    return LIS2DUX12_MOTION_INVALID_ARGUMENT;
  }
  route.emb_function = PROPERTY_ENABLE;
  return (lis2dux12_pin_int1_route_set(&motion_context, &route) == 0) ?
         LIS2DUX12_MOTION_OK : LIS2DUX12_MOTION_BUS_ERROR;
}

lis2dux12_motion_result_t LIS2DUX12_MotionSetClassRules(
    const lis2dux12_class_rule_t *rules, size_t rule_count)
{
  if ((rule_count > LIS2DUX12_MOTION_MAX_CLASS_RULES) ||
      ((rule_count != 0U) && (rules == NULL)))
  {
    return LIS2DUX12_MOTION_INVALID_ARGUMENT;
  }
  if (rule_count != 0U)
  {
    memcpy(class_rules, rules, rule_count * sizeof(class_rules[0]));
  }
  class_rule_count = rule_count;
  return LIS2DUX12_MOTION_OK;
}

lis2dux12_motion_result_t LIS2DUX12_MotionProcessInterrupt(void)
{
  lis2dux12_mlc_status_mainpage_t status = {0};
  lis2dux12_acceleration_t acceleration;
  uint8_t axis;
  if (!motion_present)
  {
    return LIS2DUX12_MOTION_NOT_PRESENT;
  }
  if ((lis2dux12_mlc_status_get(&motion_context, &status) != 0) ||
      (LIS2DUX12_MotionReadAcceleration(&acceleration) != LIS2DUX12_MOTION_OK))
  {
    return LIS2DUX12_MOTION_BUS_ERROR;
  }
  latest_event.mlc_status = LIS2DUX12_MlcStatusToMask(&status);
  if ((latest_event.mlc_status != 0U) &&
      (lis2dux12_mlc_out_get(&motion_context, latest_event.mlc_output) != 0))
  {
    return LIS2DUX12_MOTION_BUS_ERROR;
  }
  for (axis = 0U; axis < 3U; axis++)
  {
    latest_event.acceleration_raw[axis] = acceleration.raw[axis];
    latest_event.acceleration_mg[axis] = acceleration.mg[axis];
  }
  latest_event.activity = LIS2DUX12_DecodeActivity(&latest_event);
  latest_event.sequence++;
  event_valid = true;
  return LIS2DUX12_MOTION_OK;
}

bool LIS2DUX12_MotionGetLatestEvent(lis2dux12_motion_event_t *event)
{
  if ((!event_valid) || (event == NULL))
  {
    return false;
  }
  *event = latest_event;
  return true;
}
