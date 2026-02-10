# Chicken Coop Controller (v3.0)

**Date:** January 5, 2026  
**Author:** vinthewrench  
**Repository:** https://github.com/vinthewrench/chickencoop-3

**Sources of Truth:**

- KiCad schematic: chickencoop3.kicad_sch
- Locked MCU pin assignments (detailed below)

This document is for anyone who wants to build or understand this minimal, reliable, ultra-low-power chicken coop door controller — sunrise open, sunset close + lock, fully offline, no sensors/cloud/GPS.

---

## Purpose and Scope

Automates chicken coop door for off-grid/remote use.

Core goals:

- Fully offline — no connectivity
- One-time setup via serial console (date/time, lat/long, tz offset)
- Months of runtime on 7–12 Ah 12 V SLA/AGM battery
- Deterministic & inspectable (LED / multimeter / serial only)
- Minimal parts for reliability & lowest power

Fixed location, local standard time (no auto DST).

---

## How It Works (High-Level)

1. Initial Setup
   Connect serial console → set date/time/lat/long/tz → stored in EEPROM.

2. Normal Operation (RUN mode)
    - MCU sleeps >99% of time (power-down)
    - Wakes on RTC alarm or button press
    - Reads RTC → computes today's sunrise/sunset
    - Sunrise → open door
    - Sunset → close door + pulse lock
    - Set next alarm → sleep

3. Manual Override
   Press illuminated button → toggle open/closed (+ lock/unlock)

4. Status
   Button bi-color LED shows motion / locked / config-needed / fault

5. Power
   12 V direct to actuators; efficient 5 V regulator for logic.

Position from actuator limit switches + timeouts. No sensors.

Firmware: pure AVR C++, interrupt-driven, <32 KB flash.

---

## Major Components

| Component     | Part / Description                | Notes                                 |
| ------------- | --------------------------------- | ------------------------------------- |
| MCU           | ATmega1284P-PU (DIP-40)           | 128 KB flash, 16 KB SRAM, 4 KB EEPROM |
| RTC           | PCF8523T (SOIC-8)                 | I²C, alarm, CR2032 backup             |
| Crystal       | AB26T-32.768kHz                   | RTC                                   |
| Door Actuator | PA-14 linear (12 V)               | Internal limits                       |
| Lock Actuator | Generic 12 V automotive door lock | Pulsed only                           |
| Motor Drivers | 2 × VNH7100BASTR                  | H-bridge                              |
| 5 V Regulator | Pololu D24V22F5                   | ~1 mA quiescent, 5.3–36 V in          |
| Button/Status | FL12DRG5 (EOZ) pushbutton         | Bi-color LED, IP67                    |
| Darlington    | ULN2003A                          | Latching relay pulsing                |
| Battery       | 12 V SLA/AGM 7–12 Ah              | Motorcycle size                       |

---

## MCU Pin Assignments (ATmega1284P DIP-40)

### Power & Reference

| Function | Pin | Notes       |
| -------- | --- | ----------- |
| VCC      | 10  | +5 V        |
| AVCC     | 30  | +5 V        |
| GND      | 11  |             |
| GND      | 31  |             |
| AREF     | 33  | Unconnected |

### ISP Programming

| Signal | Pin   | Port  |
| ------ | ----- | ----- |
| MOSI   | 6     | PB5   |
| MISO   | 7     | PB6   |
| SCK    | 8     | PB7   |
| RESET  | 9     | RESET |
| VCC    | 10/30 |       |
| GND    | 11/31 |       |

### GPIO & Peripherals

