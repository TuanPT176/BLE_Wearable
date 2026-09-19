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
#include <stdbool.h>
#include "smtc_modem_api.h"

/* ---- LoRaWAN ---------------------------------------------------------- */

#define LBM_STACK_ID                 0

/* AS923-2 (frequency plan chosen by the user for The Things Stack).
 * Build defines needed by the library: REGION_AS_923, RP2_103 (RP002-1.0.3). */
#define LBM_REGION                   SMTC_MODEM_REGION_AS_923_GRP2

/* Periodic application uplink: 4-byte big-endian counter on this port, unconfirmed. */
#define LBM_UPLINK_PORT              101
#define LBM_UPLINK_PERIOD_S          60u
#define LBM_FIRST_UPLINK_DELAY_S     10u

/* ---- Data rate / spreading factor -------------------------------------- *
 * AS923 data rates (RP002-1.0.3), 125 kHz unless noted:
 *   DR0 = SF12   DR1 = SF11   DR2 = SF10   DR3 = SF9   DR4 = SF8   DR5 = SF7
 *   DR6 = SF7 @ 250 kHz       DR7 = FSK 50 kbit/s
 * With the 400 ms uplink dwell time AS923 uses by default, DR0/DR1 are not
 * allowed, so DR2 (SF10) is the lowest usable rate; LBM masks the rest.
 * A higher DR = shorter airtime and less range. */
#define LBM_ADR_NETWORK_CONTROLLED   0 /* the network server picks DR and TX power (static node) */
#define LBM_ADR_MOBILE_LONG_RANGE    1 /* built-in profile favouring range, for a moving node */
#define LBM_ADR_MOBILE_LOW_POWER     2 /* built-in profile favouring airtime, for a moving node */
#define LBM_ADR_FIXED_DR             3 /* no ADR: always LBM_FIXED_DR */

#define LBM_ADR_MODE                 LBM_ADR_NETWORK_CONTROLLED

/* Used only when LBM_ADR_MODE is LBM_ADR_FIXED_DR. */
#define LBM_FIXED_DR                 2

/* Copies of each uplink sent (1..15). Ignored (the server decides) when
 * LBM_ADR_MODE is LBM_ADR_NETWORK_CONTROLLED. */
#define LBM_NB_TRANS                 1

/* Data rate of the JOIN requests only. -1 keeps LBM's default mix (DR2..DR5);
 * 0..7 forces that DR, e.g. 2 = always SF10. Applied before the join starts. */
#define LBM_JOIN_DR                  (-1)

/* ---- SOS button (PB5, active low, internal pull-up, EXTI falling edge) ---- *
 * A press sends one emergency uplink: high priority and exempt from the duty
 * cycle. Payload = { 0x01, press counter as 4 bytes big-endian }. A press made
 * before the join finishes is sent right after the join. */
#define LBM_SOS_PORT                 102
#define LBM_SOS_CONFIRMED            true /* ask the server for an ACK (LBM retransmits if none) */
#define LBM_SOS_DEBOUNCE_MS          300u

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
