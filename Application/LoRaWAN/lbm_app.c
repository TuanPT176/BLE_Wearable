/**
 ******************************************************************************
 * @file    lbm_app.c
 * @brief   LoRaWAN application on top of LoRa Basics Modem (see lbm_app.h).
 *
 * Flow (same shape as Semtech's main_periodical_uplink example):
 *   LBM_App_Init()          -> smtc_modem_init(), registers the LBM task
 *   LBM task                -> smtc_modem_run_engine(), re-armed by the HAL
 *   RESET event             -> region + credentials + smtc_modem_join_network()
 *   JOINED event            -> first uplink, start the periodic alarm
 *   ALARM event             -> uplink, restart the alarm
 *
 * The event callback runs inside smtc_modem_run_engine(), i.e. in task
 * context, never in an ISR.
 ******************************************************************************
 */

#include "lbm_app.h"

#include <stdbool.h>
#include <string.h>

#include "main.h"
#include "app_conf.h"
#include "stm32_seq.h"
#include "smtc_modem_api.h"
#include "smtc_modem_utilities.h"
#include "lbm_config.h"
#include "lbm_hal_wb09.h"

volatile LBM_State_t g_lbmState = LBM_STATE_IDLE;
volatile uint32_t    g_lbmEventCount;
volatile uint8_t     g_lbmLastEvent;
volatile int32_t     g_lbmLastRc;
volatile uint32_t    g_lbmUplinkCount;
volatile uint8_t     g_lbmTxDoneStatus;
volatile uint32_t    g_lbmDownlinkCount;
volatile uint8_t     g_lbmDownlinkPort;
volatile uint8_t     g_lbmDownlinkLen;

static const uint8_t s_devEui[SMTC_MODEM_EUI_LENGTH]  = LBM_DEV_EUI;
static const uint8_t s_joinEui[SMTC_MODEM_EUI_LENGTH] = LBM_JOIN_EUI;
static const uint8_t s_appKey[SMTC_MODEM_KEY_LENGTH]  = LBM_APP_KEY;

static bool s_credentialsMissing(void)
{
  static const uint8_t zeroEui[SMTC_MODEM_EUI_LENGTH];
  static const uint8_t zeroKey[SMTC_MODEM_KEY_LENGTH];

  return (memcmp(s_devEui, zeroEui, sizeof(zeroEui)) == 0) ||
         (memcmp(s_appKey, zeroKey, sizeof(zeroKey)) == 0);
}

/* Records the first failing return code; returns true when rc is OK. */
static bool s_check(smtc_modem_return_code_t rc)
{
  if (rc != SMTC_MODEM_RC_OK)
  {
    g_lbmLastRc = (int32_t)rc;
    g_lbmState = LBM_STATE_ERROR;
    return false;
  }
  return true;
}

static void s_sendUplink(void)
{
  uint32_t n = g_lbmUplinkCount;
  uint8_t payload[4] = {
    (uint8_t)(n >> 24), (uint8_t)(n >> 16), (uint8_t)(n >> 8), (uint8_t)n
  };

  if (s_check(smtc_modem_request_uplink(LBM_STACK_ID, LBM_UPLINK_PORT, false, payload, sizeof(payload))))
  {
    g_lbmUplinkCount++;
  }
}

static void s_onModemEvent(void)
{
  smtc_modem_event_t event;
  uint8_t pending = 0;

  do
  {
    if (!s_check(smtc_modem_get_event(&event, &pending)))
    {
      return;
    }

    g_lbmEventCount++;
    g_lbmLastEvent = (uint8_t)event.event_type;

    switch (event.event_type)
    {
      case SMTC_MODEM_EVENT_RESET:
        if (s_credentialsMissing())
        {
          g_lbmState = LBM_STATE_NO_CREDENTIALS;
          break;
        }
        if (s_check(smtc_modem_set_deveui(LBM_STACK_ID, s_devEui)) &&
            s_check(smtc_modem_set_joineui(LBM_STACK_ID, s_joinEui)) &&
            s_check(smtc_modem_set_appkey(LBM_STACK_ID, s_appKey)) &&
            s_check(smtc_modem_set_nwkkey(LBM_STACK_ID, s_appKey)) &&
            s_check(smtc_modem_set_region(LBM_STACK_ID, LBM_REGION)) &&
            s_check(smtc_modem_join_network(LBM_STACK_ID)))
        {
          g_lbmState = LBM_STATE_JOINING;
        }
        break;

      case SMTC_MODEM_EVENT_JOINED:
        g_lbmState = LBM_STATE_JOINED;
        s_sendUplink();
        (void)s_check(smtc_modem_alarm_start_timer(LBM_FIRST_UPLINK_DELAY_S));
        break;

      case SMTC_MODEM_EVENT_JOINFAIL:
        g_lbmState = LBM_STATE_JOIN_FAILED; /* LBM keeps retrying with its own back-off */
        break;

      case SMTC_MODEM_EVENT_ALARM:
        s_sendUplink();
        (void)s_check(smtc_modem_alarm_start_timer(LBM_UPLINK_PERIOD_S));
        break;

      case SMTC_MODEM_EVENT_TXDONE:
        g_lbmTxDoneStatus = (uint8_t)event.event_data.txdone.status;
        break;

      case SMTC_MODEM_EVENT_DOWNDATA:
      {
        uint8_t rxBuf[SMTC_MODEM_MAX_LORAWAN_PAYLOAD_LENGTH];
        uint8_t rxLen = 0;
        uint8_t remaining = 0;
        smtc_modem_dl_metadata_t meta;

        if (s_check(smtc_modem_get_downlink_data(rxBuf, &rxLen, &meta, &remaining)))
        {
          g_lbmDownlinkCount++;
          g_lbmDownlinkPort = meta.fport;
          g_lbmDownlinkLen = rxLen;
        }
        break;
      }

      default:
        break;
    }
  } while (pending > 0u);
}

static void LBM_Task(void)
{
  uint32_t sleepMs = smtc_modem_run_engine();

  if (smtc_modem_is_irq_flag_pending())
  {
    UTIL_SEQ_SetTask(1U << CFG_TASK_LBM_ID, CFG_SEQ_PRIO_0);
  }
  else
  {
    LBM_HAL_ArmEngineWake(sleepMs);
  }
}

void LBM_App_Init(void)
{
  UTIL_SEQ_RegTask(1U << CFG_TASK_LBM_ID, UTIL_SEQ_RFU, LBM_Task);
  smtc_modem_init(&s_onModemEvent);
  UTIL_SEQ_SetTask(1U << CFG_TASK_LBM_ID, CFG_SEQ_PRIO_0);
}
