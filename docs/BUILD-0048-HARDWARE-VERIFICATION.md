# HomeGuard-S3 Build-0048 — Hardware verification

Build-0048 adds a fail-closed hardware verification layer between the abstract board profile and any future permission to energize physical outputs.

The verification record contains the complete pin map, active-polarity verification state, verification timestamp and CRC32. Validation rejects unsupported schemas, invalid or duplicate GPIO assignments, missing required output assignments, unverified polarity, missing timestamp and CRC mismatch.

The implementation deliberately does not invent GPIO numbers or active levels. A record is considered valid only after real board mapping has been entered and verified. Until then, physical outputs must remain blocked.

Host tests cover valid records and all critical rejection paths. The same implementation is included in the ESP-IDF firmware build. Android is synchronized to version 0.0.48 / versionCode 48.
