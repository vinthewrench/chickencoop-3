# Chicken Coop Controller (v3.0)

**Author:** vinthewrench  
**Repository:** https://github.com/vinthewrench/chickencoop-3

This project is a minimal, reliable, ultra-low-power controller for a chicken coop door.  
It opens at sunrise, closes and locks at sunset, runs completely offline, and requires no sensors, cloud services, or GPS.

Like all my designs, the complete project is available here including firmware, schematics, and PCB layout.

- **Software License:** MIT
- **Hardware License:** CERN-OHL-P v2

You are free to study, modify, and build your own version.

Automate a chicken coop door for off-grid or remote installations.

Design goals:

- Fully offline operation
- One-time configuration via serial console
- Months of runtime on a small 12-volt battery
- Deterministic and inspectable system behavior
- Minimal hardware for reliability

The controller is designed for a **fixed geographic location** using **local standard time (no automatic DST).**

 
# How It Works

### Initial Setup

Connect a serial console and configure:

- Date and time
- Latitude and longitude
- Time zone offset
- Schedule offsets

Configuration is stored in EEPROM.
 
### Normal Operation

The MCU spends most of its time in deep sleep.

1. RTC alarm wakes the MCU
2. Current time is read
3. Sunrise / sunset are calculated
4. If required, the door actuator runs
5. Next alarm is programmed
6. MCU returns to sleep

Typical operation:

- Sunrise → open door  
- Sunset → close door + pulse lock  
 
### Manual Override

Pressing the illuminated button toggles the door state:

- open
- close
- lock/unlock
 
### Status

The button’s bi-color LED indicates:

- motion
- locked state
- configuration required
- fault conditions

 
# Major Components

| Component | Part | Notes |
|---|---|---|
| MCU | ATmega1284P-PU (DIP-40) | 128 KB flash |
| RTC | DS3231M | I²C RTC with integrated resonator |
| Door Actuator | PA-14 linear actuator | Internal limit switches |
| Lock Actuator | 12-V automotive door lock | Pulsed only |
| Motor Drivers | 2 × VNH7100BASTR | H-bridge |
| 5V Regulator | Pololu D24V22F5 | Efficient buck converter |
| Button | FL12DRG5 | Bi-color illuminated switch |
| Relay Driver | ULN2003A | Drives latching relays |
| Relays | DSP1-L2-DC12V | 12-V latching relays |
| Battery | 12V SLA / AGM | 7–22 Ah |

 
# MCU

**ATmega1284P-PU**

Chosen for:

- large flash and RAM
- simple architecture
- predictable behavior
- easy ISP programming
- DIP package for repairability

Clock source: **internal 8 MHz RC oscillator**

Programming: **ISP**

 
# Real Time Clock

**DS3231M**

The controller uses the DS3231M as its sole time reference.

Unlike many RTC chips, the DS3231M integrates its own resonator, eliminating the need for an external 32.768 kHz watch crystal. This removes layout sensitivity and mechanical fragility from the design.

For equipment that lives outside and gets dragged around a pasture, robustness matters more than saving a few microamps.

# Sunrise / Sunset Computation

The controller computes sunrise and sunset locally using the configured latitude, longitude, and date.

The algorithm is a simplified NOAA / NREL model.

Typical accuracy:

**±2–5 minutes**

No network, GPS, or sensors are required.

# Actuator Control

Two **VNH7100BAS H-bridges** drive:

- the door actuator
- the locking mechanism

Control philosophy:

- no external sensors
- time-based actuation
- mechanical limits inside the actuator
- timeout protection

Lock pulses are typically **200–300 ms**.
 
 # Optional Outputs

Two **DSP1-L2-DC12V latching relays** are included for optional loads such as:

- electric fence
- coop lighting
- future accessories

Because they are latching relays they draw **no holding current**.

They are driven from the MCU through a **ULN2003A**.

# Power System

The system runs from a **single 12-volt battery**.

### Direct 12V Loads
- door actuator
- lock actuator

### Logic Supply
A **Pololu D24V22F5 buck regulator** produces 5 V for the MCU and logic.

Typical sleep current:

**~355 µA**

This allows **months of operation on a small SLA battery.**

# Wake / Sleep Strategy

The MCU operates almost entirely in **power-down sleep mode**.

Wake sources:

- RTC alarm interrupt
- button press

After servicing the event the MCU immediately returns to sleep.

