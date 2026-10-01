/**
 ******************************************************************************
 * @file    lbm_config.h
 * @brief   LoRa Basics Modem (LBM) port configuration: pulls the settings
 *          from wearable_config.h and the keys from lbm_credentials.h.
 ******************************************************************************
 */
#ifndef LBM_CONFIG_H
#define LBM_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "smtc_modem_api.h"

/* Region, uplink schedule, data rate, SOS and radio front-end settings:
 * section 6 of wearable_config.h. */
#include "../wearable_config.h"

#include "lbm_credentials.h"

#endif /* LBM_CONFIG_H */
