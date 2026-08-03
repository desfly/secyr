# HomeGuard-S3 Build-0024

Build-0024 introduces an intermediate compiler gate before the real ESP-IDF
container build.

## Added

- lightweight mock headers for the ESP-IDF APIs used by the project;
- syntax-only compilation of every file in `firmware/esp-idf/main`;
- JSON report for every checked translation unit;
- host-gate artifact in GitHub Actions;
- separate host, diagnostic and firmware artifacts;
- targeted checks for app_main, W5500, I2C, UART, microSD and HTTP sources.

## Why this matters

Static text audits cannot catch C++ type errors. The mock syntax stage catches:

- missing includes;
- missing forward declarations;
- invalid aggregate initializers;
- incorrect method signatures;
- accidental namespace/type errors;
- malformed `app_main` integration.

It does not replace the real ESP-IDF compiler because the mocks intentionally
cover only the API surface used by this project.