| Function     | Port.Bit | Pin | Notes                         |
| ------------ | -------- | --- | ----------------------------- |
| LED_GREEN    | PA0      | 40  | Button green LED              |
| LED_RED      | PA1      | 39  | Button red LED                |
| LOCK_INA     | PA2      | 38  | Lock driver                   |
| LOCK_INB     | PA3      | 37  | Lock driver                   |
| LOCK_EN      | PA4      | 36  | Lock enable (pulse)           |
| DOOR_INA     | PA5      | 35  | Door driver                   |
| DOOR_INB     | PA6      | 34  | Door driver                   |
| DOOR_EN      | PA7      | 33  | Door enable                   |
| RTC_INT      | PD2      | 16  | INT0 – RTC alarm (active-low) |
| DOOR_SW      | PD3      | 17  | Manual button input           |
| RELAY1_RESET | PD4      | 18  | Latching relay pulse          |
| RELAY1_SET   | PD5      | 19  | Latching relay pulse          |
| RELAY2_SET   | PD6      | 20  | Latching relay pulse          |
| RELAY2_RESET | PD7      | 21  | Latching relay pulse          |
| CONFIG_SW    | PC6      | 28  | Ground = CONFIG mode          |

---

## MCU Fuse Configuration

**MCU:** ATmega1284P-PU (DIP-40)  
**Clock source:** Internal 8 MHz RC oscillator  
**Programming:** ISP only (via AVR-ISP-6) – no bootloader, no USB

Fuses are one-time critical settings that configure clock source, startup behavior, brown-out detection, EEPROM preservation, boot vector, and programming interfaces.  
Incorrect fuse settings can brick the chip (e.g., wrong clock → no ISP without external clock recovery).  
Always program fuses **before** flashing firmware on a new or erased chip.

### Verified Working Fuse Values (read from silicon)

Low Fuse (lfuse) = 0xE2 (binary: 11100010)

| Bit | Name     | Value | Meaning / Why chosen                      |
| --- | -------- | ----- | ----------------------------------------- |
| 7   | CKDIV8   | 1     | Clock not divided by 8 → full 8 MHz       |
| 6   | CKOUT    | 1     | Clock output disabled                     |
| 5:4 | SUT1:0   | 10    | Recommended startup delay for internal RC |
| 3:0 | CKSEL3:0 | 0010  | Internal 8 MHz calibrated RC oscillator   |

High Fuse (hfuse) = 0xD1 (recommended – preserves EEPROM)  
OR hfuse = 0xD9 (erases EEPROM on chip erase)

| Bit | Name      | Value (0xD1)    | Meaning / Why chosen                                   |
| --- | --------- | --------------- | ------------------------------------------------------ |
| 7   | RSTDISBL  | 1               | Reset pin enabled                                      |
| 6   | DWEN      | 1               | debugWIRE disabled                                     |
| 5   | SPIEN     | 0               | ISP enabled (must be 0)                                |
| 4   | WDTON     | 1               | Watchdog not always-on                                 |
| 3   | EESAVE    | 0 (D1) / 1 (D9) | EEPROM preserved (0xD1) vs erased (0xD9) on chip erase |
| 2:1 | BOOTSZ1:0 | 00              | Max boot size (unused – no bootloader)                 |
| 0   | BOOTRST   | 1               | Reset vector = application code                        |

Note on JTAG: Firmware disables JTAG at runtime via MCUCR JTD bit (not via fuse).

Extended Fuse (efuse) = 0xFB

| Bit | Name        | Value | Meaning / Why chosen                                           |
| --- | ----------- | ----- | -------------------------------------------------------------- |
| 2:0 | BODLEVEL2:0 | 111   | Brown-Out Detect disabled (best for slow battery voltage drop) |

Policy

- Fuse values are LOCKED — do not change unless clock source or hardware changes.
- Always use 0xD1 for hfuse unless you want EEPROM erased during development.
- JTAG disable handled in firmware.

Recommended Programming Command (Makefile snippet)

```set-fuses:
	$(AVRDUDE) -c $(PROGRAMMER) -P $(PORT) -p $(AVRDUDE_MCU) -B 200 \
		-U lfuse:w:0xE2:m \
		-U hfuse:w:0xD1:m \
		-U efuse:w:0xFB:m
```

## Sunrise/Sunset Computation

The controller calculates sunrise and sunset times completely offline using only the current date and the user-configured geographic location (latitude, longitude) and timezone offset. No external sensors, GPS, or internet are involved.

Core Algorithm
Port of simplified NOAA/NREL sunrise/sunset model.

