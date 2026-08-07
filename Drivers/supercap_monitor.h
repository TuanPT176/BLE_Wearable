#ifndef SUPERCAP_MONITOR_H
#define SUPERCAP_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

#define SUPERCAP_MONITOR_R_TOP_OHM       2000000UL
#define SUPERCAP_MONITOR_R_BOTTOM_OHM    1000000UL
#define SUPERCAP_MONITOR_MAX_MV             3800UL
#define SUPERCAP_MONITOR_FILTER_SAMPLES          8U

bool SupercapMonitor_Init(void);
uint16_t SupercapMonitor_ReadMillivolts(void);

#endif /* SUPERCAP_MONITOR_H */
