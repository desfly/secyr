# HomeGuard-S3

Охоронна система та розумний дім для
**ESP32-S3 HW-678 V0.0.0 / ESP32-S3-WROOM-1-N16R8**.

## Поточний стан

- вихідний код firmware;
- ESP-IDF 5.4.2 workflow;
- вебпанель;
- Android-проєкт;
- драйвери ADS1115, MCP23017, INA226, DS3231, W5500, microSD,
  DS18B20 і RS-485;
- п’ять контрольованих зон із 3 кΩ EOL;
- два PT-506 4–20 мА;
- два водяні крани;
- журнали, телеметрія та API.

## GitHub build

Докладна інструкція:

[README-GITHUB.md](README-GITHUB.md)

Workflow:

```text
.github/workflows/homeguard-build.yml
```

Готовий firmware artifact з’явиться тільки після успішного реального
`idf.py build` у GitHub Actions.
