#ifndef POWER_POLICY_H
#define POWER_POLICY_H

#include <stdint.h>

/* Temporary Threshold Macros (in mV) */
#define POWER_THRESHOLD_HIGH     2800U
#define POWER_THRESHOLD_NORMAL   2500U
#define POWER_THRESHOLD_LOW      2100U
#define POWER_THRESHOLD_CRITICAL 1800U

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
