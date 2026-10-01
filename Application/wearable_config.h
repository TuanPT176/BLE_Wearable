/**
 ******************************************************************************
 * @file    wearable_config.h
 * @brief   Every build-time knob of the wearable firmware in one place:
 *          test/diagnostic switches, feature switches, timings, thresholds
 *          and board configuration.
 *
 * Rules:
 *  - Edit values here only. The modules include this file; none of them
 *    defines these macros itself.
 *  - Nothing in section 1 has any effect unless ENABLE_TEST = 1. With
 *    ENABLE_TEST = 0 every test is compiled out and every diagnostic switch
 *    is forced to its production value.
 *  - Not here on purpose: register addresses and chip constants (they are
 *    facts, not choices), the BLE/NFC wire format (the phone app depends on
 *    it), STM32CubeMX settings (Core/Inc/app_conf.h) and the LoRaWAN keys
 *    (Application/LoRaWAN/lbm_credentials.h, never committed with real keys).
 ******************************************************************************
 */
#ifndef WEARABLE_CONFIG_H
#define WEARABLE_CONFIG_H

/* ========================================================================== */
/* 1. Tests and diagnostics                                                   */
/* ========================================================================== */

/* Master switch. 0 = production build: every test below is off, whatever
 * the values in the ENABLE_TEST block say. 1 = the ENABLE_TEST block below
 * decides which tests/diagnostics are built in. */
#define ENABLE_TEST 0

#if ENABLE_TEST
/* ---- Edit these (only used when ENABLE_TEST = 1) ------------------------- */

/* LoRaTest (Application/LoRaTest): raw SX1262 bring-up + LORA_TX_REPEAT_COUNT
 * test packets at boot, before BLE. Drives the SX1262 directly, so it needs
 * LBM_APP_ENABLE = 0. Blocks boot for about count x period. */
#define LORA_TEST_ENABLE                0

/* ECG ~20 Hz disturbance hunt (Application/SensorManager/ecg_diag.h).
 * Change ONE switch per measurement; the values shown are production. */

/* 1 = write 0 to LED1_PA, LED2_PA and LED PILOT PA right after the ECG
 * configuration (cross-check: the soft reset already leaves them at 0). */
#define ECG_DIAG_PPG_OFF                0
/* ECG FIFO drain task period, ms (10-120). Production: 45. The real period
 * is this plus the drain time, because the timer is re-armed afterwards. */
#define ECG_DIAG_DRAIN_PERIOD_MS        45U
/* 1 = hand ECG_DATA packets to BLE on every drain (production). N (2-8) =
 * only every N-th drain; the FIFO is still drained every period. */
#define ECG_DIAG_NOTIFY_EVERY_N_DRAINS  1U
/* 0 = keep the central's connection interval (production). 8-500 = request
 * this interval in ms when ECG starts; the one actually used ends up in
 * g_ecgDiag.conn_interval_1p25. */
#define ECG_DIAG_CONN_INTERVAL_MS       0U
/* 1 = read back the MAX86150 ECG registers into g_ecgDiag.regs after the
 * ECG configuration (reads only). */
#define ECG_DIAG_DUMP_REGS              0

#else
/* ---- Production values: do not edit, use the block above ----------------- */
#define LORA_TEST_ENABLE                0
#define ECG_DIAG_PPG_OFF                0
#define ECG_DIAG_DRAIN_PERIOD_MS        45U
#define ECG_DIAG_NOTIFY_EVERY_N_DRAINS  1U
#define ECG_DIAG_CONN_INTERVAL_MS       0U
#define ECG_DIAG_DUMP_REGS              0
#endif /* ENABLE_TEST */

/* ---- LoRaTest radio settings (only read when LORA_TEST_ENABLE = 1) -------- */

/* 1 = TCXO on SX1262 DIO3 (E22 rig and the retrofitted custom PCB),
 * 0 = bare crystal on XTA/XTB. */
