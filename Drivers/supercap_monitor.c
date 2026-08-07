#include "supercap_monitor.h"

#include "main.h"

static uint16_t sample_buffer[SUPERCAP_MONITOR_FILTER_SAMPLES];
static uint32_t sample_sum;
static uint8_t sample_index;
static uint8_t sample_count;
static bool initialized;

bool SupercapMonitor_Init(void)
{
  uint8_t index;

  for (index = 0U; index < SUPERCAP_MONITOR_FILTER_SAMPLES; index++)
  {
    sample_buffer[index] = 0U;
  }
  sample_sum = 0UL;
  sample_index = 0U;
  sample_count = 0U;
  initialized = (hadc1.Instance == ADC1);
  return initialized;
}

uint16_t SupercapMonitor_ReadMillivolts(void)
{
  uint32_t adc_raw;
  uint32_t adc_mv;
  uint32_t supercap_mv;

  if (!initialized)
  {
    return 0U;
  }

  if (HAL_ADC_Start(&hadc1) != HAL_OK)
  {
    return (sample_count == 0U) ? 0U : (uint16_t)(sample_sum / sample_count);
  }
  if (HAL_ADC_PollForConversion(&hadc1, 5U) != HAL_OK)
  {
    (void)HAL_ADC_Stop(&hadc1);
    return (sample_count == 0U) ? 0U : (uint16_t)(sample_sum / sample_count);
  }

  adc_raw = HAL_ADC_GetValue(&hadc1);
  (void)HAL_ADC_Stop(&hadc1);

  adc_mv = __HAL_ADC_CALC_DATA_TO_VOLTAGE(ADC_VIN_RANGE_3V6,
                                           adc_raw,
                                           ADC_DS_DATA_WIDTH_12_BIT);
  supercap_mv = (adc_mv *
                 (SUPERCAP_MONITOR_R_TOP_OHM + SUPERCAP_MONITOR_R_BOTTOM_OHM)) /
                SUPERCAP_MONITOR_R_BOTTOM_OHM;

  if (supercap_mv > UINT16_MAX)
  {
    supercap_mv = UINT16_MAX;
  }

  if (sample_count < SUPERCAP_MONITOR_FILTER_SAMPLES)
  {
    sample_count++;
  }
  else
  {
    sample_sum -= sample_buffer[sample_index];
  }

  sample_buffer[sample_index] = (uint16_t)supercap_mv;
  sample_sum += supercap_mv;
  sample_index++;
  if (sample_index >= SUPERCAP_MONITOR_FILTER_SAMPLES)
  {
    sample_index = 0U;
  }

  return (uint16_t)(sample_sum / sample_count);
}
