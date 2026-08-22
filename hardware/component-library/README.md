# HomeGuard-S3 Component Library

Еталонна бібліотека **конкретних фізичних компонентів** проєкту HomeGuard-S3.

## Закон бібліотеки

**Фото → факт → картка. Нема підтвердженого факту — нема запису.**

- **1 компонент = 1 односторінкова PDF-картка.**
- У картці використовується фото саме того фізичного модуля, який затверджений для HomeGuard-S3.
- Фіксуються точна назва/маркування, параметри, видимий pinout і підтверджені особливості.
- Не підміняти компонент іншим модулем лише через однаковий чип або схожий вигляд.
- Не домислювати pinout, рівні керування, номінали або характеристики, яких не видно на фото і які не були окремо підтверджені.
- Заміна компонента в проєкті означає створення нової/оновленої картки та окреме затвердження.

## Затверджений реєстр

| ID | Компонент | Статус | Картка |
|---|---|---|---|
| HG-COMP-001 | ESP S3-N16R8 HW678 V0.0.0 WiFi+BT Sixspan | **APPROVED** | [PDF](cards/HG-COMP-001_ESP-S3-N16R8_HW678_V0.0.0_Sixspan.pdf) |
| HG-COMP-002 | W5500 Ethernet Module, blue PCB, RJ45 | **APPROVED** | [PDF](cards/HG-COMP-002_W5500_photo_card.pdf) |
| HG-COMP-003 | Micro SD / TF SPI Module | **APPROVED** | [PDF](cards/HG-COMP-003_MicroSD_module_photo_card.pdf) |
| HG-COMP-004 | ADS1115 HW-198, 16Bit I2C ADC+PGA — 2 модулі | **APPROVED** | [PDF](cards/HG-COMP-004_ADS1115_HW-198_x2_photo_card.pdf) |
| HG-COMP-005 | INA226 IIC / CJMCU-226 | **APPROVED** | [PDF](cards/HG-COMP-005_INA226_CJMCU_photo_card.pdf) |
| HG-COMP-006 | PZEM-004T 004T-100A-D-P + external Close CT | **APPROVED** | [PDF](cards/HG-COMP-006_PZEM-004T_100A_CT_photo_card.pdf) |
| HG-COMP-007 | KWS-40007 motorized ball valve 1/2", 3-wire / 2-point, AC 220 V | **APPROVED** | [PDF](cards/HG-COMP-007_KWS-40007_1-2inch_photo_card.pdf) |
| HG-COMP-008 | 4-channel 5 V optocoupler relay module — **red PCB** | **APPROVED** | [PDF](cards/HG-COMP-008_4Relay_5V_Optocoupler_photo_card.pdf) |

## HG-COMP-008 — контроль від підміни

Еталоном є **саме червона плата з фото користувача**:

- 4 сині реле `JQC-3FF-S-Z`;
- котушки `5VDC`;
- видимі написи реле `10A/250VAC` та `15A/125VAC`;
- клеми керування `DC+`, `DC-`, `IN1`, `IN2`, `IN3`, `IN4`;
- жовті перемички `S1`, `S2`, `S3`, `S4`;
- на платі є позначення `H/L level module`;
- точне положення H/L та порядок силових `NO/COM/NC` **не вважати встановленими**, доки це не буде підтверджено тестом або читабельним маркуванням.

Будь-яка синя/чорна типова 4-релейна плата, навіть з подібними реле, **не є HG-COMP-008**.

---

**Library status:** HG-COMP-001 … HG-COMP-008 — APPROVED  
**Updated:** 2026-08-22