#define LORA_USE_TCXO                   1
/* TCXO supply: 0x00 = 1.6 V, 0x01 = 1.7 V, 0x02 = 1.8 V (worked on both boards). */
#define LORA_TCXO_VOLTAGE               0x02u
#define LORA_TCXO_TIMEOUT_MS            5u
/* SetRegulatorMode: 1 = DC-DC + LDO, 0 = LDO only (not verified on the PCB). */
#define LORA_USE_DCDC                   1
/* 1 = DIO2 drives the antenna TX/RX switch (E22 module wiring). */
#define LORA_USE_DIO2_RF_SWITCH         1
#define LORA_XTAL_HZ                    32000000UL
/* Carrier frequency, Hz. Check it is legal in your region before TX. */
#define LORA_FREQ_HZ                    923000000UL
/* TX power, dBm (E22-900M22S is rated +22 dBm). */
#define LORA_OUTPUT_POWER_DBM           22
#define LORA_SPREADING_FACTOR           7
/* SX1262 register codes: bandwidth 0x04 = 125 kHz, coding rate 0x01 = 4/5. */
#define LORA_BANDWIDTH_REG              0x04u
#define LORA_CODING_RATE_REG            0x01u
#define LORA_TX_TIMEOUT_MS              3000u
/* Packets sent back to back before boot continues (1 = single shot). */
#define LORA_TX_REPEAT_COUNT            30u
#define LORA_TX_REPEAT_PERIOD_MS        1000u

/* ========================================================================== */
/* 2. Features                                                                */
/* ========================================================================== */

/* LoRaWAN (LoRa Basics Modem). 0 = not built in: the LoRa CONTROL commands
 * answer "not built", the SX1262 stays in reset, the SOS button does
 * nothing. 1 = built in, but nothing starts on its own: the join only starts
 * on the BLE command LoRa join (CONTROL 0x0F). */
#define LBM_APP_ENABLE                  1

#if LORA_TEST_ENABLE && LBM_APP_ENABLE
#error "LoRaTest and LBM both drive the SX1262 - set LBM_APP_ENABLE to 0 for LORA_TEST_ENABLE"
#endif

/* ========================================================================== */
/* 3. Power: NEH7100 PMIC, supercap monitor, power policy                     */
/* ========================================================================== */

/* NEH7100 register values written at boot (main.c, before the radio) and
 * re-checked by NEH7100_EnsureConfig(). Decoded against the datasheet:
 *   REG00 0x48: LVD 2.6 V, OVP 3.8 V
 *   REG01 0x67: LDO 2.4 V, LDO_CTRL = 1 (LDO stays on below LVD), USB 200 mA
 *   REG04 0x20: boost factor 2x-8x
 *   REG05 0x06: MPPT every 32 s */
#define NEH7100_REG00_EXPECTED          0x48U
#define NEH7100_REG01_EXPECTED          0x67U
#define NEH7100_REG04_EXPECTED          0x20U
#define NEH7100_REG05_EXPECTED          0x06U

/* NEH7100 harvester profiles picked by NEH7100_Dynamic_Optimization_Task()
 * (not called anywhere yet). */
#define CONFIG_FREQ_INDOOR              0x40  /* f_max = 512 kHz, f_min = 32 kHz */
#define CONFIG_BF_INDOOR                0x32  /* BF_max = 16x, BF_min = 8x */
#define CONFIG_FREQ_OUTDOOR             0x52  /* f_max = 1.024 MHz, f_min = 128 kHz */
#define CONFIG_BF_OUTDOOR               0x20  /* BF_max = 8x, BF_min = 2x */
/* Harvest current that switches profile, uA (hysteresis between the two). */
#define THRESHOLD_TO_OUTDOOR            1000.0f  /* go outdoor above 1 mA */
#define THRESHOLD_TO_INDOOR             500.0f   /* back indoor below 0.5 mA */

/* Supercap voltage divider on the ADC input: Vcap = Vadc x (TOP + BOTTOM) / BOTTOM. */
#define SUPERCAP_MONITOR_R_TOP_OHM      2000000UL
#define SUPERCAP_MONITOR_R_BOTTOM_OHM   1000000UL
/* Supercap full-scale voltage, mV (documentation only, not used by the code yet). */
#define SUPERCAP_MONITOR_MAX_MV         3800UL
/* Moving-average length of the supercap reading, samples. */
#define SUPERCAP_MONITOR_FILTER_SAMPLES 8U

/* Supercap voltage (mV) at or above which each power profile applies.
 * Temporary values; PowerPolicy_Update() is not called yet. */
