/*
 * Bare-metal debug logging via USART1.
 * Writes directly to USART1 hardware registers - works without FreeRTOS scheduler.
 * Use DBG_LOG("message") for debug output before scheduler starts.
 */
#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include "device.h"
#include <stdarg.h>
#include <stdio.h>

/* Direct USART1 character output - blocks until TX ready, no buffering */
static inline void dbg_putc(char c)
{
    /* Wait for TXRDY bit (bit 1 of US_CSR) */
    while (!(USART1_REGS->US_CSR & US_CSR_USART_TXRDY_Msk)) { }
    USART1_REGS->US_THR = (uint32_t)c;
}

static inline void dbg_puts(const char *s)
{
    while (*s) {
        if (*s == '\n') dbg_putc('\r');
        dbg_putc(*s++);
    }
}

/* Print a hex value */
static inline void dbg_hex32(uint32_t val)
{
    const char hex[] = "0123456789ABCDEF";
    dbg_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        dbg_putc(hex[(val >> i) & 0xF]);
    }
}

/* Convenience macro: prints tag + newline */
#define DBG_LOG(msg) dbg_puts("[DBG] " msg "\n")
#define DBG_LOG_VAL(msg, val) do { dbg_puts("[DBG] " msg " "); dbg_hex32((uint32_t)(val)); dbg_puts("\n"); } while(0)

#endif /* DEBUG_LOG_H */
