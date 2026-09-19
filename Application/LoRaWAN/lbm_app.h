/**
 ******************************************************************************
 * @file    lbm_app.h
 * @brief   LoRaWAN application on top of LoRa Basics Modem: OTAA join on
 *          The Things Stack (AS923-2) and a periodic uplink.
 ******************************************************************************
 */
#ifndef LBM_APP_H
#define LBM_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  LBM_STATE_IDLE = 0,       /* LBM_App_Init() not called yet, or modem not reset yet */
  LBM_STATE_NO_CREDENTIALS, /* lbm_credentials.h is still all zero: not joining */
  LBM_STATE_JOINING,
  LBM_STATE_JOINED,
  LBM_STATE_JOIN_FAILED,
  LBM_STATE_ERROR           /* an LBM API call returned an error, see g_lbmLastRc */
} LBM_State_t;

/*
 * There is no trace sink on this board (PA1 is DIO1, not a UART), so read the
 * state with Live Expressions in STM32CubeIDE:
 *   g_lbmState, g_lbmEventCount, g_lbmLastEvent, g_lbmLastRc, g_lbmUplinkCount,
 *   g_lbmTxDoneStatus (0=not sent, 1=sent, 2=confirmed), g_lbmDownlinkCount,
 *   g_lbmDownlinkPort, g_lbmDownlinkLen, g_lbmRadioIrqCount, g_lbmPanicLine.
 */
extern volatile LBM_State_t g_lbmState;
extern volatile uint32_t    g_lbmEventCount;
extern volatile uint8_t     g_lbmLastEvent;    /* smtc_modem_event_type_t of the last event */
extern volatile int32_t     g_lbmLastRc;       /* last smtc_modem_return_code_t that was not OK */
extern volatile uint32_t    g_lbmUplinkCount;
extern volatile uint8_t     g_lbmTxDoneStatus;
extern volatile uint32_t    g_lbmDownlinkCount;
extern volatile uint8_t     g_lbmDownlinkPort;
extern volatile uint8_t     g_lbmDownlinkLen;
extern volatile uint32_t    g_lbmRadioIrqCount; /* defined in smtc_modem_hal_wb09.c: DIO1 events via EXTI */
extern volatile uint32_t    g_lbmDio1PollEdges; /* DIO1 rising edges seen by the 1 ms poll */
extern volatile uint8_t     g_lbmDio1Level;     /* DIO1 (PA1) level sampled by the poll */
extern volatile uint32_t    g_lbmPanicLine;     /* defined in smtc_modem_hal_wb09.c */

/**
 * @brief Register the LBM sequencer task and initialise the modem. Call once,
 *        after MX_APPE_Init() (the sequencer must already be initialised).
 *        The join starts from the modem RESET event, run by the LBM task.
 */
void LBM_App_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* LBM_APP_H */