Standard horizon correction: −0.833° (refraction + sun disk).

Key Mathematical Steps

1. Calculate day of year (from YYYY-MM-DD)

2. Solar declination (δ)  
   δ ≈ 23.45° × sin( (360° / 365) × (day_of_year + 284) )

3. Equation of time (EoT) — compact polynomial or table

4. Solar hour angle at sunrise/sunset (ω_s)  
   cos(ω_s) = [sin(−0.833°) − sin(φ) × sin(δ)] / [cos(φ) × cos(δ)]

5. Time from local solar noon  
   Sunrise ≈ 12:00 − (ω_s / 15) hours  
   Sunset ≈ 12:00 + (ω_s / 15) hours

6. Final adjustments
    - Longitude correction: longitude / 15 hours
    - Timezone offset (±HH)
    - Equation of time  
      → minutes past midnight local time

Accuracy & Edge Cases

- Typical: ±2–5 minutes
- Polar day/night: no valid event → skip
- No DST: fixed user offset
- Leap years: handled via day-of-year

AVR Implementation Notes

- Fixed-point math preferred (avoid float bloat)
- Execution: <10 ms @ 8 MHz
- Source: public-domain NOAA-style port

---

## Actuator Control Logic

Controls two 12 V actuators via VNH7100BASTR H-bridges.

Core Design Principles

- No external sensors — mechanical inference
- Lock never left energized
- Timeouts & default-off states
- Faults indicated via LED

### Driver Control Signals

| Pin | Function    | Idle/Default | Notes         |
| --- | ----------- | ------------ | ------------- |
| INA | Direction A | 0            |               |
| INB | Direction B | 0            |               |
| EN  | Enable      | 0            | High = active |

### Door Actuator (PA-14)

- Extend = open, retract = close
- Run until limit switch or timeout (~30 s max)
- Timeout → fault LED
- Optional brief reverse pulse after stop

### Lock Actuator

- 150–400 ms pulse (max 500 ms)
- Reverse polarity for lock/unlock
- Only after door fully closed

### General Safety

- All pins default 0 on boot/after use
- VNH7100 overcurrent protection
- Manual override bypasses timeouts

### Typical durations

- Door: 5–15 s
- Lock: 200–300 ms

## RTC Setup, Programming, and Wake/Sleep Strategy

PCF8523T is sole time authority — MCU uptime never used for scheduling.

### Hardware

- PCF8523T SOIC-8
- AB26T 32.768 kHz crystal (12.5 pF)
- CR2032 backup
- INT (pin 7) → PD2 / INT0 (active-low, ~10 kΩ pull-up)

### Critical Behaviors

- CLKOUT defaults ON (32.768 kHz) → must disable (COF=111)
- INT open-drain, latched low until AF cleared
- OS flag (seconds bit 7) cleared on time write
- CL[1:0] = 01 for 12.5 pF crystal

### Initialization

- Set 12.5 pF load
- Clear STOP
- Disable CLKOUT
- Clear stale AF

### Time Set

- STOP → freeze
- Verify frozen
- Burst write time/date
- Restart oscillator

### Alarm Set (hour:minute only)

- Safety check: refuse if already in matching minute
- Disable AIE → clear AF → write alarm regs → clear AF → enable AIE

## Sleep / Wake Strategy

### MCU: Power-Down mode (~0.1 µA)

**Wake sources**

- RTC INT → PD2 / INT0 (level-sensitive)
- Button → PD3

**Initialization**

- PD2 input, low-level trigger
- Enable INT0

**Enter Sleep**

- cli()
- Clear INTF0
- If PD2 already low → skip sleep
- set_sleep_mode(PWR_DOWN)
- sleep_enable()
- sei()
- sleep_cpu()

**On wake**

- sleep_disable()
- Handle event → set next alarm → sleep again

**Key Guard**
Prevents sleep lock if INT latched low before entering sleep.

Result: ~355 µA sleep current, months on battery.

## Power Architecture

