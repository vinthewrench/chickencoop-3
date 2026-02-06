/*
 * mini_printf.h
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


 /*
  * mini_printf()
  *
  * Lightweight, deterministic printf replacement for AVR targets.
  *
  * Design goals:
  *  - Small code size
  *  - No heap use
  *  - No floating point
  *  - No 64-bit formatting
  *  - No locale
  *  - Fully deterministic
  *
  * Supported format specifiers:
  *
  *   %s      const char *
  *   %c      char
  *   %u      unsigned int (16-bit on AVR)
  *   %d      int (16-bit on AVR)
  *   %L      int32_t interpreted as latitude/longitude e4
  *           (prints signed DD.DDDD)
  *   %%      literal %
  *
  * Optional features:
  *
  *   Width:  %5u   %02d
  *           - Numeric width supported
  *           - Zero padding supported with leading '0'
  *           - Padding applies only to %u and %d
  *
  * NOT supported:
  *
  *   - %lu, %ld, %llu, %lld
  *   - %f or any floating point
  *   - precision (.2)
  *   - left alignment (-)
  *   - hex (%x), octal (%o)
  *   - scientific notation
  *
  * Behavior notes:
  *
  *   - %u and %d consume unsigned int / int
  *     (on AVR these are 16-bit)
  *
  *   - Passing 32-bit values requires manual formatting.
  *     Example:
  *         uint32_t v;
  *         mini_printf("%u", (unsigned)v);   // lower 16 bits only
  *
  *   - If an unsupported specifier is encountered,
  *     a '?' character is printed.
  *
  * This is intentionally minimal. If you need more,
  * you are probably on the wrong platform.
  */

#pragma once

void mini_printf(const char *fmt, ...);
