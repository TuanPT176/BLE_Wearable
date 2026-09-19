/**
 ******************************************************************************
 * @file    lbm_config.h
 * @brief   Build-time configuration of the LoRa Basics Modem (LBM) port for
 *          the STM32WB09 wearable: region, uplink schedule and radio front-end.
 ******************************************************************************
 */
#ifndef LBM_CONFIG_H
#define LBM_CONFIG_H

#include <stdint.h>
#include "smtc_modem_api.h"

/* ---- LoRaWAN ---------------------------------------------------------- */

#define LBM_STACK_ID                 0

/* AS923-2 (frequency plan chosen by the user for The Things Stack).
 * Build defines needed by the library: REGION_AS_923, RP2_103 (RP002-1.0.3). */
#define LBM_REGION                   SMTC_MODEM_REGION_AS_923_GRP2

/* Application uplink: 4-byte big-endian counter on this port. */
#define LBM_UPLINK_PORT              101
#define LBM_UPLINK_PERIOD_S          60u
#define LBM_FIRST_UPLINK_DELAY_S     10u

/* ---- Radio front-end -------------------------------------------------- */

/* TCXO fed from SX1262 DIO3 (SetDio3AsTcxoCtrl). The custom PCB's bare XTAL
 * never let the SX1262 finish booting, so a TCXO was retrofitted; 0x02 = 1.8 V
 * per the SX1262 datasheet - the value that worked on the Nucleo + E22 rig and
 * on the retrofitted board. Set LBM_RADIO_USE_TCXO to 0 for a plain crystal. */
#define LBM_RADIO_USE_TCXO           1
#define LBM_RADIO_TCXO_VOLTAGE_REG   0x02u
#define LBM_RADIO_TCXO_STARTUP_MS    5u
#define LBM_RADIO_TCXO_STARTUP_TICKS (LBM_RADIO_TCXO_STARTUP_MS * 64u) /* 15.625 us ticks */

/* Both are inherited from the E22-900M22S module and are NOT verified on the
 * custom PCB: DC-DC needs the inductor populated, DIO2-as-RF-switch needs DIO2
 * to actually drive the antenna switch. */
#define LBM_RADIO_USE_DCDC           1
#define LBM_RADIO_USE_DIO2_RF_SWITCH 1

/* Added to the requested EIRP before the PA table lookup (antenna/board loss). */
#define LBM_TX_POWER_OFFSET_DB       0

#include "lbm_credentials.h"

#endif /* LBM_CONFIG_H */
