# Build-0038 — Persistent AccessControl

Build-0038 makes the operator database reboot-persistent without storing raw PINs.

The portable `AccessStoreCodec` serializes up to eight bounded AccessControl records into a versioned fixed-size image protected by CRC32. The image contains operator ID, display name, role, enabled state, salt and iterative SHA-256 PIN digest only.

ESP-IDF uses `AccessNvsStore` with namespace `hg_access` and key `users_v1`. Boot restore is fail-closed: missing storage yields an empty access database, while malformed, truncated, unsupported-version or CRC-invalid storage is rejected.

Host tests cover round-trip restore, roles, PIN verification after restore, corruption rejection and format-version rejection. No unresolved hardware GPIO is assigned by this build.