#define POWER_THRESHOLD_HIGH            2800U
#define POWER_THRESHOLD_NORMAL          2500U
#define POWER_THRESHOLD_LOW             2100U
#define POWER_THRESHOLD_CRITICAL        1800U

/* ========================================================================== */
/* 4. BLE application                                                         */
/* ========================================================================== */

/* SENSOR_DATA notify / sensor processing period while measuring, ms. The
 * phone app and the protocol docs assume 1 s. */
#define WEARABLE_SENSOR_PERIOD_MS       1000U

/* ========================================================================== */
/* 5. Sensors (SensorManager)                                                 */
/* ========================================================================== */

/* ---- MAX30208 temperature ---- */
/* Wait before the first result poll after starting a conversion, ms. */
#define TEMPERATURE_FIRST_POLL_DELAY_MS     20U
/* Poll interval while the conversion is not ready, ms. */
#define TEMPERATURE_RETRY_DELAY_MS          5U
/* Give up on a conversion after this long (error 0x11 on DEVICE_STATUS), ms. */
#define TEMPERATURE_CONVERSION_TIMEOUT_MS   60U
/* Re-probe a missing MAX30208 every N sensor periods (~N s). */
#define TEMPERATURE_REPROBE_INTERVAL_CALLS  5U

/* ---- LIS2DUXS12TR accelerometer / QVar ---- */
/* Re-probe a missing or failed LIS2DUXS12TR every N sensor periods. */
#define MOTION_REPROBE_INTERVAL_CALLS       5U

/* QVar wear detector: peak-to-peak of the raw QVar over a window decides
 * WEARABLE_FLAG_WEAR_DETECTED (0x40). First-guess values, not calibrated:
 * capture g_qvarDiag worn and not worn, then retune. */
#define QVAR_WINDOW_SAMPLES                 100U  /* window length, ~1 s at the 100 Hz ODR */
#define QVAR_WORN_ENTER_P2P                 600U  /* raw LSB: at or above -> worn */
#define QVAR_WORN_EXIT_P2P                  300U  /* raw LSB: at or below -> not worn */
#define QVAR_DEBOUNCE_WINDOWS               2U    /* consecutive windows needed to switch */

/* ---- MAX86150 PPG (heart rate / SpO2) ---- */
/* Re-probe / reconfigure a missing MAX86150 every N sensor periods. */
#define OPTICAL_REPROBE_INTERVAL_CALLS      2U
/* PPG FIFO drain period, ms. Must stay well under 320 ms (32 samples at 100 sps). */
#define OPTICAL_DRAIN_INTERVAL_MS           200U
/* Red and IR LED current code, 0.2 mA/LSB (0x24 = ~7 mA). Tune per enclosure. */
#define OPTICAL_DEFAULT_LED_CURRENT_CODE    0x24U
/* Heart-rate peak detector (heuristic, not clinical). */
#define OPTICAL_DC_ALPHA                    0.05f    /* baseline EMA factor */
#define OPTICAL_ENVELOPE_ALPHA              0.03f    /* pulse-amplitude envelope EMA factor */
#define OPTICAL_PULSE_SIGN                  (-1.0f)  /* flip if beats are not detected */
#define OPTICAL_THRESHOLD_HIGH_FRAC         0.5f     /* beat when the pulse rises above this x envelope */
#define OPTICAL_THRESHOLD_LOW_FRAC          0.25f    /* re-arm when the pulse falls below this x envelope */
#define OPTICAL_MIN_ENVELOPE                50.0f    /* raw counts; below = noise / no perfusion */
#define OPTICAL_MIN_DC_FOR_VALID            2000.0f  /* raw counts; below = sensor not on skin */
#define OPTICAL_MIN_BPM                     30U      /* beats outside this range are rejected */
#define OPTICAL_MAX_BPM                     220U
#define OPTICAL_BEAT_HISTORY_LEN            4U       /* HR = mean of the last N beat intervals */
#define OPTICAL_SPO2_WINDOW_DRAINS          5U       /* SpO2 recomputed every N drains (~1 s) */

/* ========================================================================== */
/* 6. LoRaWAN (LoRa Basics Modem, used when LBM_APP_ENABLE = 1)               */
/* ========================================================================== */

#define LBM_STACK_ID                    0

/* AS923-2 (frequency plan chosen for The Things Stack). Changing it also
 * needs the matching library build defines (now REGION_AS_923, RP2_103). */
