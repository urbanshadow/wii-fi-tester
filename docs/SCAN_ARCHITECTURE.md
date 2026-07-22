# Network-scan target

The completion criterion is not merely “card detected.” The homebrew should
cycle through the legal 2.4 GHz channels and display each observed network's
SSID, BSSID, channel, signal level, and security-information summary, while
leaving real NAND and installed IOS titles unchanged.

## Preferred architecture: PPC-resident SoftMAC path

The Wii module is a BCM4318 SoftMAC device behind SSB-over-SDIO. The public
Linux `b43` stack identifies the Wii daughterboard as SDIO `02d0:044b` and the
underlying chip as `14e4:4318`. The no-install design therefore keeps the
driver in the PowerPC homebrew process:

```text
Homebrew UI / scan table
        |
802.11 management-frame parser and channel scan state machine
        |
minimal b43 PHY/MAC bring-up + runtime firmware
        |
SSB-over-SDIO window and register access
        |
CMD52/CMD53 transport
        |
Hollywood SDIO1 MMIO (AHB access)
```

This avoids depending on `/dev/net/wd`, which Wii Mini IOS omits, and avoids
installing a normal-Wii IOS. It is a larger implementation than calling the IOS
network API, but every change disappears when the app exits or the console is
power-cycled.

## Runtime firmware

BCM4318 needs on-chip microcode before it can receive beacon/probe-response
frames. Firmware must be loaded from the app's directory on USB at runtime and
must not be installed to NAND.

Two workable inputs should be supported:

1. OpenFWWF firmware built for the BCM4318/G-PHY revision, where its supported
   feature set is sufficient for passive receive/scan.
2. A user-supplied, locally extracted b43-compatible firmware set. Proprietary
   firmware, IOS contents, and console keys must never be committed or included
   in release archives.

A known-good original Wii is useful as a behavioral oracle for SDIO traces,
register ordering, and error behavior. The production implementation must not
require extracted IOS contents, and a normal-Wii IOS must not be installed on a
Wii Mini's System Menu slot.

## Staged implementation

### Stage 0 — current

- Bounded adaptive SDIO host reset/power/clock setup, including Nintendo's
  reset-mask and initial-divider behavior plus standards-style fallbacks.
- Optional CMD0 followed by CMD5/CMD3/CMD7 enumeration.
- Read-only CMD52 CCCR/CIS identification.
- No IOS WLAN device calls; the running IOS version is informational only.

### Stage 1 — data path (initial diagnostic implemented)

- PIO byte-mode CMD53 with no DMA now validates one function-1 byte.
- Function 1 is enabled with fixed timeouts and its prior state is restored.
- Validate DAT0 by reading the SSB chip-identification register.
- Implement the BCM4318 read-after-write32 SDIO quirk.
- Restore function-enable and host state on all exits.

### Stage 2 — SSB and device identity

- Discover SSB cores and revisions through the SDIO address window.
- Read SPROM/board calibration data and MAC address.
- Reset and clock the 802.11 core without enabling RF.
- Compare identities and raw traces with a known-good original Wii.

### Stage 3 — firmware and receive-only radio

- Validate firmware file size, format, revision, and checksum before touching
  the device.
- Upload microcode/initvals, start the MAC, and verify its self-test/status.
- Configure one legal channel in receive-only mode.
- Receive and validate a beacon frame with bounded FIFO draining.

### Stage 4 — scan UI

- Passive dwell on legal channels first; do not transmit probe requests.
- Parse beacons into a deduplicated BSSID table.
- Show hidden SSIDs safely and sanitize all lengths before display.
- Add optional active probes only after passive scan is stable.
- Derive allowed channels from the console/module country data; never assume
  channel 12/13 legality globally.

## Soldering-aware diagnostics

Each stage must remain separately visible. In particular:

- CMD5 timeout: power, CLK, CMD, seating, or passives.
- Command CRC/index/end-bit errors: CLK/CMD integrity.
- CMD52 succeeds but CMD53 times out/CRC fails: DAT0 or its series resistor.
- SSB chip ID is unstable: marginal power, clock, or SDIO signal integrity.
- Firmware starts but no beacons appear on any channel: antenna/RF/PHY setup,
  not basic SDIO soldering.

All waits must be finite. The app must keep a last-known stage and raw status so
an unplugged, bridged, or intermittently connected module produces a report
rather than a permanent black screen.
