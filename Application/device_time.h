#ifndef APPLICATION_DEVICE_TIME_H_
#define APPLICATION_DEVICE_TIME_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize the device time module.
 *        The device time will be set to NOT SYNCHRONIZED initially.
 * @retval true if successful, false otherwise.
 */
bool DeviceTime_Init(void);

/**
 * @brief Set the device Unix time based on the synchronization payload.
 *        This establishes a new monotonic reference point.
 * @param unix_seconds Seconds since Unix epoch.
 * @param milliseconds Fractional seconds in milliseconds (0..999).
 * @retval true if successful, false if inputs are invalid.
 */
bool DeviceTime_SetUnixTime(uint32_t unix_seconds, uint16_t milliseconds);

/**
 * @brief Check if the device has been synchronized with the central at least once.
 * @retval true if synchronized, false otherwise.
 */
bool DeviceTime_IsSynchronized(void);

/**
 * @brief Get the current device wall-clock time in Unix seconds.
 * @retval Unix timestamp in seconds.
 */
uint32_t DeviceTime_GetUnixSeconds(void);

/**
 * @brief Get the current milliseconds component of the device wall-clock time.
 * @retval Milliseconds (0..999).
 */
uint16_t DeviceTime_GetMilliseconds(void);

/**
 * @brief Get the full device wall-clock time in Unix milliseconds.
 * @retval Unix timestamp in milliseconds.
 */
uint64_t DeviceTime_GetUnixMilliseconds(void);

/**
 * @brief Optional periodic processing function. 
 *        Can be called from the main loop to perform background tasks if needed.
 */
void DeviceTime_Process(void);

#endif /* APPLICATION_DEVICE_TIME_H_ */
