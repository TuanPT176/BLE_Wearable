#ifndef ECG_DIAG_H
#define ECG_DIAG_H

/* The ECG_DIAG_* switches (ECG ~20 Hz disturbance hunt) live in
 * wearable_config.h and only take effect with ENABLE_TEST = 1. */
#include "../wearable_config.h"

#if (ECG_DIAG_DRAIN_PERIOD_MS < 10U) || (ECG_DIAG_DRAIN_PERIOD_MS > 120U)
#error "ECG_DIAG_DRAIN_PERIOD_MS: keep 10-120 ms (the 32-sample FIFO lasts 160 ms at 200 sps)"
#endif
#if (ECG_DIAG_NOTIFY_EVERY_N_DRAINS < 1U) || (ECG_DIAG_NOTIFY_EVERY_N_DRAINS > 8U)
#error "ECG_DIAG_NOTIFY_EVERY_N_DRAINS: 1-8 (the ECG packet queue holds 16 packets)"
#endif
#if (ECG_DIAG_CONN_INTERVAL_MS != 0U) && \
    ((ECG_DIAG_CONN_INTERVAL_MS < 8U) || (ECG_DIAG_CONN_INTERVAL_MS > 500U))
#error "ECG_DIAG_CONN_INTERVAL_MS: 0 (off) or 8-500 ms"
#endif

#include <stdint.h>

/*
 * Observations only (always built, never changes behaviour). The board has
 * no UART, so read this over SWD after an ECG session with STM32CubeProgrammer
 * in hot-plug mode (the core keeps running): g_ecgDiag's address and size
 * are in STM32CubeIDE/Debug/BLE_p2pServer_GATT.map.
 */
#define ECG_DIAG_MAGIC      0xEC6D1A60UL
#define ECG_DIAG_REG_COUNT  13U

typedef struct
{
  uint32_t magic;                   /* ECG_DIAG_MAGIC once WEARABLE_APP_Init ran */
  uint16_t conn_interval_1p25;      /* last interval from the stack, 1.25 ms units */
  uint16_t conn_updates;            /* connection update complete events */
  uint8_t conn_req_status;          /* 0xFF = not requested, else status of the diag request */
  uint8_t regs_valid;               /* 1 once ECG_DIAG_DUMP_REGS read the chip */
  uint8_t regs[ECG_DIAG_REG_COUNT][2]; /* [address, value]; 0xEE,0xEE = read failed */
  uint32_t ecg_sessions;            /* successful ECG_START since boot */
  uint32_t ecg_overflows;           /* FIFO rollovers (samples lost inside a packet stream) */
  uint32_t ecg_dropped_packets;     /* packets dropped from the full BLE queue */

  /* Drain timing of the current/last ECG session, reset at ECG start. Times
   * in microseconds from SysTick (the Cortex-M0+ has no DWT cycle counter).
   * "fire" = the drain timer callback. */
  uint32_t drains;                  /* timer-triggered drains */
  uint32_t period_us_min;           /* fire to next fire = real drain period */
  uint32_t period_us_max;
  uint32_t period_us_sum;
  uint32_t period_count;
  uint32_t fire_to_read_done_us_min; /* fire until the FIFO I2C read returned */
  uint32_t fire_to_read_done_us_max;
  uint32_t fire_to_read_done_us_sum;
  uint32_t read_us_min;             /* the FIFO I2C read alone */
  uint32_t read_us_max;
  uint32_t read_us_sum;
  uint32_t samples_sum;             /* samples returned by those drains */
  uint8_t samples_min;              /* per drain */
  uint8_t samples_max;
  uint8_t queue_max;                /* deepest ECG packet queue seen */
  uint8_t reserved;
  uint32_t tx_retry_drains;         /* extra drains triggered by the TX-pool event */
} ecg_diag_info_t;

extern volatile ecg_diag_info_t g_ecgDiag;

#endif /* ECG_DIAG_H */
