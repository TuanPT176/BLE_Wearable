#include "nfc_manager.h"
#include "nfc_io.h"
#include "nfc_config.h"
#include "nfc_log.h"
#include "../Drivers/ST25DV/st25dv.h"
#include <string.h>

static uint8_t mb_rx_buf[ST25DV_MAX_MAILBOX_LENGTH];
static uint8_t mb_tx_buf[ST25DV_MAX_MAILBOX_LENGTH];

static void NFC_Manager_HandleCommand(uint8_t *data, uint16_t length)
{
    if (length == 0) return;

    uint8_t cmd = data[0];
    uint16_t tx_len = 0;

    mb_tx_buf[0] = cmd; // echo command

    switch (cmd)
    {
        case NFC_CMD_GET_CONFIG:
            mb_tx_buf[1] = NFC_RESP_ACK;
            memcpy(&mb_tx_buf[2], &nfc_config, sizeof(NFC_Config_t));
            tx_len = 2 + sizeof(NFC_Config_t);
            break;

        case NFC_CMD_SET_CONFIG:
            if (length >= 1 + sizeof(NFC_Config_t))
            {
                memcpy(&nfc_config, &data[1], sizeof(NFC_Config_t));
                if (NFC_Config_Save())
                {
                    mb_tx_buf[1] = NFC_RESP_ACK;
                }
                else
                {
                    mb_tx_buf[1] = NFC_RESP_ERR;
                }
            }
            else
            {
                mb_tx_buf[1] = NFC_RESP_ERR;
            }
            tx_len = 2;
            break;

        case NFC_CMD_GET_LOG_INFO:
            mb_tx_buf[1] = NFC_RESP_ACK;
            memcpy(&mb_tx_buf[2], &nfc_log_header, sizeof(NFC_LogHeader_t));
            tx_len = 2 + sizeof(NFC_LogHeader_t);
            break;

        case NFC_CMD_GET_LOG:
            mb_tx_buf[1] = NFC_RESP_ACK;
            uint16_t count = NFC_Log_Count();
            mb_tx_buf[2] = count & 0xFF;
            mb_tx_buf[3] = (count >> 8) & 0xFF;
            tx_len = 4;
            
            // Note: FTM buffer is 256 bytes.
            // 256 - 4 = 252 bytes. 252 / 8 = 31 records max per packet.
            // For a full read, we might need a more complex protocol with chunking.
            // For now, we'll pack as many as possible (up to 31).
            uint16_t records_to_send = (count > 31) ? 31 : count;
            for (uint16_t i = 0; i < records_to_send; i++)
            {
                NFC_Log_Read(i, (NFC_SensorRecord_t*)&mb_tx_buf[tx_len]);
                tx_len += sizeof(NFC_SensorRecord_t);
            }
            break;

        case NFC_CMD_CLEAR_LOG:
            NFC_Log_Clear();
            mb_tx_buf[1] = NFC_RESP_ACK;
            tx_len = 2;
            break;

        default:
            mb_tx_buf[1] = NFC_RESP_ERR;
            tx_len = 2;
            break;
    }

    if (tx_len > 0)
    {
        ST25DV_WriteMailboxData(&st25dv_obj, mb_tx_buf, tx_len);
    }
}

void NFC_Manager_Init(void)
{
    // Initialize IO driver
    if (NFC_IO_Init() != 0) {
        return; // Initialization failed
    }

    // Enable Mailbox dynamically
    ST25DV_SetMBEN_Dyn(&st25dv_obj);

    // Initialize Sub-modules
    NFC_Config_Init();
    NFC_Log_Init();
}

void NFC_Manager_Process(void)
{
    ST25DV_MB_CTRL_DYN_STATUS mb_status;
    
    if (ST25DV_ReadMBCtrl_Dyn(&st25dv_obj, &mb_status) == 0)
    {
        if (mb_status.CurrentMsg == ST25DV_RF_MSG)
        {
            uint8_t mb_len = 0;
            if (ST25DV_ReadMBLength_Dyn(&st25dv_obj, &mb_len) == 0)
            {
                uint16_t length = mb_len + 1; // Length in register is actual length - 1
                if (ST25DV_ReadMailboxData(&st25dv_obj, mb_rx_buf, 0, length) == 0)
                {
                    NFC_Manager_HandleCommand(mb_rx_buf, length);
                }
            }
        }
    }
}
