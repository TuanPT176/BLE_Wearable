/*!
 * \file      sx126x_hal.c
 *
 * \brief     Implements the LoRa Basics Modem HAL for the STM32WB09
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
 * LoRa Basics Modem HAL for the STM32WB09 (bare metal + cooperative sequencer).
 *
 *  - Time base: HAL_GetTick() (SysTick, 1 ms). This only holds because
 *    LBM_App_Init() forbids Stop/Off mode (CFG_LPM_LBM) and
 *    PWR_EnterSleepMode() no longer calls HAL_SuspendTick(); with the tick
 *    suspended during sleep, uwTick loses time and every timer below stalls.
 *  - Modem timer: one software timer dispatched from LBM_HAL_TimerTick(), called
 *    by SysTick_Handler. The callback runs in ISR context and only sets flags.
 *  - Radio IRQ: DIO1 (PA1) EXTI, forwarded by LBM_HAL_RadioIrq().
 *  - Every LBM interruption wakes the LBM sequencer task through
 *    smtc_modem_hal_user_lbm_irq(); the task runs smtc_modem_run_engine().
 *  - Context store: RAM in the .noinit section - survives debugger/software
 *    resets but NOT a power cycle. Flash persistence is a follow-up.
 *
 * Milestone limits: no FUOTA, no Store&Forward, no Device Management, no trace.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "smtc_modem_hal.h"
#include "main.h"
#include "app_conf.h"
#include "stm32_seq.h"
#include "hw_rng.h"
#include "lbm_config.h"
#include "lbm_hal_wb09.h"

#ifndef MIN
#define MIN( a, b ) ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
#endif
#ifndef MAX
#define MAX( a, b ) ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
#endif

/* ---- Context slots kept in .noinit RAM ---------------------------------- */

#define LBM_CTX_MAGIC 0x4C424D31u /* "LBM1" */

#define LBM_CTX_MODEM_SIZE 32u
#define LBM_CTX_KEY_SIZE 32u
#define LBM_CTX_LORAWAN_SIZE 64u
#define LBM_CTX_SE_SIZE 512u

__attribute__( ( section( ".noinit" ) ) ) static uint32_t s_ctx_modem_magic;
__attribute__( ( section( ".noinit" ) ) ) static uint8_t  s_ctx_modem[LBM_CTX_MODEM_SIZE];
__attribute__( ( section( ".noinit" ) ) ) static uint32_t s_ctx_key_magic;
__attribute__( ( section( ".noinit" ) ) ) static uint8_t  s_ctx_key[LBM_CTX_KEY_SIZE];
__attribute__( ( section( ".noinit" ) ) ) static uint32_t s_ctx_lorawan_magic;
__attribute__( ( section( ".noinit" ) ) ) static uint8_t  s_ctx_lorawan[LBM_CTX_LORAWAN_SIZE];
__attribute__( ( section( ".noinit" ) ) ) static uint32_t s_ctx_se_magic;
__attribute__( ( section( ".noinit" ) ) ) static uint8_t  s_ctx_se[LBM_CTX_SE_SIZE];

__attribute__( ( section( ".noinit" ) ) ) static uint8_t          crashlog_buff_noinit[CRASH_LOG_SIZE];
__attribute__( ( section( ".noinit" ) ) ) static volatile uint8_t crashlog_length_noinit;
__attribute__( ( section( ".noinit" ) ) ) static volatile bool    crashlog_available_noinit;

typedef struct
{
    uint32_t* magic;
    uint8_t*  buf;
    uint32_t  size;
} lbm_ctx_slot_t;

static bool lbm_ctx_get_slot( modem_context_type_t type, lbm_ctx_slot_t* slot )
{
    switch( type )
    {
    case CONTEXT_MODEM:
        *slot = ( lbm_ctx_slot_t ){ &s_ctx_modem_magic, s_ctx_modem, LBM_CTX_MODEM_SIZE };
        return true;
    case CONTEXT_KEY_MODEM:
        *slot = ( lbm_ctx_slot_t ){ &s_ctx_key_magic, s_ctx_key, LBM_CTX_KEY_SIZE };
        return true;
    case CONTEXT_LORAWAN_STACK:
        *slot = ( lbm_ctx_slot_t ){ &s_ctx_lorawan_magic, s_ctx_lorawan, LBM_CTX_LORAWAN_SIZE };
        return true;
    case CONTEXT_SECURE_ELEMENT:
        *slot = ( lbm_ctx_slot_t ){ &s_ctx_se_magic, s_ctx_se, LBM_CTX_SE_SIZE };
        return true;
    default:
        return false; /* FUOTA / Store&Forward are not built */
    }
}

