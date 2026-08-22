# HomeGuard-S3 audit journal — continuation 07

Date: 2026-08-22 (Europe/Kyiv)
Repository: `desfly/secyr`

## Build #1860 hardware result

The user flashed the `HomeGuard-S3 Build #1860` artifact after verifying `homeguard_s3.bin` SHA256:

`f6c43e2b965513f5e908ca6b4fd95c42e7fb58721f3372af29b9bdb78633f332`

The write completed successfully and esptool reported `Hash of data verified` for bootloader, partition table, OTA data, and application.

Real-hardware result: the onboard addressable RGB LED did **not illuminate at all** during the observed RST/EN test — no WHITE and no RED.

## Correction of video interpretation

A previous assistant interpretation incorrectly treated a bright reflection/highlight on the square RGB package as emitted WHITE light. The user correctly clarified that the RGB LED never illuminated in this test. The small red board LED near USB is a separate indicator and must not be confused with the addressable RGB.

Authoritative hardware fact for Build #1860: **RGB = no visible response**.

## Source comparison completed

The current `hg_rgb_diagnostic.cpp` was restored byte-for-byte to the source version at the previously hardware-working Build-1813 commit `22d9f1e804b33d890deca54fdb38595d171ea0ac`; both have blob SHA:

`0c246d3fb045c408185f1719707dd2947177e8c3`

The following source files/config were also checked between Build-1813 commit and current `main` and are identical by blob SHA/content:

- `firmware/esp-idf/main/hg_board_hw678.hpp` — GPIO48 remains onboard RGB;
- `firmware/esp-idf/main/hg_reset_sequence.cpp`;
- `firmware/esp-idf/main/hg_reset_sequence.hpp`;
- `firmware/esp-idf/main/app_main.cpp`;
- `firmware/esp-idf/main/CMakeLists.txt`;
- `firmware/esp-idf/sdkconfig.defaults`.

Therefore, simply changing WS2812 pulse timing or RGB color bytes again is not justified. The exact source driver has already been restored and Build #1860 still failed physically.

## Locked rule

Do not modify the RGB driver, pulse timings, GPIO assignment, or RED/WHITE bytes again without new direct evidence. Do not infer RGB illumination from camera glare or reflections.

## Next investigation

Determine why the Build-1813 hardware behavior is not reproduced by Build #1860 despite identical relevant source blobs. Compare the actual CI build environment/artifacts and boot/runtime conditions, not just the driver source. No further flash should be requested until a concrete difference is identified and recorded.
