#ifndef NEH7100_H
#define NEH7100_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEH7100_I2C_ADDRESS             0x3CU
#define NEH7100_REGISTER_COUNT            11U

/* NEH7100_REGxx_EXPECTED, the indoor/outdoor profiles and their switching
 * thresholds: wearable_config.h. */
#include "../wearable_config.h"

typedef enum {
    ENV_INDOOR,
    ENV_OUTDOOR
} EnvironmentState;

typedef enum
{
  NEH7100_FREQ_32K = 0,
  NEH7100_FREQ_64K,
  NEH7100_FREQ_128K,
  NEH7100_FREQ_256K,
  NEH7100_FREQ_512K,
  NEH7100_FREQ_1M
} neh7100_frequency_t;

bool NEH7100_Init(void);
bool NEH7100_ReadAll(uint8_t registers[NEH7100_REGISTER_COUNT]);
bool NEH7100_EnsureConfig(void);
bool NEH7100_SetFrequency(neh7100_frequency_t maximum,
                          neh7100_frequency_t minimum);
bool NEH7100_UpdateFrequency(uint16_t current_uA_x10);
uint16_t NEH7100_GetCurrent_uA_x10(void);
bool NEH7100_IsPresent(void);

float NEH7100_Get_Charging_Current(void);
void NEH7100_Set_Environment(EnvironmentState env);
void NEH7100_Dynamic_Optimization_Task(void);

#ifdef __cplusplus
}
#endif

#endif /* NEH7100_H */
