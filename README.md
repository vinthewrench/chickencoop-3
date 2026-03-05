# Chicken Coop Controller 3.0

**Build your own automated, off-grid chicken coop door**

A minimal, ultra-reliable, battery-powered AVR controller that opens the door at sunrise, closes and locks it at sunset — completely offline, no sensors, no Wi-Fi, no cloud, no GPS.

Designed for a mobile pasture-raised coop that moves with cattle and sheep. Runs for months on a small 12 V battery (solar is only a top-up). Survives Arkansas mud, ice, dust, coyotes, and power failures.

Full 18-page design story, sunrise math, and lessons learned here:  
https://www.vinthewrench.com/<update this>

### Features

- True sunrise/sunset calculation using location + date (NO light sensor)
- Gravity-hung door with Progressive Automations PA-14 actuator
- Automatic locking bar (automotive-style actuator) to stop predators
- Manual override button with bi-color LED status
- Ultra-low sleep current (~355 µA) → months on a 7–22 Ah SLA/AGM battery
- One-time setup via serial console (no phone app or internet required)
- Two extra latching relays for electric fence or lights (zero holding current)
- Handles power failures, manual overrides, and reboots correctly
- Fully open-source (firmware + schematics + PCB)

### Licenses

- Firmware: MIT
- Hardware (schematics + PCB): CERN-OHL-P v2

You are free to study, modify, and build your own.

---

## How It Works

1. The DS3231M RTC wakes the ATmega1284P at the next event (sunrise or sunset).
2. Firmware calculates exact sunrise/sunset from latitude, longitude, and date.
3. If needed, it drives the door actuator and lock.
4. It sets the next alarm and goes back to deep sleep.

No drift, no false triggers from clouds, headlights, or moving the coop.

Manual button press overrides the schedule until the next natural event (sunrise/sunset). The system never fights you.

---

## Hardware

Custom 100 × 130 mm PCB (all through-hole friendly where possible).

### Major Components

| Component        | Part                          | Notes                                         |
| ---------------- | ----------------------------- | --------------------------------------------- |
| MCU              | ATmega1284P-PU (DIP-40)       | 128 KB flash, easy ISP, repairable            |
| RTC              | DS3231M                       | Integrated resonator, temperature compensated |
| Door Actuator    | Progressive Automations PA-14 | IP65, internal limits, stainless rod          |
| Lock Actuator    | 12 V automotive door lock     | Pulses only (200–300 ms)                      |
| Motor Drivers    | 2 × VNH7100BAS                | 15 A H-bridges with thermal copper pours      |
| 5 V Regulator    | Pololu D24V22F5               | High efficiency, low quiescent current        |
| Button           | Illuminated bi-color switch   | Red/Green status + override                   |
| Relays           | 2 × DSP1-L2-DC12V             | Latching (zero holding current)               |
| Relay Driver     | ULN2003A                      | Drives relays + optional loads                |
| Optional Console | SparkFun DEV-15096            | USB-to-serial for setup (removable)           |

Power: Single 12 V battery (SLA/AGM recommended). Direct 12 V to motors, regulated 5 V for logic.

---

## Firmware

Written in plain C for avr-gcc. No Arduino framework, no hidden magic.

Typical workflow:

```bash
cd firmware
make set-fuses
make
make flash          # or use your programmer
```

Full build system, clean targets, and EEPROM config handling included.

---

## Setup & Configuration

1. Flip the **CONFIG switch** to ON (this puts the controller into console mode).
2. Connect the optional SparkFun USB-to-serial module (or any 5 V TTL serial).
3. Open a terminal (38400 baud).
4. Set date/time, latitude, longitude, timezone offset, and schedule offsets.
5. Flip the **CONFIG switch** back to OFF when done.
6. Remove the module — the controller no longer needs it.

All settings stored in EEPROM. No re-configuration unless you move to a new location.

---

## Using the CONFIG Switch & Console

The physical **CONFIG switch** on the board toggles between normal low-power run mode and console configuration mode. When flipped to ON, the MCU stays awake, enables the serial console, and allows full setup/debug without sleeping. Flip back to OFF to resume automatic scheduling and deep sleep.

**Configuration steps:**

1. Flip CONFIG to ON.
2. Connect serial (38400 baud) and type commands.
3. Use `set` to configure, `event add` for schedules, `save` to commit.
4. Flip CONFIG to OFF.

**What you need to set (minimum for basic operation):**

- Date and time (`set date YYYY-MM-DD`, `set time HH:MM[:SS]`)
- Location (`set lat +/-DD.DDDD`, `set lon +/-DDD.DDDD`)
- Timezone (`set tz +/-HH`, `set dst on|off`)
- Events (`event add door on sunrise +0`, `event add door off sunset -15`, etc. for lock/relays)

**Key commands:**
| Command | What it does |
|----------------------------------|--------------|
| `help` | List all commands |
| `config` | Show current settings |
| `set date YYYY-MM-DD` | Set date |
| `set time HH:MM[:SS]` | Set time (local) |
| `set lat +/-DD.DDDD` | Set latitude |
| `set lon +/-DDD.DDDD` | Set longitude |
| `set tz +/-HH` | Set timezone offset |
| `set dst on\|off` | Enable/disable DST |
| `event add ...` | Add door open/close events (sunrise/sunset or fixed time) |
| `save` | Commit everything to EEPROM |
| `schedule` | Show today’s full schedule |
| `solar` | Show sunrise/sunset |
| `door open\|close\|toggle` | Manual door test |
| `lock engage\|release` | Manual lock test |

Type `help` for full list. Changes are RAM-only until `save`.

---

## Repository Contents

- /firmware/ — AVR source + Makefile
- /hardware/ — KiCad schematics + PCB layout

---

## Typical Daily Workflow (after initial setup)

The controller just works. You walk out once a day to collect eggs.

---

## Links & Resources

- Full article: https://www.vinthewrench.com/<update>
- Buy the exact actuator: Progressive Automations PA-14 (PA-01-12-56-N-12VDC model)

Questions? Open an issue or find me on X @vinthewrench.
