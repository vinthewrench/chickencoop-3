# Chicken Coop Controller

**Firmware and host-side tooling for an offline, solar-aware chicken coop controller based on the ATmega32U4.**

This repository contains:
- A **desktop host build** for testing and development
- An **AVR firmware build** for deployment
- A shared `src/` tree with no generated artifacts

The build system is intentionally explicit and boring:
- C++ only
- No generated files in `src/`
- All object files live under `build/`
- Host and firmware builds are independent

Hardware for this repository is frozen at **Chicken Coop Controller V3.0**.
Firmware and host code must conform to the shipped hardware.

---

## Repository Layout

```
.
├── Makefile
│   # Top-level build orchestrator
│   # Invokes host/ and firmware/ builds
│
├── README.md
│   # Project overview and build instructions
│
├── coop.cfg
│   # Example/default runtime configuration for host testing
│
├── firmware/
│   # AVR firmware for Chicken Coop Controller V3.0 (avr-g++)
│   ├── Makefile
│   ├── main_firmware.cpp
│   │   # Firmware entry point
│   │   # Deterministic, offline firmware for Hardware V3.0
│   │
│   ├── uart.cpp
│   │   # UART service (console/debug)
│   │
│   ├── rtc.cpp
│   │   # RTC service (PCF8523-backed)
│   │
│   ├── uptime.cpp
│   │   # System uptime service
│   │
│   └── platform/
│       # Board-specific AVR implementation (Hardware V3.0)
│       ├── config_eeprom.cpp
│       │   # Persistent configuration storage
│       ├── config_sw_avr.cpp
│       │   # Hardware config switch handling
│       ├── console_io_avr.cpp
│       │   # AVR-side console I/O
│       ├── door_avr.cpp
│       │   # Door motor control (real hardware)
│       ├── lock_avr.cpp
│       │   # Lock motor control (real hardware)
│       ├── relays_avr.cpp
│       │   # Relay pulse drivers
│       └── i2c_avr.cpp
│           # I2C implementation (RTC, peripherals)
│
├── host/
│   # Desktop host simulator and test console (clang++)
│   ├── Makefile
│   ├── main_host.cpp
│   │   # Host simulator entry point
│   │
│   └── platform/
│       # Host-side hardware simulation and glue
│       ├── config_host.cpp
│       ├── config_sw_host.cpp
│       ├── console_io_host.cpp
│       ├── door_hw_host.cpp
│       ├── door_host.cpp
│       ├── lock_hw_host.cpp
│       ├── lock_host.cpp
│       ├── rtc_host.cpp
│       ├── uptime_host.cpp
│       └── relay_host.cpp
│
└── src/
    # Shared, platform-independent logic
    # No main(), no generated files, no hardware assumptions
    ├── config.h
    ├── config_common.cpp
    ├── config_sw.h
    │
    ├── door.h
    ├── door.cpp
    │   # Shared door logic (used by host + firmware)
    ├── door_hw.h
    │
    ├── lock.h
    ├── lock.cpp
    │   # Shared lock logic (used by host + firmware)
    ├── lock_hw.h
    │
    ├── rtc.h
    ├── rtc_common.cpp
    │
    ├── solar.cpp
    ├── solar.h
    │
    ├── time_dst.cpp
    ├── time_dst.h
    │
    ├── uart.h
    ├── uptime.h
    │
    ├── scheduler.cpp
    ├── scheduler.h
    ├── scheduler_reconcile.cpp
    ├── scheduler_reconcile.h
    ├── next_event.cpp
    ├── next_event.h
    ├── events.h
    │
    ├── relay.h
    │
    ├── console/
    │   # Shared console implementation
    │   ├── console.cpp
    │   ├── console.h
    │   ├── console_cmds.cpp
    │   ├── console_time.cpp
    │   ├── console_time.h
    │   ├── console_io.h
    │   └── mini_printf.cpp
    │       └── mini_printf.h
    │
    └── devices/
        # Abstract device model layer
        ├── device.h
        ├── devices.h
        ├── devices.cpp
        ├── door_device.cpp
        ├── relay_device.cpp
        └── foo_device.cpp
```

### Rules
- `src/` must always remain clean
- No `.o`, `.elf`, `.hex`, or generated files under `src/`
- All build products live under `build/`
- Host and firmware builds do **not** share object files

---

## Top-Level Make Targets

Run all commands from the **repository root**.

```sh
make
make all
```
Builds **both** host and firmware (default).

```sh
make host
```
Builds **host console only**.

```sh
make firmware
```
Builds **AVR firmware only**.

```sh
make clean
```
Cleans host and firmware build outputs.

---

## Host Console (Desktop Test Environment)

### Purpose
The host build is a functional test harness for:
- Command parsing
- Solar and schedule calculations
- Configuration handling
- Console behavior
- Simulated RTC, EEPROM, relays, door, lock, and uptime

### Build
```sh
cd host
make
```

### Run
```sh
./build/console_host
```

### Exit
```
exit
```

The host build uses **clang++** and builds entirely under `host/build/`.

No source files live under `host/src/`. All shared logic comes from the top-level `src/` directory.

---

## Firmware (ATmega32U4)

### Build
```sh
cd firmware
make
```

### Outputs
```
coop_firmware.elf
coop_firmware.hex
coop_firmware.map
```

### Toolchain
- `avr-g++`
- Target MCU: **ATmega32U4**
- Clock: **Internal 8 MHz RC oscillator**

---

## Flashing (Atmel-ICE, ISP)

```sh
cd firmware
make flash
```

Assumptions:
- Atmel-ICE
- ISP mode
- USB transport
- No bootloader

---

## Fuse Management (LOCKED VALUES)

⚠️ **WARNING**  
Fuse settings are hardware configuration. Treat as write-once per board.

### Read fuses
```sh
cd firmware
make check-fuses
```

### Program fuses (ONLY on new or recovered chips)
```sh
cd firmware
make set-fuses
```

### Locked Fuse Values

| Fuse | Value | Meaning |
|-----:|:-----:|--------|
| Low  | 0xFF  | Internal 8 MHz RC, CLKDIV8 disabled |
| High | 0xD8  | JTAG disabled, reset enabled |
| Ext  | 0xF5  | Brown-out detect ≈ 4.3 V |

These values are fixed by the hardware design and should not be changed casually.

---

## Time, Solar, and DST Behavior

- RTC runs in **local civil time**
- `tz` is the **standard time offset** (e.g. `-6` for Central)
- Daylight Saving Time is handled via **US rules (post-2007)**
- DST logic lives in `src/time_dst.cpp`
- DST is applied at the **solar computation boundary**, not inside the solar math itself

This system is fully offline and deterministic.

---

## Version Injection

### Single source of truth
Defined in the **top-level Makefile**:

Note: `PROJECT_VERSION` refers to the **firmware/software version**, not the hardware revision.
Hardware for this repository is **Chicken Coop Controller V3.0**.

```make
PROJECT_VERSION := 2.6.x
```

### Injected into builds as
```c++
PROJECT_VERSION
```

### Used for
- Startup banner
- `version` console command
- Debug output

---

## Sanity Check Workflow

From the repo root:

```sh
make clean
make
make host
make firmware
```

If this completes without errors, the build system is stable.

---

## Final Notes

- GNU Make **3.81 compatible**
- Relative paths used intentionally
- No clever build magic
- If something breaks, it’s real code, not glue

This document is the **authoritative reference** for building, testing, and flashing the Chicken Coop Controller.
