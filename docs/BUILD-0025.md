# HomeGuard-S3 Build-0025

Build-0025 adds the final host-side gate before the first genuine ESP-IDF build.

## New release gates

1. Project preflight.
2. Source/header audit.
3. ESP-IDF component dependency audit.
4. Reserved and duplicate GPIO audit.
5. 16 MiB partition and firmware budget audit.
6. Mock syntax compilation.
7. Mock compilation and final link of all ESP-IDF `main` sources.

## Firmware budget

The factory and both OTA application partitions are checked for identical
size. A warning threshold is calculated at 85% of the available application
partition.

The real CI additionally runs:

```text
idf.py size
idf.py size-components
idf.py size-files
```

## Meaning of mock link PASS

Mock link PASS confirms that the project translation units can compile and
link together using the modeled API surface. It catches duplicate symbols,
missing definitions and unresolved internal references.

It still does not prove compatibility with every real ESP-IDF declaration.
Only the pinned ESP-IDF 5.4.2 build can produce a flashable binary.
