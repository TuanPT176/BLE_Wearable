#ifndef WEARABLE_STATE_MANAGER_H
#define WEARABLE_STATE_MANAGER_H

typedef enum
{
  WEARABLE_STATE_IDLE = 0,
  WEARABLE_STATE_MEASURING,
  WEARABLE_STATE_ECG_ACTIVE,
  WEARABLE_STATE_LOW_POWER,
  WEARABLE_STATE_EMERGENCY,
  WEARABLE_STATE_ERROR
} wearable_state_t;

void WearableState_Init(void);
void WearableState_Set(wearable_state_t state);
wearable_state_t WearableState_Get(void);
int WearableState_AllowsPeriodicMeasurement(void);

#endif /* WEARABLE_STATE_MANAGER_H */