#define LBM_REGION                      SMTC_MODEM_REGION_AS_923_GRP2

/* Application uplink: 4-byte big-endian counter on this port, unconfirmed.
 * LBM_UPLINK_PERIOD_S = 0: no uplink after the join and no periodic uplink;
 * the radio only transmits for the join, the BLE test command (CONTROL 0x10)
 * and the SOS button. Otherwise one uplink right after the join, the next
 * LBM_FIRST_UPLINK_DELAY_S later, then one every LBM_UPLINK_PERIOD_S. */
#define LBM_UPLINK_PORT                 101
#define LBM_UPLINK_PERIOD_S             0u
#define LBM_FIRST_UPLINK_DELAY_S        10u

/* Cap on the SX1262 output power, dBm (-9 to 22). AS923 asks for 14 dBm;
 * the cap wins when lower. About 89 mA peak at 14 dBm, 54 mA at 5 dBm,
 * 41 mA at 0 dBm (DC-DC). CONTROL 0x12 changes it until the next reset. */
#define LBM_TX_POWER_MAX_DBM            0

/* Data rate. AS923 (RP002-1.0.3), 125 kHz unless noted:
 *   DR0 = SF12  DR1 = SF11  DR2 = SF10  DR3 = SF9  DR4 = SF8  DR5 = SF7
 *   DR6 = SF7 @ 250 kHz     DR7 = FSK 50 kbit/s
 * With the default 400 ms dwell time DR0/DR1 are not allowed (LBM masks
 * them). Higher DR = shorter airtime, less range.
 * LBM_ADR_MODE options: */
#define LBM_ADR_NETWORK_CONTROLLED      0  /* network server picks DR and TX power (static node) */
#define LBM_ADR_MOBILE_LONG_RANGE       1  /* built-in profile favouring range (moving node) */
#define LBM_ADR_MOBILE_LOW_POWER        2  /* built-in profile favouring airtime (moving node) */
#define LBM_ADR_FIXED_DR                3  /* no ADR: always LBM_FIXED_DR */
#define LBM_ADR_MODE                    LBM_ADR_NETWORK_CONTROLLED
/* Used only when LBM_ADR_MODE is LBM_ADR_FIXED_DR. */
#define LBM_FIXED_DR                    2
/* Copies of each uplink (1-15). Ignored with LBM_ADR_NETWORK_CONTROLLED. */
#define LBM_NB_TRANS                    1
/* Data rate of the JOIN requests only: -1 = LBM's default mix (DR2-DR5),
 * 0-7 = force that DR (2 = always SF10). */
#define LBM_JOIN_DR                     (-1)

/* SOS button (PB5, active low): one emergency uplink per press, high
 * priority and exempt from the duty cycle. Payload = { 0x01, press counter
 * as 4 bytes big-endian }. A press before the join is sent after it. */
#define LBM_SOS_PORT                    102
#define LBM_SOS_CONFIRMED               true  /* ask the server for an ACK (LBM retransmits) */
#define LBM_SOS_DEBOUNCE_MS             300u

/* Radio front-end. TCXO on SX1262 DIO3: the custom PCB's bare crystal never
 * let the SX1262 boot, so a TCXO was retrofitted; 0x02 = 1.8 V. Set
 * LBM_RADIO_USE_TCXO to 0 for a plain crystal. */
#define LBM_RADIO_USE_TCXO              1
#define LBM_RADIO_TCXO_VOLTAGE_REG      0x02u
#define LBM_RADIO_TCXO_STARTUP_MS       5u
#define LBM_RADIO_TCXO_STARTUP_TICKS    (LBM_RADIO_TCXO_STARTUP_MS * 64u)  /* 15.625 us ticks */
/* Inherited from the E22-900M22S module, NOT verified on the custom PCB:
 * DC-DC needs the inductor fitted, DIO2-as-RF-switch needs DIO2 wired to
 * the antenna switch. */
#define LBM_RADIO_USE_DCDC              1
#define LBM_RADIO_USE_DIO2_RF_SWITCH    1
/* Added to the requested EIRP before the PA table lookup (antenna/board loss), dB. */
#define LBM_TX_POWER_OFFSET_DB          0

#endif /* WEARABLE_CONFIG_H */
