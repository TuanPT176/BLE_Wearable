#include "wearable_state_manager.h"

static wearable_state_t current_state;

void WearableState_Init(void)
{
  current_state = WEARABLE_STATE_IDLE;
}

void WearableState_Set(wearable_state_t state)
{
  current_state = state;
}

wearable_state_t WearableState_Get(void)
{
  return current_state;
}

int WearableState_AllowsPeriodicMeasurement(void)
{
  return (current_state == WEARABLE_STATE_MEASURING) ||
         (current_state == WEARABLE_STATE_ECG_ACTIVE);
}
