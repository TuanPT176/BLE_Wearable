#include "device_time.h"
#include "main.h"

static bool is_synchronized = false;
static uint32_t base_unix_seconds = 0;
static uint16_t base_milliseconds = 0;
static uint32_t monotonic_reference_ms = 0;

bool DeviceTime_Init(void)
{
  is_synchronized = false;
  base_unix_seconds = 0;
  base_milliseconds = 0;
  monotonic_reference_ms = 0;
  return true;
}

bool DeviceTime_SetUnixTime(uint32_t unix_seconds, uint16_t milliseconds)
{
  if (milliseconds > 999)
  {
    return false;
  }

  base_unix_seconds = unix_seconds;
  base_milliseconds = milliseconds;
  monotonic_reference_ms = HAL_GetTick();
  is_synchronized = true;
  
  return true;
}

bool DeviceTime_IsSynchronized(void)
{
  return is_synchronized;
}

uint32_t DeviceTime_GetUnixSeconds(void)
{
  if (!is_synchronized)
  {
    return 0;
  }
  
  uint32_t now_ms = HAL_GetTick();
  uint32_t elapsed_ms = now_ms - monotonic_reference_ms; /* Handles rollover correctly */
  
  uint32_t total_ms = base_milliseconds + elapsed_ms;
  uint32_t elapsed_sec = total_ms / 1000U;
  
  return base_unix_seconds + elapsed_sec;
}

uint16_t DeviceTime_GetMilliseconds(void)
{
  if (!is_synchronized)
  {
    return 0;
  }
  
  uint32_t now_ms = HAL_GetTick();
  uint32_t elapsed_ms = now_ms - monotonic_reference_ms;
  
  uint32_t total_ms = base_milliseconds + elapsed_ms;
  return (uint16_t)(total_ms % 1000U);
}

uint64_t DeviceTime_GetUnixMilliseconds(void)
{
  if (!is_synchronized)
  {
    return 0;
  }
  
  uint32_t now_ms = HAL_GetTick();
  uint32_t elapsed_ms = now_ms - monotonic_reference_ms;
  
  uint64_t total_ms = (uint64_t)base_unix_seconds * 1000ULL + base_milliseconds + elapsed_ms;
  return total_ms;
}

void DeviceTime_Process(void)
{
  /* 
   * Empty implementation.
   * Deriving time on the fly works for up to 49.7 days of uninterrupted operation 
   * since the last sync, due to HAL_GetTick() wrap-around handling.
   * A 32-bit delta calculation `now - reference` will automatically handle rollover 
   * correctly if called within the 49.7 days window.
   */
}
