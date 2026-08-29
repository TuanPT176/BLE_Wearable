#ifndef NEH7100_H
#define NEH7100_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEH7100_I2C_ADDRESS             0x3CU
#define NEH7100_REGISTER_COUNT            11U

#define NEH7100_REG00_EXPECTED          0x48U
#define NEH7100_REG01_EXPECTED          0x67U
#define NEH7100_REG04_EXPECTED          0x20U
#define NEH7100_REG05_EXPECTED          0x06U

// Định nghĩa các giá trị cấu hình thanh ghi tương ứng với từng môi trường
#define CONFIG_FREQ_INDOOR   0x40 // f_max = 512kHz, f_min = 32kHz
#define CONFIG_BF_INDOOR     0x32 // BF_max = 16x, BF_min = 8x

#define CONFIG_FREQ_OUTDOOR  0x52 // f_max = 1.024MHz, f_min = 128kHz
#define CONFIG_BF_OUTDOOR    0x20 // BF_max = 8x, BF_min = 2x

// Định nghĩa ngưỡng chuyển đổi môi trường (đơn vị: Micro-Ampe - uA)
#define THRESHOLD_TO_OUTDOOR  1000.0f // Chuyển sang chế độ ngoài trời nếu dòng > 1000 uA (1 mA)
#define THRESHOLD_TO_INDOOR    500.0f // Chuyển về chế độ trong nhà nếu dòng < 500 uA (0.5 mA)

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
