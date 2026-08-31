#include "power_policy.h"

static PowerProfile_t current_profile = POWER_PROFILE_NORMAL;

static PowerPolicyConfig_t config_high = { .sensor_interval_ms = 100, .ble_interval_ms = 100, .logger_interval_ms = 1000 };
static PowerPolicyConfig_t config_normal = { .sensor_interval_ms = 1000, .ble_interval_ms = 1000, .logger_interval_ms = 5000 };
static PowerPolicyConfig_t config_low = { .sensor_interval_ms = 5000, .ble_interval_ms = 5000, .logger_interval_ms = 60000 };
static PowerPolicyConfig_t config_critical = { .sensor_interval_ms = 60000, .ble_interval_ms = 0xFFFFFFFF, .logger_interval_ms = 0xFFFFFFFF };

void PowerPolicy_Init(void)
{
  current_profile = POWER_PROFILE_NORMAL;
}

void PowerPolicy_Update(uint16_t vcap_mv)
{
  /* Simple hysteresis implementation */
  if (vcap_mv >= POWER_THRESHOLD_HIGH) {
    current_profile = POWER_PROFILE_HIGH;
  }
  else if (vcap_mv >= POWER_THRESHOLD_NORMAL) {
    if (current_profile != POWER_PROFILE_HIGH) {
      current_profile = POWER_PROFILE_NORMAL;
    }
  }
  else if (vcap_mv >= POWER_THRESHOLD_LOW) {
    if (current_profile != POWER_PROFILE_NORMAL && current_profile != POWER_PROFILE_HIGH) {
       current_profile = POWER_PROFILE_LOW;
    } else if (vcap_mv < (POWER_THRESHOLD_NORMAL - 100)) { // 100mV hysteresis
       current_profile = POWER_PROFILE_LOW;
    }
  }
  else if (vcap_mv >= POWER_THRESHOLD_CRITICAL) {
    if (vcap_mv < (POWER_THRESHOLD_LOW - 100)) {
       current_profile = POWER_PROFILE_CRITICAL;
    }
  }
  else {
    current_profile = POWER_PROFILE_CRITICAL;
  }
}

PowerProfile_t PowerPolicy_GetProfile(void)
{
  return current_profile;
}

PowerPolicyConfig_t PowerPolicy_GetConfig(void)
{
  switch (current_profile) {
    case POWER_PROFILE_HIGH:
      return config_high;
    case POWER_PROFILE_NORMAL:
      return config_normal;
    case POWER_PROFILE_LOW:
      return config_low;
    case POWER_PROFILE_CRITICAL:
    default:
      return config_critical;
  }
}
