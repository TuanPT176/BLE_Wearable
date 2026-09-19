/**
 ******************************************************************************
 * @file    lbm_credentials.h
 * @brief   OTAA credentials for The Things Stack. LEFT EMPTY ON PURPOSE: fill
 *          in your own values. LBM_DEV_EUI/LBM_APP_KEY all-zero keeps the
 *          device from joining (LBM_App reports LBM_STATE_NO_CREDENTIALS).
 *
 * Do not commit real keys to a shared repository.
 * With a LoRaWAN 1.0.x device profile the TTN "AppKey" goes in LBM_APP_KEY
 * (it is loaded as the NwkKey). Byte order is the same as the hex string
 * shown by the console (MSB first).
 ******************************************************************************
 */
#ifndef LBM_CREDENTIALS_H
#define LBM_CREDENTIALS_H

#define LBM_DEV_EUI  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
#define LBM_JOIN_EUI { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
#define LBM_APP_KEY  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

#endif /* LBM_CREDENTIALS_H */
