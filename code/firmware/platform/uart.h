/*
 * uart.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Source file
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No network dependencies
 *
 * Updated: 2025-12-29
 */

#pragma once
void uart_init(void);
int  uart_getc(void);
void uart_putc(char c);
