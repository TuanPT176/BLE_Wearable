#ifndef POWER_POLICY_H
#define POWER_POLICY_H

#include <stdint.h>

/* POWER_THRESHOLD_* (mV): wearable_config.h. */
#include "../wearable_config.h"

typedef enum {
  POWER_PROFILE_HIGH = 0,
  POWER_PROFILE_NORMAL,
  POWER_PROFILE_LOW,
  POWER_PROFILE_CRITICAL
} PowerProfile_t;

typedef struct {
  uint32_t sensor_interval_ms;
  uint32_t ble_interval_ms;
  uint32_t logger_interval_ms;
} PowerPolicyConfig_t;

void PowerPolicy_Init(void);
void PowerPolicy_Update(uint16_t vcap_mv);
PowerProfile_t PowerPolicy_GetProfile(void);
PowerPolicyConfig_t PowerPolicy_GetConfig(void);

#endif /* POWER_POLICY_H */
