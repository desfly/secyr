# Output driver design

## MCP23017 allocation proposal

Port A outputs:

| Bit | Function | Startup |
|---:|---|---|
| GPA0 | corridor light relay | OFF |
| GPA1 | siren | OFF |
| GPA2 | cold valve OPEN | OFF |
| GPA3 | cold valve CLOSE | OFF |
| GPA4 | hot valve OPEN | OFF |
| GPA5 | hot valve CLOSE | OFF |
| GPA6 | reserve relay 1 | OFF |
| GPA7 | reserve relay 2 | OFF |

Port B inputs:

| Bit | Function |
|---:|---|
| GPB0 | cold valve open limit |
| GPB1 | cold valve closed limit |
| GPB2 | hot valve open limit |
| GPB3 | hot valve closed limit |
| GPB4 | enclosure tamper |
| GPB5 | external service input |
| GPB6 | reserve |
| GPB7 | reserve |

## Driver stage

MCP23017 output -> 1 kΩ to 4.7 kΩ gate/base resistor -> ULN2803A or
logic-level MOSFET driver -> relay/actuator control input.

For bare DC motors use a properly rated reversible H-bridge or relay
interlock module. ULN2803A alone cannot reverse a two-wire motor.

## Hardware interlock

For each valve, OPEN and CLOSE commands must be mutually exclusive by hardware:

- two interlocked relays, or
- H-bridge with shoot-through protection, or
- actuator-specific three-wire controller.

Firmware interlock is additional protection, not the only protection.
