#include "st1vafe3bx_motion.h"

#include <string.h>

static stmdev_ctx_t motion_context;
static st1vafe3bx_platform_t motion_platform;
static st1vafe3bx_md_t motion_mode;
static st1vafe3bx_motion_event_t latest_event;
static st1vafe3bx_class_rule_t classification_rules[
    ST1VAFE3BX_MOTION_MAX_CLASS_RULES];
static size_t classification_rule_count;
static bool motion_present;
static bool basic_interrupts_enabled;
static bool classifier_loaded;
static bool event_valid;

static uint8_t ST1VAFE3BX_MlcStatusToMask(
    const st1vafe3bx_mlc_status_mainpage_t *status)
{
  return (uint8_t)((status->is_mlc1 << 0U) |
                   (status->is_mlc2 << 1U) |
                   (status->is_mlc3 << 2U) |
                   (status->is_mlc4 << 3U));
}

static uint8_t ST1VAFE3BX_FsmStatusToMask(
    const st1vafe3bx_fsm_status_mainpage_t *status)
{
  return (uint8_t)((status->is_fsm1 << 0U) |
                   (status->is_fsm2 << 1U) |
                   (status->is_fsm3 << 2U) |
                   (status->is_fsm4 << 3U) |
                   (status->is_fsm5 << 4U) |
                   (status->is_fsm6 << 5U) |
                   (status->is_fsm7 << 6U) |
                   (status->is_fsm8 << 7U));
}

static int32_t ST1VAFE3BX_MgRound(float_t value)
{
  return (value >= 0.0f) ? (int32_t)(value + 0.5f) : (int32_t)(value - 0.5f);
}

static st1vafe3bx_activity_t ST1VAFE3BX_DecodeActivity(
    const st1vafe3bx_motion_event_t *event)
{
  size_t index;

  for (index = 0U; index < classification_rule_count; index++)
  {
    const st1vafe3bx_class_rule_t *rule = &classification_rules[index];
    if ((rule->classifier == ST1VAFE3BX_CLASSIFIER_MLC) &&
        (rule->output_index < ST1VAFE3BX_MLC_OUTPUT_COUNT) &&
        ((event->mlc_status & (1U << rule->output_index)) != 0U) &&
        (event->mlc_output[rule->output_index] == rule->output_value))
    {
      return rule->activity;
    }
    if ((rule->classifier == ST1VAFE3BX_CLASSIFIER_FSM) &&
        (rule->output_index < ST1VAFE3BX_FSM_OUTPUT_COUNT) &&
        ((event->fsm_status & (1U << rule->output_index)) != 0U) &&
        (event->fsm_output[rule->output_index] == rule->output_value))
    {
      return rule->activity;
    }
  }
  return ST1VAFE3BX_ACTIVITY_UNKNOWN;
}

