# FireSide PCB Changelog

All notable changes to this project will be documented in this file.

## [2.5] - Latest
### Added
- BJT-based status indicator circuit

### Changed
- Layout and routing optimised

### Generated
- Drill files and Gerbers
- ERC and DRC checks completed

---

## [2.4]
### Added
- LEDs to indicate ARM and LAUNCH
- 3.3V switch for RYLR998

### Changed
- RYLR998 moved to the back side of the board
- Manual length matching applied to SPI traces (removed tuning)
- Layout and routing optimised

### Generated
- Drill files and Gerbers
- ERC and DRC checks completed

---

## [2.3]
### Added
- 100nF and 4.7uF capacitors to 5Vin line before MCU
- Testpoints for 5V and 3.3V

### Changed
- SPI traces tuned for length matching
- Routing optimised

---

## [2.2]
### Added
- AMS1117 3.3V regulator to power open channels and RYLR998 independently

### Changed
- Removed redundant BJT-based ignition circuitry
- Replaced STM32-L412KB footprint and step file
- Layout and routing modified

---

## [2.1]
### Added
- Thermistor circuitry
- 5V switch for microcontroller

### Changed
- Routing optimised

---

## [2.0]
### Added
- Labels in schematic for clarity
- 5V and 3.3V open power channels
- Step files to all footprints
- Labelling across layout

### Changed
- Overall layout optimised to reduce board dimensions
- SD card reader and D4184 symbol/footprints updated
- AMS1117 capacitor values modified per datasheet
- BJT-based circuitry updated for redundant ignition
- Passive components changed from THT to SMD

---

## [1.0]
### Created
- Initial PCB design
- Schematic and layout
