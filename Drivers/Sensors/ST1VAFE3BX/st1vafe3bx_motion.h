#ifndef ST1VAFE3BX_MOTION_H
#define ST1VAFE3BX_MOTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "st1vafe3bx_platform.h"

#define ST1VAFE3BX_MLC_OUTPUT_COUNT  4U
#define ST1VAFE3BX_FSM_OUTPUT_COUNT  8U
#define ST1VAFE3BX_MOTION_MAX_CLASS_RULES  16U

typedef enum
{
  ST1VAFE3BX_MOTION_OK = 0,
  ST1VAFE3BX_MOTION_NOT_PRESENT,
  ST1VAFE3BX_MOTION_NOT_READY,
  ST1VAFE3BX_MOTION_BUS_ERROR,
  ST1VAFE3BX_MOTION_INVALID_ARGUMENT
} st1vafe3bx_motion_result_t;

typedef enum
{
  ST1VAFE3BX_ACTIVITY_UNKNOWN = 0,
  ST1VAFE3BX_ACTIVITY_NOT_WORN,
  ST1VAFE3BX_ACTIVITY_SLEEPING,
  ST1VAFE3BX_ACTIVITY_NORMAL,
  ST1VAFE3BX_ACTIVITY_FALL
} st1vafe3bx_activity_t;

typedef enum
{
  ST1VAFE3BX_CLASSIFIER_MLC = 0,
  ST1VAFE3BX_CLASSIFIER_FSM
} st1vafe3bx_classifier_t;

typedef struct
{
  st1vafe3bx_classifier_t classifier;
  uint8_t output_index;
  uint8_t output_value;
  st1vafe3bx_activity_t activity;
} st1vafe3bx_class_rule_t;

typedef struct
{
  int16_t raw[3];
  int32_t mg[3];
} st1vafe3bx_acceleration_t;

typedef struct
{
  int16_t acceleration_raw[3];
  int32_t acceleration_mg[3];
  uint8_t mlc_status;
  uint8_t fsm_status;
  uint8_t mlc_output[ST1VAFE3BX_MLC_OUTPUT_COUNT];
  uint8_t fsm_output[ST1VAFE3BX_FSM_OUTPUT_COUNT];
  bool free_fall;
  bool wake_up;
  st1vafe3bx_activity_t activity;
  uint32_t sequence;
} st1vafe3bx_motion_event_t;

/* Optional sensor: failure to probe does not fail the rest of the firmware. */
st1vafe3bx_motion_result_t ST1VAFE3BX_MotionInit(I2C_HandleTypeDef *i2c);
bool ST1VAFE3BX_MotionIsPresent(void);
uint16_t ST1VAFE3BX_MotionGetHalAddress(void);
st1vafe3bx_motion_result_t ST1VAFE3BX_MotionConfigureAccelerometer(
    st1vafe3bx_odr_t odr,
    st1vafe3bx_fs_t full_scale,
    st1vafe3bx_bw_t bandwidth);
st1vafe3bx_motion_result_t ST1VAFE3BX_MotionReadAcceleration(
    st1vafe3bx_acceleration_t *sample);

/*
 * Load a future Unico-generated MLC/FSM UCF register table. This is called
 * from task context only; never from the PB2 interrupt handler.
 */
st1vafe3bx_motion_result_t ST1VAFE3BX_MotionLoadUcf(
    const ucf_line_t *configuration,
    size_t line_count);

/* Route the embedded-function interrupt to the sensor INT pin after UCF load. */
st1vafe3bx_motion_result_t ST1VAFE3BX_MotionArmClassifierInterrupt(void);

/* Install output-code meanings after the final UCF model is generated. */
st1vafe3bx_motion_result_t ST1VAFE3BX_MotionSetClassRules(
    const st1vafe3bx_class_rule_t *rules,
    size_t rule_count);

/* Read and clear MLC/FSM interrupt sources and capture the current acceleration. */
st1vafe3bx_motion_result_t ST1VAFE3BX_MotionProcessInterrupt(void);
bool ST1VAFE3BX_MotionGetLatestEvent(st1vafe3bx_motion_event_t *event);

st1vafe3bx_motion_result_t ST1VAFE3BX_MotionPowerDown(void);

#endif /* ST1VAFE3BX_MOTION_H */