static int32_t ST1VAFE3BX_ConfigureBaseMotion(void)
{
  st1vafe3bx_pin_conf_t pin_configuration = {0};
  st1vafe3bx_pin_int_route_t interrupt_route = {0};
  st1vafe3bx_int_config_t interrupt_configuration = {0};
  st1vafe3bx_wakeup_config_t wakeup_configuration = {0};
  int32_t result;

  result = st1vafe3bx_sw_reset(&motion_context);
  if (result != 0)
  {
    return result;
  }
  result = st1vafe3bx_init_set(&motion_context);
  if (result != 0)
  {
    return result;
  }

  motion_mode.fs = ST1VAFE3BX_4g;
  motion_mode.odr = ST1VAFE3BX_50Hz_LP;
  motion_mode.bw = ST1VAFE3BX_BW_ODR_div_4;
  result = st1vafe3bx_mode_set(&motion_context, &motion_mode);

  /* Active-high push-pull matches PB2 rising-edge EXTI with an MCU pull-down. */
  pin_configuration.int_push_pull = PROPERTY_ENABLE;
  result += st1vafe3bx_pin_conf_set(&motion_context, &pin_configuration);
  result += st1vafe3bx_int_pin_polarity_set(&motion_context,
                                            ST1VAFE3BX_ACTIVE_HIGH);

  /*
   * Low-rate inertial wake/free-fall events; no periodic DRDY interrupt.
   * At 50 Hz, FF duration 3 is approximately 60 ms.
   */
  result += st1vafe3bx_ff_thresholds_set(&motion_context,
                                         ST1VAFE3BX_312_mg);
  result += st1vafe3bx_ff_duration_set(&motion_context, 3U);
  wakeup_configuration.wake_dur = ST1VAFE3BX_1_ODR;
  wakeup_configuration.wake_ths = 48U;
  wakeup_configuration.wake_ths_weight = 1U;
  wakeup_configuration.wake_enable = ST1VAFE3BX_SLEEP_OFF;
  wakeup_configuration.inact_odr = ST1VAFE3BX_ODR_NO_CHANGE;
  result += st1vafe3bx_wakeup_config_set(&motion_context,
                                         wakeup_configuration);
  interrupt_route.free_fall = PROPERTY_ENABLE;
  interrupt_route.wake_up = PROPERTY_ENABLE;
  result += st1vafe3bx_pin_int_route_set(&motion_context, &interrupt_route);
  interrupt_configuration.int_cfg = ST1VAFE3BX_INT_LATCHED;
  result += st1vafe3bx_int_config_set(&motion_context,
                                      &interrupt_configuration);
  basic_interrupts_enabled = (result == 0);
  return result;
}

st1vafe3bx_motion_result_t ST1VAFE3BX_MotionInit(I2C_HandleTypeDef *i2c)
{
  bool address_high;

  memset(&latest_event, 0, sizeof(latest_event));
  motion_present = false;
  basic_interrupts_enabled = false;
  classifier_loaded = false;
  event_valid = false;
  classification_rule_count = 0U;
  if (i2c == NULL)
  {
    return ST1VAFE3BX_MOTION_INVALID_ARGUMENT;
  }

  /* SA0 may select either ST address; probe both without assuming the PCB strap. */
  for (address_high = false; ; address_high = true)
  {
    ST1VAFE3BX_PlatformInit(&motion_context, &motion_platform,
                            i2c, address_high);
    if (ST1VAFE3BX_PlatformProbe(&motion_context) == 0)
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
    return ST1VAFE3BX_MOTION_NOT_PRESENT;
  }
  if (ST1VAFE3BX_ConfigureBaseMotion() != 0)
  {
    motion_present = false;
    return ST1VAFE3BX_MOTION_BUS_ERROR;
  }
  return ST1VAFE3BX_MOTION_OK;
}

bool ST1VAFE3BX_MotionIsPresent(void)
{
  return motion_present;
}

uint16_t ST1VAFE3BX_MotionGetHalAddress(void)
{
  return motion_platform.hal_address;
}

st1vafe3bx_motion_result_t ST1VAFE3BX_MotionConfigureAccelerometer(
    st1vafe3bx_odr_t odr,
    st1vafe3bx_fs_t full_scale,
    st1vafe3bx_bw_t bandwidth)
{
  st1vafe3bx_md_t requested_mode;

  if (!motion_present)
  {
    return ST1VAFE3BX_MOTION_NOT_PRESENT;
  }
  requested_mode.odr = odr;
  requested_mode.fs = full_scale;
  requested_mode.bw = bandwidth;
  if (st1vafe3bx_mode_set(&motion_context, &requested_mode) != 0)
  {
    return ST1VAFE3BX_MOTION_INVALID_ARGUMENT;
  }
  motion_mode = requested_mode;
  return ST1VAFE3BX_MOTION_OK;
}

