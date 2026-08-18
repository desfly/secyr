from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "esp-idf" / "main"
CORE = ROOT / "firmware" / "src"

errors = []
warnings = []

rules = [
    (
        re.compile(r"\bESP_RETURN_ON_ERROR\b"),
        '#include "esp_check.h"',
        "ESP_RETURN_ON_ERROR requires esp_check.h",
    ),
    (
        re.compile(r"\bpdMS_TO_TICKS\b"),
        '#include "freertos/FreeRTOS.h"',
        "pdMS_TO_TICKS requires FreeRTOS.h",
    ),
    (
        re.compile(r"\bxTaskCreate\b|\bvTaskDelay\b"),
        '#include "freertos/task.h"',
        "FreeRTOS task API requires task.h",
    ),
    (
        re.compile(r"\bstd::uint(?:8|16|32|64)_t\b"),
        "#include <cstdint>",
        "fixed-width integer type requires cstdint",
    ),
    (
        re.compile(r"\bstd::size_t\b"),
        "#include <cstddef>",
        "std::size_t requires cstddef",
    ),
]

for source in sorted(MAIN.glob("*.cpp")) + sorted(MAIN.glob("*.hpp")):
    text = source.read_text(encoding="utf-8")
    for pattern, include, message in rules:
        if pattern.search(text) and include not in text:
            errors.append(f"{source.name}: {message}")

    if "ESP_ERROR_CHECK(" in text and source.suffix == ".hpp":
        warnings.append(f"{source.name}: ESP_ERROR_CHECK found in header")

    if "new " in text or "malloc(" in text:
        warnings.append(f"{source.name}: dynamic allocation should be reviewed")

    if re.search(r"\bGPIO_NUM_(?:19|20|35|36|37|43|44|45|46|48)\b", text):
        warnings.append(f"{source.name}: reserved GPIO referenced")

# AccessControl carries users plus a 64-entry audit trail and is too large to
# copy as an automatic local in ESP-IDF's httpd task. Build-1090 demonstrated
# that such a rollback snapshot can overflow the 8 KiB HTTP stack before the
# handler reaches even its early validation returns. Rollback snapshots must
# therefore live outside the task stack (for example behind a smart pointer).
access_http = (MAIN / "hg_access_http.cpp").read_text(encoding="utf-8")
large_access_stack_copy = re.compile(
    r"\b(?:const\s+)?(?:auto|(?:homeguard::)?AccessControl)\s+\w+\s*=\s*\*access_\s*;"
)
if large_access_stack_copy.search(access_http):
    errors.append(
        "hg_access_http.cpp: AccessControl rollback snapshot must not be copied into the httpd task stack"
    )

# Build-1218 exposed the same class of bug during boot: AccessStoreCodec::decode
# constructed a full temporary AccessControl on ESP-IDF's main task stack while
# AccessNvsStore::load also held the persisted image. Forbid full automatic
# AccessControl temporaries in the NVS codec; decode scratch must stay limited
# to the compact persisted records.
access_store = (CORE / "access_store.cpp").read_text(encoding="utf-8")
full_access_local = re.compile(r"\bAccessControl\s+[A-Za-z_]\w*\s*(?:;|\{)")
if full_access_local.search(access_store):
    errors.append(
        "access_store.cpp: full AccessControl temporary must not be placed on the NVS decode task stack"
    )

for warning in warnings:
    print(f"WARNING: {warning}")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("ESP-IDF source audit PASS")
