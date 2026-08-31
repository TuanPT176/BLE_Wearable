#include "data_recovery_manager.h"
#include "nfc_log.h"
#include "wearable_data.h"
#include "../STM32_BLE/App/wearable.h"
#include "ble.h"

static RecoveryState_t current_state = RECOVERY_STATE_IDLE;
static uint16_t current_recovery_seq = 0;
static uint16_t latest_acked_seq = 0;
static uint32_t last_tx_time = 0; // Simple pacing mechanism
#define TX_DELAY_MS 50

void DataRecovery_Init(void)
{
    current_state = RECOVERY_STATE_IDLE;
    current_recovery_seq = 0;
    latest_acked_seq = 0;
}

bool DataRecovery_StartBLERecovery(uint16_t start_sequence)
{
    if (current_state != RECOVERY_STATE_IDLE) {
        return false;
    }
    
    // Check if start_sequence is within bounds, or just start from oldest
    if (start_sequence < nfc_log_header.oldest_sequence) {
        start_sequence = nfc_log_header.oldest_sequence;
    }
    if (start_sequence > nfc_log_header.newest_sequence) {
        return false; // Nothing to recover
    }
    
    current_recovery_seq = start_sequence;
    current_state = RECOVERY_STATE_BLE_ACTIVE;
    return true;
}

void DataRecovery_StopBLERecovery(void)
{
    current_state = RECOVERY_STATE_IDLE;
}

void DataRecovery_HandleACK(uint16_t acked_sequence)
{
    latest_acked_seq = acked_sequence;
}

void DataRecovery_Clear(void)
{
    NFC_Log_Clear();
    current_state = RECOVERY_STATE_IDLE;
}

RecoveryState_t DataRecovery_GetState(void)
{
    return current_state;
}

// Helper to calculate distance from oldest sequence
static uint16_t GetIndexForSequence(uint16_t sequence)
{
    if (sequence < nfc_log_header.oldest_sequence || sequence > nfc_log_header.newest_sequence) {
        return 0xFFFF; // Invalid
    }
    return sequence - nfc_log_header.oldest_sequence;
}

void DataRecovery_Process(void)
{
    if (current_state == RECOVERY_STATE_BLE_ACTIVE)
    {
        // Simple pacing: replace with HAL_GetTick() if HAL is available, assuming we have some tick mechanism.
        // For STM32, HAL_GetTick() is usually available.
        extern uint32_t HAL_GetTick(void);
        
        if (HAL_GetTick() - last_tx_time < TX_DELAY_MS) {
            return; // Wait
        }
        
        if (current_recovery_seq > nfc_log_header.newest_sequence) {
            // Done
            current_state = RECOVERY_STATE_IDLE;
            return;
        }
        
        uint16_t log_index = GetIndexForSequence(current_recovery_seq);
        if (log_index == 0xFFFF) {
            current_recovery_seq++;
            return;
        }
        
        NFC_SensorRecord_t record;
        if (NFC_Log_Read(log_index, &record))
        {
            wearable_recovery_packet_t packet;
            packet.sequence_number = record.sequence;
            packet.timestamp = record.timestamp;
            packet.crc = record.crc16;
            // copy sensor data payload
            WearableData_EncodeSensor(&record.sensor_data, packet.payload);
            
            uint8_t buffer[WEARABLE_RECOVERY_PAYLOAD_LENGTH];
            WearableData_EncodeRecovery(&packet, buffer);
            
            WEARABLE_Data_t ble_data;
            ble_data.p_Payload = buffer;
            ble_data.Length = WEARABLE_RECOVERY_PAYLOAD_LENGTH;
            
            // Assume ConnectionHandle is available or managed elsewhere. Typically 0x0001 or stored in context.
            // Using a dummy handle 0x0001 for now. Real implementation should track connection handle.
            // In ST stack, connection handle is passed to notify function.
            tBleStatus ret = WEARABLE_NotifyValue(WEARABLE_RECOVERY_DATA, &ble_data, 0x0001); 
            if (ret == BLE_STATUS_SUCCESS) {
                current_recovery_seq++;
                last_tx_time = HAL_GetTick();
            }
        }
        else
        {
             current_recovery_seq++; // Skip if read failed
        }
    }
}