st1vafe3bx_motion_result_t ST1VAFE3BX_MotionReadAcceleration(
    st1vafe3bx_acceleration_t *sample)
{
  st1vafe3bx_xl_data_t acceleration = {0};
  uint8_t axis;

  if (sample == NULL)
  {
    return ST1VAFE3BX_MOTION_INVALID_ARGUMENT;
  }
  if (!motion_present)
  {
    return ST1VAFE3BX_MOTION_NOT_PRESENT;
  }
  if (st1vafe3bx_xl_data_get(&motion_context, &motion_mode,
                             &acceleration) != 0)
  {
    return ST1VAFE3BX_MOTION_BUS_ERROR;
  }
  for (axis = 0U; axis < 3U; axis++)
  {
    sample->raw[axis] = acceleration.raw[axis];
    sample->mg[axis] = ST1VAFE3BX_MgRound(acceleration.mg[axis]);
  }
  return ST1VAFE3BX_MOTION_OK;
}

st1vafe3bx_motion_result_t ST1VAFE3BX_MotionLoadUcf(
    const ucf_line_t *configuration,
    size_t line_count)
{
  size_t line;
  int32_t result = 0;

  if ((configuration == NULL) || (line_count == 0U))
  {
    return ST1VAFE3BX_MOTION_INVALID_ARGUMENT;
  }
  if (!motion_present)
  {
    return ST1VAFE3BX_MOTION_NOT_PRESENT;
  }

  classifier_loaded = false;
  for (line = 0U; line < line_count; line++)
  {
    uint8_t value = configuration[line].data;
    result = st1vafe3bx_write_reg(&motion_context,
                                  configuration[line].address,
                                  &value,
                                  1U);
    if (result != 0)
    {
      break;
    }
  }

  /* Always return to the main register bank after a malformed/partial table. */
  result += st1vafe3bx_mem_bank_set(&motion_context,
                                    ST1VAFE3BX_MAIN_MEM_BANK);
  if (result != 0)
  {
    return ST1VAFE3BX_MOTION_BUS_ERROR;
  }

  /* The UCF may replace full-scale/ODR; retain the actual conversion mode. */
  if (st1vafe3bx_mode_get(&motion_context, &motion_mode) != 0)
  {
    return ST1VAFE3BX_MOTION_BUS_ERROR;
  }

  classifier_loaded = true;
  event_valid = false;
  return ST1VAFE3BX_MOTION_OK;
}

st1vafe3bx_motion_result_t ST1VAFE3BX_MotionSetClassRules(
    const st1vafe3bx_class_rule_t *rules,
    size_t rule_count)
{
  size_t index;

  if (((rules == NULL) && (rule_count != 0U)) ||
      (rule_count > ST1VAFE3BX_MOTION_MAX_CLASS_RULES))
  {
    return ST1VAFE3BX_MOTION_INVALID_ARGUMENT;
  }
  for (index = 0U; index < rule_count; index++)
  {
    if ((rules[index].classifier > ST1VAFE3BX_CLASSIFIER_FSM) ||
        ((rules[index].classifier == ST1VAFE3BX_CLASSIFIER_MLC) &&
         (rules[index].output_index >= ST1VAFE3BX_MLC_OUTPUT_COUNT)) ||
        ((rules[index].classifier == ST1VAFE3BX_CLASSIFIER_FSM) &&
         (rules[index].output_index >= ST1VAFE3BX_FSM_OUTPUT_COUNT)) ||
        (rules[index].activity > ST1VAFE3BX_ACTIVITY_FALL))
    {
      return ST1VAFE3BX_MOTION_INVALID_ARGUMENT;
    }
  }

  if (rule_count != 0U)
  {
    memcpy(classification_rules, rules,
           rule_count * sizeof(classification_rules[0]));
  }
  classification_rule_count = rule_count;
  return ST1VAFE3BX_MOTION_OK;
}

