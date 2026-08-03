# HomeGuard-S3 — electrical design Build-0026

## Safety boundary

The ESP32-S3, sensors, RTC, Ethernet, microSD and logic expanders are
SELV low-voltage circuits.

The 230 V measurement section must be galvanically isolated and physically
separated. No mains conductor may connect directly to ESP32, ADS1115,
INA226 or the common low-voltage ground.

## Power architecture

```text
230 VAC
  |
  +-- isolated certified 12 VDC power supply, recommended 12 V / 5 A
        |
        +-- fused 12 V actuator rail
        |     +-- cold-water valve
        |     +-- hot-water valve
        |     +-- siren / relay loads
        |
        +-- 12 V sensor rail
        |     +-- PT-506 transmitters
        |     +-- smoke/gas/flood/motion wired sensors
        |
        +-- 5 V buck converter
        |     +-- optional peripheral modules
        |
        +-- 3.3 V buck/LDO
              +-- HW-678 ESP32-S3
              +-- ADS1115 ×2
              +-- MCP23017
              +-- INA226 logic
              +-- DS3231
              +-- W5500
              +-- microSD
```

Battery 3S2P is connected through a protected charger/BMS and an automatic
power-path or ideal-diode module. The firmware must only monitor the battery;
it must not bypass BMS protection.

## Recommended protection

- AC input: certified fuse and surge protection inside an enclosed DIN module.
- 12 V main input: replaceable fuse.
- Each valve: separate fuse or resettable PTC.
- Each long cable entering the enclosure: series resistor/PTC and TVS.
- I2C leaving the PCB is not recommended.
- RS-485: isolated transceiver, TVS and 120 Ω termination where required.
- All inductive DC loads: flyback suppression at the load or driver.
- MCP23017 outputs must drive ULN2803A/MOSFET/relay drivers, never motors directly.
- Valve OPEN and CLOSE lines need hardware interlock in addition to software.
