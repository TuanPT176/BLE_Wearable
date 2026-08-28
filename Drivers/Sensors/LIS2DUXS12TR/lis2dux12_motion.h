#ifndef LIS2DUX12_MOTION_H
#define LIS2DUX12_MOTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lis2dux12_platform.h"

#define LIS2DUX12_MLC_OUTPUT_COUNT     4U
#define LIS2DUX12_MOTION_MAX_CLASS_RULES  16U

typedef enum
{
  LIS2DUX12_MOTION_OK = 0,
  LIS2DUX12_MOTION_NOT_PRESENT,
  LIS2DUX12_MOTION_BUS_ERROR,
  LIS2DUX12_MOTION_INVALID_ARGUMENT
} lis2dux12_motion_result_t;

typedef enum
{
  LIS2DUX12_ACTIVITY_UNKNOWN = 0,
  LIS2DUX12_ACTIVITY_NOT_WORN,
  LIS2DUX12_ACTIVITY_SLEEPING,
  LIS2DUX12_ACTIVITY_NORMAL,
  LIS2DUX12_ACTIVITY_FALL
} lis2dux12_activity_t;

typedef struct
{
  uint8_t output_index;
  uint8_t output_value;
  lis2dux12_activity_t activity;
} lis2dux12_class_rule_t;

typedef struct
{
  int16_t raw[3];
  int32_t mg[3];
} lis2dux12_acceleration_t;

typedef struct
{
  int16_t acceleration_raw[3];
  int32_t acceleration_mg[3];
  uint8_t mlc_status;
  uint8_t mlc_output[LIS2DUX12_MLC_OUTPUT_COUNT];
  lis2dux12_activity_t activity;
  uint32_t sequence;
} lis2dux12_motion_event_t;

lis2dux12_motion_result_t LIS2DUX12_MotionInit(I2C_HandleTypeDef *i2c);
uint16_t LIS2DUX12_MotionGetHalAddress(void);
lis2dux12_motion_result_t LIS2DUX12_MotionReadAcceleration(
    lis2dux12_acceleration_t *sample);
lis2dux12_motion_result_t LIS2DUX12_MotionLoadUcf(
    const ucf_line_t *configuration, size_t line_count);
lis2dux12_motion_result_t LIS2DUX12_MotionArmMlcInterrupt(void);
lis2dux12_motion_result_t LIS2DUX12_MotionSetClassRules(
    const lis2dux12_class_rule_t *rules, size_t rule_count);
lis2dux12_motion_result_t LIS2DUX12_MotionProcessInterrupt(void);
bool LIS2DUX12_MotionGetLatestEvent(lis2dux12_motion_event_t *event);
lis2dux12_motion_result_t LIS2DUX12_MotionInitQvar(void);
lis2dux12_motion_result_t LIS2DUX12_MotionReadQvar(int16_t *qvar_value);


#endif /* LIS2DUX12_MOTION_H */