/* ---- Timer and IRQ state ------------------------------------------------- */

static struct
{
    volatile bool     active;
    volatile uint32_t deadline_ms;
    void ( *callback )( void* context );
    void* context;
} s_timer;

static volatile bool     s_modem_irq_masked;
static volatile bool     s_engine_wake_armed;
static volatile uint32_t s_engine_wake_deadline_ms;
static uint32_t          s_time_offset_ms;

static void ( *s_radio_callback )( void* context );
static void* s_radio_context;

volatile uint32_t g_lbmPanicLine;
volatile uint32_t g_lbmRadioIrqCount;  /* DIO1 events delivered by the EXTI interrupt */
volatile uint32_t g_lbmDio1PollEdges;  /* DIO1 rising edges seen by the 1 ms SysTick poll */
volatile uint8_t  g_lbmDio1Level;      /* DIO1 (PA1) level sampled by the poll */

static uint8_t s_dio1_prev_level;

/* ---- Reset / watchdog ---------------------------------------------------- */

void smtc_modem_hal_reset_mcu( void )
{
    NVIC_SystemReset( );
}

void smtc_modem_hal_reload_wdog( void )
{
    /* No watchdog configured */
}

/* ---- Time ---------------------------------------------------------------- */

uint32_t smtc_modem_hal_get_time_in_ms( void )
{
    return HAL_GetTick( ) + s_time_offset_ms;
}

uint32_t smtc_modem_hal_get_time_in_s( void )
{
    return smtc_modem_hal_get_time_in_ms( ) / 1000u;
}

void smtc_modem_hal_set_offset_to_test_wrapping( const uint32_t offset_to_test_wrapping )
{
    s_time_offset_ms = offset_to_test_wrapping;
}

/* ---- Timer --------------------------------------------------------------- */

void smtc_modem_hal_start_timer( const uint32_t milliseconds, void ( *callback )( void* context ), void* context )
{
    uint32_t primask = __get_PRIMASK( );
    __disable_irq( );
    s_timer.callback    = callback;
    s_timer.context     = context;
    s_timer.deadline_ms = HAL_GetTick( ) + milliseconds;
    s_timer.active      = true;
    __set_PRIMASK( primask );
}

void smtc_modem_hal_stop_timer( void )
{
    s_timer.active = false;
}

void LBM_HAL_TimerTick( void )
{
    uint32_t now = HAL_GetTick( );

    if( s_timer.active && !s_modem_irq_masked && ( ( int32_t ) ( now - s_timer.deadline_ms ) >= 0 ) )
    {
        void ( *callback )( void* ) = s_timer.callback;
        void* context               = s_timer.context;

        s_timer.active = false;
        if( callback != NULL )
        {
            callback( context );
        }
        smtc_modem_hal_user_lbm_irq( );
    }

    if( s_engine_wake_armed && ( ( int32_t ) ( now - s_engine_wake_deadline_ms ) >= 0 ) )
    {
        s_engine_wake_armed = false;
        smtc_modem_hal_user_lbm_irq( );
    }

    /* Safety net for the DIO1 EXTI: the SX1262 keeps DIO1 high until LBM clears the IRQ, so a
     * rising edge is still visible to a 1 ms poll. If the EXTI delivers the same edge the radio
     * callback runs twice, which only sets a flag (idempotent). It also tells apart "DIO1 never
     * reaches the MCU" (poll edges stay 0) from "EXTI path broken" (poll edges > 0, EXTI count 0). */
    uint8_t level = ( HAL_GPIO_ReadPin( DIO1_GPIO_Port, DIO1_Pin ) == GPIO_PIN_SET ) ? 1u : 0u;
    g_lbmDio1Level = level;
    if( !s_modem_irq_masked )
    {
        if( ( level != 0u ) && ( s_dio1_prev_level == 0u ) )
        {
            g_lbmDio1PollEdges++;
            if( s_radio_callback != NULL )
            {
                s_radio_callback( s_radio_context );
            }
            smtc_modem_hal_user_lbm_irq( );
        }
        s_dio1_prev_level = level;
    }
}

