#ifndef LIS2DUXS12_MOTION_H
#define LIS2DUXS12_MOTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lis2duxs12_platform.h"

#define LIS2DUXS12_MLC_OUTPUT_COUNT       4U
#define LIS2DUXS12_MOTION_MAX_CLASS_RULES 16U

typedef enum
{
  LIS2DUXS12_MOTION_OK = 0,
  LIS2DUXS12_MOTION_NOT_PRESENT,
  LIS2DUXS12_MOTION_BUS_ERROR,
  LIS2DUXS12_MOTION_INVALID_ARGUMENT
} lis2duxs12_motion_result_t;

typedef enum
{
  LIS2DUXS12_ACTIVITY_UNKNOWN = 0,
  LIS2DUXS12_ACTIVITY_NOT_WORN,
  LIS2DUXS12_ACTIVITY_SLEEPING,
  LIS2DUXS12_ACTIVITY_NORMAL,
  LIS2DUXS12_ACTIVITY_FALL
} lis2duxs12_activity_t;

typedef struct
{
  uint8_t output_index;
  uint8_t output_value;
  lis2duxs12_activity_t activity;
} lis2duxs12_class_rule_t;

typedef struct
{
  int16_t raw[3];
  int32_t mg[3];
} lis2duxs12_acceleration_t;

typedef struct
{
  int16_t acceleration_raw[3];
  int32_t acceleration_mg[3];
  uint8_t mlc_status;
  uint8_t mlc_output[LIS2DUXS12_MLC_OUTPUT_COUNT];
  lis2duxs12_activity_t activity;
  uint32_t sequence;
} lis2duxs12_motion_event_t;

lis2duxs12_motion_result_t LIS2DUXS12_MotionInit(I2C_HandleTypeDef *i2c);
uint16_t LIS2DUXS12_MotionGetHalAddress(void);
lis2duxs12_motion_result_t LIS2DUXS12_MotionReadAcceleration(
    lis2duxs12_acceleration_t *sample);
lis2duxs12_motion_result_t LIS2DUXS12_MotionLoadUcf(
    const ucf_line_t *configuration, size_t line_count);
lis2duxs12_motion_result_t LIS2DUXS12_MotionArmMlcInterrupt(void);
lis2duxs12_motion_result_t LIS2DUXS12_MotionSetClassRules(
    const lis2duxs12_class_rule_t *rules, size_t rule_count);
lis2duxs12_motion_result_t LIS2DUXS12_MotionProcessInterrupt(void);
bool LIS2DUXS12_MotionGetLatestEvent(lis2duxs12_motion_event_t *event);
lis2duxs12_motion_result_t LIS2DUXS12_MotionInitQvar(void);
lis2duxs12_motion_result_t LIS2DUXS12_MotionConfigureQvar(
    bool enable, lis2duxs12_ah_qvar_gain_t gain, lis2duxs12_ah_qvar_zin_t zin);
lis2duxs12_motion_result_t LIS2DUXS12_MotionReadQvar(int16_t *qvar_value);

#endif /* LIS2DUXS12_MOTION_H */
