/**
 ******************************************************************************
 * @file    lbm_app.h
 * @brief   LoRaWAN application on top of LoRa Basics Modem: OTAA join on
 *          The Things Stack (AS923-2) and a periodic uplink.
 ******************************************************************************
 */
#ifndef LBM_APP_H
#define LBM_APP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 0 = LoRaWAN not built in: the LoRa CONTROL commands answer "not built",
 * the SX1262 stays held in reset, the SOS button does nothing.
 * 1 = LoRaWAN available, but nothing starts on its own: the join only starts
 * on the BLE command LoRa join (CONTROL 0x0F). */
#define LBM_APP_ENABLE 1

/* LBM_App_SendTestUplink() result when the device has not joined yet. */
#define LBM_APP_NOT_JOINED (-1)

typedef enum
{
  LBM_STATE_IDLE = 0,      /* LBM_App_Init() not called yet, or modem not reset yet */
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

/* SOS button (PB5) */
extern volatile uint32_t    g_lbmSosPressCount; /* presses accepted after debouncing */
extern volatile uint32_t    g_lbmSosSentCount;  /* SOS uplinks accepted by LBM */
extern volatile uint8_t     g_lbmSosPending;    /* 1 = a press is waiting (radio not joined yet, or LBM busy) */
extern volatile int32_t     g_lbmSosLastRc;     /* last smtc_modem_return_code_t of an SOS request */

/*
 * Lives in .noinit RAM, so it SURVIVES a software/debugger reset (unlike the
 * plain globals above, which restart from 0 on every boot). Use it to tell
 * "slow init" from "reset loop": boot_count climbing with init_stage stuck at 1
 * means the MCU keeps restarting inside smtc_modem_init(); panic_text then says
 * which LBM assertion fired. Cleared automatically after a power cycle.
 */
#define LBM_DIAG_MAGIC 0x4C424D44u /* "LBMD" */
typedef struct
{
  uint32_t magic;
  uint32_t boot_count;    /* LBM_App_Init() calls since power-up */
  uint32_t init_stage;    /* 1 = inside smtc_modem_init(), 2 = it returned */
  uint32_t panic_count;
  uint32_t panic_line;
  uint32_t radio_resets;  /* sx126x_hal_reset() calls since power-up */
  uint32_t busy_timeouts; /* radio BUSY stuck high past the 100 ms timeout */
  uint32_t spi_errors;    /* HAL_SPI_TransmitReceive() failures */
  uint8_t  first_read_valid;   /* set once the first radio read of THIS boot is captured */
  uint8_t  first_read_cmd[4];  /* its command bytes: 1D 02 9F 00 = ReadRegister(0x029F) */
  uint8_t  first_read_data[9]; /* what the chip answered: a healthy chip after reset gives 00 xx xx... */
  char     panic_text[96];
} LBM_Diag_t;
extern volatile LBM_Diag_t g_lbmDiag; /* defined in smtc_modem_hal_wb09.c */

/**
 * @brief Register the LBM sequencer task and initialise the modem. Call once,
 *        after MX_APPE_Init() (the sequencer must already be initialised).
 *        The join starts from the modem RESET event, run by the LBM task.
 */
void LBM_App_Init(void);

/**
 * @brief true once LBM_App_Init() has registered the LBM task. The SysTick and
 *        DIO1 handlers must not wake the task before that: the sequencer would
 *        call a task with no function registered.
 */
bool LBM_App_IsStarted(void);

/**
 * @brief Called after every batch of modem events (LBM task context), e.g. to
 *        report the new state over BLE. NULL removes it.
 */
void LBM_App_SetStatusCallback(void (*callback)(void));

/** @brief Cap on the SX1262 output power in dBm, read before every TX. */
void LBM_App_SetTxPowerMax(int8_t dbm);
int8_t LBM_App_GetTxPowerMax(void);

/**
 * @brief Starts LBM on the first call (the join follows the modem RESET event),
 *        joins again after LBM_App_Leave(). Task context only: the first call
 *        runs smtc_modem_init(). Returns a smtc_modem_return_code_t.
 */
int32_t LBM_App_Join(void);

/**
 * @brief One unconfirmed uplink on LBM_UPLINK_PORT (4-byte counter).
 *        Returns LBM_APP_NOT_JOINED, or a smtc_modem_return_code_t.
 */
int32_t LBM_App_SendTestUplink(void);

/**
 * @brief Stops the join attempts and the uplinks (smtc_modem_leave_network);
 *        the radio stays asleep until LBM_App_Join(). Returns a
 *        smtc_modem_return_code_t.
 */
int32_t LBM_App_Leave(void);

/**
 * @brief SOS button handler. Call from the PB5 EXTI callback (ISR context): it
 *        debounces the press and wakes the LBM task, which sends the SOS
 *        uplink (right after the join if the device is not joined yet).
 *        Does nothing until LBM_App_Init() has run.
 */
void LBM_App_SosButtonIrq(void);

#ifdef __cplusplus
}
#endif

#endif /* LBM_APP_H */