st1vafe3bx_motion_result_t ST1VAFE3BX_MotionArmClassifierInterrupt(void)
{
  st1vafe3bx_pin_int_route_t route = {0};
  st1vafe3bx_int_config_t interrupt_configuration = {0};
  int32_t result;

  if (!motion_present)
  {
    return ST1VAFE3BX_MOTION_NOT_PRESENT;
  }
  if (!classifier_loaded)
  {
    return ST1VAFE3BX_MOTION_NOT_READY;
  }

  result = st1vafe3bx_embedded_state_set(&motion_context, PROPERTY_ENABLE);
  route.emb_function = PROPERTY_ENABLE;
  route.free_fall = basic_interrupts_enabled ? PROPERTY_ENABLE : PROPERTY_DISABLE;
  route.wake_up = basic_interrupts_enabled ? PROPERTY_ENABLE : PROPERTY_DISABLE;
  result += st1vafe3bx_pin_int_route_set(&motion_context, &route);
  result += st1vafe3bx_embedded_int_cfg_set(
      &motion_context, ST1VAFE3BX_EMBEDDED_INT_LATCHED);
  interrupt_configuration.int_cfg = ST1VAFE3BX_INT_LATCHED;
  result += st1vafe3bx_int_config_set(&motion_context,
                                      &interrupt_configuration);
  return (result == 0) ? ST1VAFE3BX_MOTION_OK
                       : ST1VAFE3BX_MOTION_BUS_ERROR;
}

st1vafe3bx_motion_result_t ST1VAFE3BX_MotionProcessInterrupt(void)
{
  st1vafe3bx_all_sources_t all_sources = {0};
  st1vafe3bx_mlc_status_mainpage_t mlc_status = {0};
  st1vafe3bx_fsm_status_mainpage_t fsm_status = {0};
  st1vafe3bx_xl_data_t acceleration = {0};
  st1vafe3bx_motion_event_t event = {0};
  int32_t result;
  uint8_t axis;

  if (!motion_present)
  {
    return ST1VAFE3BX_MOTION_NOT_PRESENT;
  }
  result = st1vafe3bx_all_sources_get(&motion_context, &all_sources);
  event.free_fall = (all_sources.free_fall != 0U);
  event.wake_up = (all_sources.wake_up != 0U);

  if (classifier_loaded)
  {
    result += st1vafe3bx_mlc_status_get(&motion_context, &mlc_status);
    result += st1vafe3bx_fsm_status_get(&motion_context, &fsm_status);
    event.mlc_status = ST1VAFE3BX_MlcStatusToMask(&mlc_status);
    event.fsm_status = ST1VAFE3BX_FsmStatusToMask(&fsm_status);
  }

  if (event.mlc_status != 0U)
  {
    result += st1vafe3bx_mlc_out_get(&motion_context, event.mlc_output);
  }
  if (event.fsm_status != 0U)
  {
    result += st1vafe3bx_fsm_out_get(&motion_context, event.fsm_output);
  }
  result += st1vafe3bx_xl_data_get(&motion_context, &motion_mode,
                                   &acceleration);
  if (result != 0)
  {
    return ST1VAFE3BX_MOTION_BUS_ERROR;
  }

  for (axis = 0U; axis < 3U; axis++)
  {
    event.acceleration_raw[axis] = acceleration.raw[axis];
    event.acceleration_mg[axis] = ST1VAFE3BX_MgRound(acceleration.mg[axis]);
  }
  event.activity = ST1VAFE3BX_DecodeActivity(&event);
  event.sequence = latest_event.sequence + 1U;
  latest_event = event;
  event_valid = true;
  return ST1VAFE3BX_MOTION_OK;
}

bool ST1VAFE3BX_MotionGetLatestEvent(st1vafe3bx_motion_event_t *event)
{
  if ((!event_valid) || (event == NULL))
  {
    return false;
  }
  *event = latest_event;
  return true;
}

st1vafe3bx_motion_result_t ST1VAFE3BX_MotionPowerDown(void)
{
  if (!motion_present)
  {
    return ST1VAFE3BX_MOTION_NOT_PRESENT;
  }
  motion_mode.odr = ST1VAFE3BX_OFF;
  return (st1vafe3bx_mode_set(&motion_context, &motion_mode) == 0)
             ? ST1VAFE3BX_MOTION_OK
             : ST1VAFE3BX_MOTION_BUS_ERROR;
}
