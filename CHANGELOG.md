# Changelog

## 0.4.0 — 2026-07-22

- Adopted the project name Wii-Fi Tester consistently across the application,
  package, report, and documentation.
- Separated hardware probing, diagnosis, presentation, report storage, and
  application flow into focused modules.
- Added stable machine-readable diagnosis codes.
- Added an internal AHB access guard and a single host-state restoration path.
- Made report saving verify writes and final close before reporting success or
  falling back from SD to USB.
- Display unattempted DAT0 and SSB stages as `SKIP`.
- Added formatting, strict-warning, and GCC static-analysis checks.
- Licensed the project under MIT and added third-party library notices to
  packaged releases.

## 0.3.2 — 2026-07-22

- Corrected the SSB backplane identity read and validated native SSB
  enumeration on an original Wii.
- Forced and restored a temporary one-bit SDIO bus for the DAT0 diagnostic.

## 0.3.1 — 2026-07-22

- Added the volatile pre-reset sequence needed to enumerate a WLAN module that
  had already been initialized by IOS.
