# Research Motor Tests 2025
FireSide PCB & Firmware and GroundSide Software

## Project Overview
This repository contains the support equipment and software developed for the 2025 research motor test campaign. The project includes both the **FireSide** embedded system with its PCB design (responsible for ignition, data sampling, and SD card logging) and the **GroundSide** software telemetry & command interface.

---

## Test Execution Hardware
The `pcb` directory contains the FireSide PCB schematics, design, and manufacturing Bill of Materials as a KiCAD 9.0 project. The FireSide system includes ignition control, data acquisition, and high speed logging built around a Nucleo32 STM32L412KB board. Key capabilities include:

- Upto 6 parallel >3 kHz channels logging using 12-bit ADC with a circular buffer
- Binary logging to SD card with onboard CSV conversion utility
- Reyax RYLR LoRa‑based command, debugging, and telemetry exchange
- MOSFET D4184 module‑based ignition control with safety interlocks
- Deterministic, debugger ready, and reliable state‑machine‑driven operation

This architecture aims to improve fault detection, reliability, and repeatability. The architecture is inspired by ECSS requirements on space operations software and commercial electronic components.

## Test Execution Software
The `src` directory contains the embedded & Python software developed for the test campaign.

### FireSide PCB Firmware (`src/FireSide/`)

| File | Description |
|------|-------------|
| **main.cpp** | Assembles the system state machine and runs the main execution loop. |
| **States.cpp** | Implements the BOOT, SAFE, ARM, LAUNCH, LOGGING, CONVERT & FAILURE states. Defines the state transition logic. |
| **DMADAQ.cpp** | Configures DMA‑based ADC sampling and SD card logging through the STM32 L4 HAL. |
| **Interfaces.hpp** | Defines shared HW interfaces for UART serials, IO pins and ADC hardware used across the FireSide firmware. Allows debugging through a USB Serial Monitor. |
| **ConvertLog.py** | Post‑processes FireSide binary log files into CSV format. Useful after testing to extract sensor data from large files if onboard conversion fails. |

### GroundSide (`src/GroundSide/`)

| File | Description |
|------|-------------|
| **GroundSide.py** | Sends commands and receives telemetry from the FireSide PCB via a Reyax RYLR LoRa chip through a USB-Serial module. Provides a traceable interface for executing a motor test. |

---

## References

1. [STM32 Nucleo‑32 Boards (MB1180) User Manual](
  https://www.st.com/resource/en/user_manual/um1956-stm32-nucleo32-boards-mb1180-stmicroelectronics.pdf
)
2. [STM32L4 HAL and Low‑Layer Drivers](
  https://www.st.com/resource/en/user_manual/um1884-description-of-stm32l4l4-hal-and-lowlayer-drivers-stmicroelectronics.pdf
)
3. [STM32L41x/42x/43x/44x/45x/46x Reference Manual](
  https://www.st.com/resource/en/reference_manual/rm0394-stm32l41xxx42xxx43xxx44xxx45xxx46xxx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
)
4. [Cortex‑M4 Programming Manual](
  https://www.st.com/resource/en/programming_manual/pm0214-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf
)
5. [STM32L412xx/422xB Device Errata](
  https://www.st.com/resource/en/errata_sheet/es0456-stm32l412xx422xb-device-errata-stmicroelectronics.pdf
)
6. [PlatformIO STM32 Platform Documentation](
  https://docs.platformio.org/en/latest/platforms/ststm32.html
)
7. [Reyax RYLR993 Transceiver Module](
  https://robu.in/product/reyax-rylr993_lite-868-915mhz-lorawan-transceiver-module-helium-compatible-lite-evaluation-boarddip-version/
)
8. [CP2102 USB to TTL Serial Converter Module](
  https://robu.in/product/cp-2102-6-pin/
)

---