The system is powered from a single 12 V sealed lead-acid (SLA) or gel battery, with direct 12 V paths to the actuators and an efficient switching regulator for the 5 V logic rail.

### Key Components

- Battery: 12 V sealed gel/AGM, motorcycle or UPS size (examples: Mighty Max ML15-12GEL or ML22-12GEL)
    - ML15-12GEL: 15 Ah capacity, dimensions ~6.00" × 3.93" × 3.88", F2 terminals
    - ML22-12GEL: 22 Ah capacity, dimensions ~7.13" × 3.01" × 6.57", internal thread terminals
    - Sealed, maintenance-free, deep-cycle capable, vibration-resistant
    - Self-discharge low (~3% per month at 20°C)
    - Charging: Use a standard 12 V SLA/gel charger (14.1–14.4 V float/absorption); never overcharge

- 5 V Regulator: Pololu D24V22F5 synchronous step-down (buck) converter
    - Input range: 5.3–36 V (perfect for 12 V battery with charging margin or sag)
    - Output: fixed 5 V, up to 2.5 A continuous (>2 A typical)
    - Typical efficiency: 85–95% (depending on VIN, load; much better than linear regulators)
    - Quiescent current (no load, enabled): ~1 mA typical (datasheet worst-case ~3 mA, but usually lower)
    - EN pin: left floating (internal pull-up keeps regulator always enabled)
    - Protections: reverse-voltage, over-current, thermal shutdown
    - No external enable/shutdown used — regulator stays on during sleep for RTC/INT wake
    - Input/output caps recommended: add 33–100 µF electrolytic + 0.1–10 µF ceramic on input for actuator spikes/long wires; 10–47 µF low-ESR on output

### Power Paths

- Direct 12 V: Battery → VNH7100BASTR drivers → actuators (door PA-14 ~1–2 A stall, lock pulse ~few A momentary)
- 5 V logic rail: Pololu → ATmega1284P, PCF8523T, LEDs, I²C pull-ups, button
- CR2032 backup: RTC only (lasts 5–10+ years at ~0.5 µA)

### Measured Power Profile (from prototype)

- Sleep (power-down, RTC armed): ~355 µA total system (MCU + RTC + regulator dominant)
- Idle/run (no actuators): ~3 mA
- Actuation peaks: short bursts (door 5–15 s @ 1–2 A, lock 200–300 ms pulse) — negligible daily energy

### Runtime Estimates

Assuming ~400 µA average sleep current (conservative, includes regulator ~1 mA typical but measured lower):

- On Mighty Max ML15-12GEL (15 Ah usable ~80% DoD for longevity → ~12 Ah effective):
    - Runtime: ~12 Ah / 0.0004 A = 30,000 hours ≈ 3.4 years theoretical
    - Realistic (self-discharge, temp effects, occasional actuation): 9–12 months safe

- On Mighty Max ML22-12GEL (22 Ah usable ~17–18 Ah effective):
    - Runtime: ~17 Ah / 0.0004 A ≈ 42,500 hours ≈ 4.8 years theoretical
    - Realistic: 16–24+ months

These are conservative — actual sleep current closer to 355 µA yields longer life. Actuations add very little (two per day, <0.1 Ah total daily).

### Why This Regulator?

- High efficiency vs linear (no heat sink needed)
- Low quiescent draw for always-on operation (RTC wake requires 5 V rail up)
- Built-in protections suit battery-powered outdoor use
- Small size (0.7" × 0.7"), easy mounting
- EN floating = always enabled (intentional — disabling would prevent RTC INT wake)

This power setup prioritizes months-long runtime with minimal parts, while keeping the system simple and inspectable.

## Quick Build / Replication Checklist

1. git clone https://github.com/vinthewrench/chickencoop-3
2. Review schematic & PCB in KiCad
3. Order parts
4. Program fuses (critical step)
5. Flash firmware via ISP
6. Serial console + ground CONFIG_SW → set values
7. Test actuators with open/close/lock/unlock commands
8. Remove config strap → verify auto sunrise/sunset

Good luck with your build.  
Open an issue on GitHub if you hit problems.
