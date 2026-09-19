/*!
 * \file      sx126x_hal.c
 *
 * \brief     Implements the sx126x radio HAL functions
 *
 * The Clear BSD License
 * Copyright Semtech Corporation 2021. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Semtech corporation nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 * NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * WB09 port of the SX126x radio driver HAL (derived from Semtech's example
 * lbm_examples/radio_hal/sx126x_hal.c): SPI3 in software-NSS mode, BUSY on
 * PB14, NRESET on PB15, NSS on PA9. Every SPI transaction waits for BUSY low
 * first; a stuck BUSY returns an error instead of hanging forever.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "sx126x_hal.h"
#include "main.h"

#define RADIO_BUSY_TIMEOUT_MS 1000u
#define RADIO_SPI_TIMEOUT_MS  1000u
#define RADIO_SPI_CHUNK       128u

extern SPI_HandleTypeDef hspi3;

typedef enum
{
    RADIO_SLEEP,
    RADIO_AWAKE
} radio_sleep_mode_t;

static radio_sleep_mode_t radio_mode = RADIO_AWAKE;

static const uint8_t s_zeros[RADIO_SPI_CHUNK];

static bool sx126x_hal_wait_on_busy( void )
{
    uint32_t start = HAL_GetTick( );
    while( HAL_GPIO_ReadPin( BUSY_GPIO_Port, BUSY_Pin ) == GPIO_PIN_SET )
    {
        if( ( HAL_GetTick( ) - start ) > RADIO_BUSY_TIMEOUT_MS )
        {
            return false;
        }
    }
    return true;
}

static sx126x_hal_status_t sx126x_hal_check_device_ready( void )
{
    bool ok;

    if( radio_mode != RADIO_SLEEP )
    {
        ok = sx126x_hal_wait_on_busy( );
    }
    else
    {
        // BUSY is HIGH in sleep mode, a falling NSS wakes the device and BUSY drops once it is ready
        HAL_GPIO_WritePin( SX_NSS_GPIO_Port, SX_NSS_Pin, GPIO_PIN_RESET );
        ok = sx126x_hal_wait_on_busy( );
        HAL_GPIO_WritePin( SX_NSS_GPIO_Port, SX_NSS_Pin, GPIO_PIN_SET );
        radio_mode = RADIO_AWAKE;
    }
    return ok ? SX126X_HAL_STATUS_OK : SX126X_HAL_STATUS_ERROR;
}

/* Clock `length` bytes out (rx discarded) */
static bool sx126x_hal_spi_write( const uint8_t* data, uint16_t length )
{
    uint8_t rx[RADIO_SPI_CHUNK];

    while( length > 0u )
    {
        uint16_t n = ( length > RADIO_SPI_CHUNK ) ? RADIO_SPI_CHUNK : length;
        if( HAL_SPI_TransmitReceive( &hspi3, ( uint8_t* ) data, rx, n, RADIO_SPI_TIMEOUT_MS ) != HAL_OK )
        {
            return false;
        }
        data += n;
        length -= n;
    }
    return true;
}

/* Clock `length` zero bytes out and store what comes back */
static bool sx126x_hal_spi_read( uint8_t* data, uint16_t length )
{
    while( length > 0u )
    {
        uint16_t n = ( length > RADIO_SPI_CHUNK ) ? RADIO_SPI_CHUNK : length;
        if( HAL_SPI_TransmitReceive( &hspi3, ( uint8_t* ) s_zeros, data, n, RADIO_SPI_TIMEOUT_MS ) != HAL_OK )
        {
            return false;
        }
        data += n;
        length -= n;
    }
    return true;
}

sx126x_hal_status_t sx126x_hal_write( const void* context, const uint8_t* command, const uint16_t command_length,
                                      const uint8_t* data, const uint16_t data_length )
{
    if( sx126x_hal_check_device_ready( ) != SX126X_HAL_STATUS_OK )
    {
        return SX126X_HAL_STATUS_ERROR;
    }

    HAL_GPIO_WritePin( SX_NSS_GPIO_Port, SX_NSS_Pin, GPIO_PIN_RESET );
    bool ok = sx126x_hal_spi_write( command, command_length );
    if( ok && ( data_length > 0u ) )
    {
        ok = sx126x_hal_spi_write( data, data_length );
    }
    HAL_GPIO_WritePin( SX_NSS_GPIO_Port, SX_NSS_Pin, GPIO_PIN_SET );

    if( !ok )
    {
        return SX126X_HAL_STATUS_ERROR;
    }

    // 0x84 - SX126x_SET_SLEEP opcode. In sleep mode BUSY is stuck at 1 => do not test it
    if( command[0] == 0x84 )
    {
        radio_mode = RADIO_SLEEP;
        return SX126X_HAL_STATUS_OK;
    }
    return sx126x_hal_check_device_ready( );
}

sx126x_hal_status_t sx126x_hal_read( const void* context, const uint8_t* command, const uint16_t command_length,
                                     uint8_t* data, const uint16_t data_length )
{
    if( sx126x_hal_check_device_ready( ) != SX126X_HAL_STATUS_OK )
    {
        return SX126X_HAL_STATUS_ERROR;
    }

    HAL_GPIO_WritePin( SX_NSS_GPIO_Port, SX_NSS_Pin, GPIO_PIN_RESET );
    bool ok = sx126x_hal_spi_write( command, command_length );
    if( ok && ( data_length > 0u ) )
    {
        ok = sx126x_hal_spi_read( data, data_length );
    }
    HAL_GPIO_WritePin( SX_NSS_GPIO_Port, SX_NSS_Pin, GPIO_PIN_SET );

    return ok ? SX126X_HAL_STATUS_OK : SX126X_HAL_STATUS_ERROR;
}

sx126x_hal_status_t sx126x_hal_reset( const void* context )
{
    HAL_GPIO_WritePin( SX_RESET_GPIO_Port, SX_RESET_Pin, GPIO_PIN_RESET );
    HAL_Delay( 5 );
    HAL_GPIO_WritePin( SX_RESET_GPIO_Port, SX_RESET_Pin, GPIO_PIN_SET );
    HAL_Delay( 5 );
    radio_mode = RADIO_AWAKE;
    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_wakeup( const void* context )
{
    return sx126x_hal_check_device_ready( );
}

/* --- EOF ------------------------------------------------------------------ */
