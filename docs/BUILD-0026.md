# HomeGuard-S3 Build-0026

Build-0026 continues without GitHub and concentrates on the electrical
implementation and first physical prototype.

## Added

- complete low-voltage power architecture;
- explicit galvanic isolation boundary for 230 V measurement;
- connector-level logical netlist;
- supervised-zone input front end requirements;
- two PT-506 4–20 mA input circuits;
- two motorized-valve connector definitions;
- proposed MCP23017 port allocation;
- driver and hardware-interlock requirements;
- exact W5500, microSD, 1-Wire and RS-485 wiring;
- preliminary bill of materials;
- preliminary power budget;
- staged first-bench test procedure;
- editable Excel workbook for BOM, wiring, power and test status.

## Important unresolved hardware facts

Before producing a final PCB schematic, the following exact parts must be
confirmed:

- cold and hot valve model and wiring;
- valve running and stall current;
- exact 230 V energy meter or isolated measurement module;
- actual INA226 module shunt resistance;
- actual 12 V sensor models and output contacts;
- selected power-path/charger/BMS module for the 3S2P battery.

The documentation deliberately does not invent these values.
