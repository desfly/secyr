#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "firmware/include/homeguard/access_control.hpp"
SOURCE = ROOT / "firmware/src/access_control.cpp"
STORE = ROOT / "firmware/src/access_store.cpp"
errors: list[str] = []


def read(path: Path) -> str:
    if not path.is_file():
        errors.append(f"missing access concurrency source: {path.relative_to(ROOT)}")
        return ""
    return path.read_text(encoding="utf-8")


def require(body: str, token: str, label: str) -> None:
    if token not in body:
        errors.append(f"access concurrency contract regressed: {label}")


header = read(HEADER)
source = read(SOURCE)
store = read(STORE)

require(source, "std::recursive_mutex g_access_control_mutex", "cross-task recursive mutex")
for token, label in [
    ("void AccessControl::set_auth_clock(AuthClock clock)", "locked auth clock setter"),
    ("bool AccessControl::auth_throttle_enabled() const", "locked auth clock reader"),
    ("std::size_t AccessControl::user_count() const", "locked user count reader"),
    ("std::size_t AccessControl::audit_size() const", "locked audit count reader"),
]:
    require(source, token, label)

if source.count("thread_local AccessUser snapshot") < 2:
    errors.append("access concurrency contract regressed: user_at/find_user do not both return immutable thread-local snapshots")
require(source, "thread_local AccessAuditRecord snapshot", "audit reader immutable snapshot")
if "return &users_[" in source or "return &audit_[" in source:
    errors.append("access concurrency contract regressed: raw internal array pointer escapes AccessControl lock")

for forbidden, label in [
    ("std::size_t user_count() const {", "inline unlocked user_count"),
    ("std::size_t audit_size() const {", "inline unlocked audit_size"),
    ("void set_auth_clock(AuthClock clock) noexcept {", "inline unlocked auth clock setter"),
    ("bool auth_throttle_enabled() const noexcept {", "inline unlocked auth clock reader"),
]:
    if forbidden in header:
        errors.append(f"access concurrency contract regressed: {label}")

for token, label in [
    ("access.user_count()", "NVS codec uses locked user count"),
    ("access.user_at(i)", "NVS codec uses snapshot user reader"),
]:
    require(store, token, label)

if source.count("std::scoped_lock lock(g_access_control_mutex)") < 16:
    errors.append("access concurrency contract regressed: AccessControl state is not consistently serialized")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("Access concurrency contract: PASS")
