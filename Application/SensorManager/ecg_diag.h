#ifndef ECG_DIAG_H
#define ECG_DIAG_H

/*
 * Compile-time switches for isolating the ~20 Hz disturbance on the ECG
 * stream. Every default below reproduces the production behaviour exactly;
 * change ONE switch per measurement, rebuild, flash, record a log.
 *
 *   ECG_DIAG_PPG_OFF               PPG LED drive
 *   ECG_DIAG_DRAIN_PERIOD_MS       FIFO drain (I2C burst) period
 *   ECG_DIAG_NOTIFY_EVERY_N_DRAINS when ECG_DATA packets are handed to BLE
 *   ECG_DIAG_CONN_INTERVAL_MS      BLE connection interval (radio events)
 *   ECG_DIAG_DUMP_REGS             read-only register dump, changes nothing
 */

/* 1 = right after the ECG configuration, write 0 to LED1_PA (0x11),
 * LED2_PA (0x12) and LED PILOT PA (0x15). Production (0) relies on the soft
 * reset inside MAX86150_EcgConfigure(): per the datasheet all three reset to
 * 0x00 and FD1 = ECG alone drives no LED, so this is only a cross-check. */
#define ECG_DIAG_PPG_OFF                0

/* Period of the ECG FIFO drain task, in ms. Production: 45. The timer is
 * re-armed at the end of each drain, so the real period is this value plus
 * the drain time (I2C at 100 kHz + notify, a few ms). Samples per ECG_DATA
 * packet stay 9, so packets follow the data, not this period. */
#define ECG_DIAG_DRAIN_PERIOD_MS        45U

/* 1 = hand queued ECG_DATA packets to the BLE stack on every drain
 * (production). N > 1 = only on every N-th drain, while the FIFO is still
 * drained every ECG_DIAG_DRAIN_PERIOD_MS: the I2C timing is unchanged, only
 * the notify timing moves. */
#define ECG_DIAG_NOTIFY_EVERY_N_DRAINS  1U

/* 0 = keep whatever connection interval the central chose (production).
 * Otherwise request min = max = this interval (ms, rounded down to 1.25 ms
 * units) with an L2CAP connection parameter update when ECG starts. The
 * central may refuse or pick another value: the interval actually used ends
 * up in g_ecgDiag.conn_interval_1p25. */
#define ECG_DIAG_CONN_INTERVAL_MS       0U

/* 1 = after the ECG configuration, read back the MAX86150 registers listed
 * in SensorManager into g_ecgDiag.regs. Reads only; no register is written. */
#define ECG_DIAG_DUMP_REGS              1

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
} ecg_diag_info_t;

extern volatile ecg_diag_info_t g_ecgDiag;

#endif /* ECG_DIAG_H */