void LBM_HAL_ArmEngineWake( uint32_t ms )
{
    uint32_t primask = __get_PRIMASK( );
    __disable_irq( );
    s_engine_wake_deadline_ms = HAL_GetTick( ) + ms;
    s_engine_wake_armed       = true;
    __set_PRIMASK( primask );
}

/* ---- Modem IRQ mask ------------------------------------------------------ */

void smtc_modem_hal_disable_modem_irq( void )
{
    HAL_NVIC_DisableIRQ( GPIOA_IRQn ); /* DIO1 (PA1) is the only EXTI on port A */
    s_modem_irq_masked = true;
}

void smtc_modem_hal_enable_modem_irq( void )
{
    s_modem_irq_masked = false;
    HAL_NVIC_EnableIRQ( GPIOA_IRQn );
}

/* ---- Context saving ------------------------------------------------------ */

void smtc_modem_hal_context_restore( const modem_context_type_t ctx_type, uint32_t offset, uint8_t* buffer,
                                     const uint32_t size )
{
    lbm_ctx_slot_t slot;

    if( !lbm_ctx_get_slot( ctx_type, &slot ) || ( ( offset + size ) > slot.size ) )
    {
        memset( buffer, 0, size );
        return;
    }

    if( *slot.magic == LBM_CTX_MAGIC )
    {
        memcpy( buffer, &slot.buf[offset], size );
    }
    else
    {
        /* Never written since power-up: hand back zeros so LBM sees an invalid context and starts from defaults */
        memset( buffer, 0, size );
    }
}

void smtc_modem_hal_context_store( const modem_context_type_t ctx_type, uint32_t offset, const uint8_t* buffer,
                                   const uint32_t size )
{
    lbm_ctx_slot_t slot;

    if( !lbm_ctx_get_slot( ctx_type, &slot ) )
    {
        return;
    }
    if( ( offset + size ) > slot.size )
    {
        smtc_modem_hal_on_panic( ( uint8_t* ) __func__, __LINE__, "ctx %d too big (%lu)", ( int ) ctx_type,
                                 ( unsigned long ) ( offset + size ) );
        return;
    }
    if( *slot.magic != LBM_CTX_MAGIC )
    {
        memset( slot.buf, 0, slot.size );
    }
    memcpy( &slot.buf[offset], buffer, size );
    *slot.magic = LBM_CTX_MAGIC;
}

void smtc_modem_hal_context_flash_pages_erase( const modem_context_type_t ctx_type, uint32_t offset, uint8_t nb_page )
{
    /* Only used by Store and Forward, which is not built */
}

/* ---- Crashlog ------------------------------------------------------------ */

void smtc_modem_hal_crashlog_store( const uint8_t* crash_string, uint8_t crash_string_length )
{
    crashlog_length_noinit = MIN( crash_string_length, CRASH_LOG_SIZE );
    memcpy( crashlog_buff_noinit, crash_string, crashlog_length_noinit );
    crashlog_available_noinit = true;
}

void smtc_modem_hal_crashlog_restore( uint8_t* crash_string, uint8_t* crash_string_length )
{
    *crash_string_length = ( crashlog_length_noinit > CRASH_LOG_SIZE ) ? CRASH_LOG_SIZE : crashlog_length_noinit;
    memcpy( crash_string, crashlog_buff_noinit, *crash_string_length );
}

void smtc_modem_hal_crashlog_set_status( bool available )
{
    crashlog_available_noinit = available;
}

bool smtc_modem_hal_crashlog_get_status( void )
{
    return crashlog_available_noinit;
}

/* ---- Panic --------------------------------------------------------------- */

