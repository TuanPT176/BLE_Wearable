#ifndef DATA_RECOVERY_MANAGER_H
#define DATA_RECOVERY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    RECOVERY_STATE_IDLE,
    RECOVERY_STATE_BLE_ACTIVE,
    RECOVERY_STATE_NFC_ACTIVE
} RecoveryState_t;

void DataRecovery_Init(void);
void DataRecovery_Process(void);

bool DataRecovery_StartBLERecovery(uint16_t start_sequence);
void DataRecovery_StopBLERecovery(void);

void DataRecovery_HandleACK(uint16_t acked_sequence);
void DataRecovery_Clear(void);

RecoveryState_t DataRecovery_GetState(void);

#endif /* DATA_RECOVERY_MANAGER_H */
