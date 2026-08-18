# HomeGuard-S3 — Cemented UI Requirements

These requirements are **locked** unless Viktor explicitly changes them.

## Bruce artwork — Web UI and Android

The approved Bruce reference is the user-supplied image where **both hands / both fists are visible in frame**.

Mandatory rules:

- Web UI `bruce.jpg` must use this approved two-fist composition.
- Android Bruce artwork/icon treatment must preserve the same approved composition when Bruce is shown.
- **Both fists must remain visible.**
- Do **not** crop away the lower fist/hand.
- Do **not** use the previous one-hand / one-fist variants.
- Do **not** blur the artwork.
- Do **not** stretch or distort face/body proportions.
- Use `object-fit: contain` or equivalent presentation when necessary to preserve the complete approved composition.
- A build that shows Bruce with only one fist, missing hand, blurred artwork, or incorrect crop is a **UI regression / acceptance failure**.

## Acceptance check

Before accepting Web UI or Android builds, visually verify:

1. Bruce is sharp.
2. Both fists are visible.
3. Lower fist is not clipped.
4. Artwork proportions are unchanged.
5. Bruce does not overlap or obscure navigation/UI controls.

Status: **CEMENTED / LOCKED**.