void smtc_modem_hal_on_panic( uint8_t* func, uint32_t line, const char* fmt, ... )
{
    uint8_t out_buff[CRASH_LOG_SIZE] = { 0 };
    int     out_len = snprintf( ( char* ) out_buff, sizeof( out_buff ), "%s:%lu ", ( const char* ) func,
                                ( unsigned long ) line );

    if( ( out_len > 0 ) && ( out_len < ( int ) sizeof( out_buff ) ) )
    {
        va_list args;
        va_start( args, fmt );
        int n = vsnprintf( ( char* ) &out_buff[out_len], sizeof( out_buff ) - ( size_t ) out_len, fmt, args );
        va_end( args );
        if( n > 0 )
        {
            out_len += n;
        }
    }
    if( out_len > ( int ) sizeof( out_buff ) )
    {
        out_len = ( int ) sizeof( out_buff );
    }

    g_lbmPanicLine = line;
    smtc_modem_hal_crashlog_store( out_buff, ( uint8_t ) out_len );
    smtc_modem_hal_reset_mcu( );
}

/* ---- Random -------------------------------------------------------------- */

uint32_t smtc_modem_hal_get_random_nb_in_range( const uint32_t val_1, const uint32_t val_2 )
{
    uint32_t lo = MIN( val_1, val_2 );
    uint32_t hi = MAX( val_1, val_2 );
    uint32_t r  = 0;

    if( HW_RNG_GetRandom32( &r ) != HW_RNG_SUCCESS )
    {
        /* BLE RNG not ready: fall back to a tick-seeded xorshift so LBM never blocks */
        static uint32_t x = 2463534242u;
        x ^= HAL_GetTick( ) + 0x9E3779B9u;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        r = x;
    }

    uint32_t span = hi - lo + 1u;
    if( span == 0u )
    {
        return r; /* full 32-bit range */
    }
    return lo + ( r % span );
}

/* ---- Radio environment --------------------------------------------------- */

void smtc_modem_hal_irq_config_radio_irq( void ( *callback )( void* context ), void* context )
{
    s_radio_callback = callback;
    s_radio_context  = context;
}

void LBM_HAL_RadioIrq( void )
{
    g_lbmRadioIrqCount++;
    if( s_radio_callback != NULL )
    {
        s_radio_callback( s_radio_context );
    }
    smtc_modem_hal_user_lbm_irq( );
}

bool smtc_modem_external_stack_currently_use_radio( void )
{
    return false;
}

void smtc_modem_hal_start_radio_tcxo( void )
{
    /* TCXO is powered by the SX1262 (DIO3) and started by the RAL BSP configuration */
}

void smtc_modem_hal_stop_radio_tcxo( void )
{
}

uint32_t smtc_modem_hal_get_radio_tcxo_startup_delay_ms( void )
{
#if LBM_RADIO_USE_TCXO
    return LBM_RADIO_TCXO_STARTUP_MS;
#else
    return 0;
#endif
}

void smtc_modem_hal_set_ant_switch( bool is_tx_on )
{
    /* Antenna switch is driven by DIO2 (see LBM_RADIO_USE_DIO2_RF_SWITCH) */
    ( void ) is_tx_on;
}

/* ---- Environment --------------------------------------------------------- */

uint8_t smtc_modem_hal_get_battery_level( void )
{
    return 255; /* unknown */
}

int8_t smtc_modem_hal_get_board_delay_ms( void )
{
    return 2;
}

int8_t smtc_modem_hal_get_temperature( void )
{
    return 25;
}

uint16_t smtc_modem_hal_get_voltage_mv( void )
{
    return 3300;
}

/* ---- Trace --------------------------------------------------------------- */

void smtc_modem_hal_print_trace( const char* fmt, ... )
{
    /* No trace sink on this board (PA1 is DIO1) */
    ( void ) fmt;
}

/* ---- Bare-metal wake-up -------------------------------------------------- */

void smtc_modem_hal_user_lbm_irq( void )
{
    UTIL_SEQ_SetTask( 1U << CFG_TASK_LBM_ID, CFG_SEQ_PRIO_0 );
}

/* --- EOF ------------------------------------------------------------------ */
