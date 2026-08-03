# HomeGuard-S3 — GitHub Build-0029

Це готовий корінь GitHub-репозиторію проєкту охоронної системи та
розумного дому на **ESP32-S3 HW-678 V0.0.0 / N16R8**.

## Що збирає GitHub Actions

Workflow:

```text
.github/workflows/homeguard-build.yml
```

Виконує два послідовні етапи:

1. **Host validation**
   - preflight;
   - аудит ESP-IDF API;
   - аудит залежностей;
   - контроль GPIO;
   - контроль таблиці розділів;
   - mock syntax;
   - mock compile/link.

2. **ESP-IDF 5.4.2 firmware**
   - `idf.py set-target esp32s3`;
   - `idf.py reconfigure`;
   - `idf.py build`;
   - звіт про розмір;
   - firmware artifact;
   - diagnostic artifact навіть після помилки.

## Як завантажити репозиторій через браузер

1. Створіть у GitHub порожній репозиторій.
2. Не додавайте README, `.gitignore` або License під час створення.
3. Розпакуйте ZIP **HomeGuard-S3-GitHub-Build-0029.zip**.
4. Відкрийте репозиторій → **Add file → Upload files**.
5. Перетягніть **вміст папки**, а не саму зовнішню папку.
6. Перевірте, що в корені GitHub видно:

```text
.github/
android/
docs/
firmware/
hardware/
tests/
tools/
web/
README.md
README-GITHUB.md
```

7. Натисніть **Commit changes**.
8. Відкрийте вкладку **Actions**.
9. Запустіть **HomeGuard-S3 Build** через `Run workflow`.

## Результати

При успішній збірці:

```text
HomeGuard-S3-firmware
```

При успіху або помилці:

```text
HomeGuard-S3-ESP-IDF-diagnostics
HomeGuard-S3-host-diagnostics
```

Після першого запуску завантажте сюди diagnostics ZIP або firmware ZIP.
