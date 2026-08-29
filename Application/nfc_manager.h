#ifndef NFC_MANAGER_H
#define NFC_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define NFC_CMD_GET_CONFIG       0x01
#define NFC_CMD_SET_CONFIG       0x02
#define NFC_CMD_GET_STATUS       0x03
#define NFC_CMD_GET_LOG_INFO     0x10
#define NFC_CMD_GET_LOG          0x11
#define NFC_CMD_CLEAR_LOG        0x12

#define NFC_RESP_ACK             0x00
#define NFC_RESP_ERR             0xFF

void NFC_Manager_Init(void);
void NFC_Manager_Process(void);

#endif /* NFC_MANAGER_H */
