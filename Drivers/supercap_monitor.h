#ifndef SUPERCAP_MONITOR_H
#define SUPERCAP_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

/* Divider resistors and filter length: wearable_config.h. */
#include "../Application/wearable_config.h"

bool SupercapMonitor_Init(void);
uint16_t SupercapMonitor_ReadMillivolts(void);

#endif /* SUPERCAP_MONITOR_H */
