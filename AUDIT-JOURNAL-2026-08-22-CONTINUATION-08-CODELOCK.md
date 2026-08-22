Build #1821 hardware-proven reset detector blobs to restore exactly:

- firmware/esp-idf/main/hg_reset_sequence.cpp = dd12150a58fabf0e99f5bf5d8172648d013ffd4a
- firmware/include/homeguard/reset_sequence.hpp = a01ae4553f72a78531922e2cc6cdee13a59af4d1
- tests/test_reset_sequence.cpp = 138c069a9a4fd6ebadcb557be1147e3fb259a2c3
- tools/check_reset_rgb_contract.py = 0d0d1f5b751627b05202eb8208de9a6203bf87a5
- tools/check_access_boundary.py = 8f0563f7dfdcaba0feea9fab847ad6e0b3dac3fc

These blobs come directly from commit 2c81591e5356a1c048691053bdac80796e1b4d59, whose hardware validation recorded WHITE x3 -> RED x1. RGB driver is intentionally not changed.
