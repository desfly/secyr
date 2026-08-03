# Safety rules

- Alarm, valve and auxiliary outputs must start OFF during boot and reset.
- Opening a water valve is a dangerous action and requires a one-time challenge.
- Duplicate request IDs must never execute a command twice.
- Maintenance mode forces siren, valves and auxiliary outputs OFF.
- A failed output driver or ADC health check must be visible in telemetry and the journal.
- Network loss must not disable local alarm evaluation.
- Emergency AP is for local recovery; it must not bypass authentication.
- AC/DC power design must include the agreed fuse, BMS, transient suppression and isolation.
