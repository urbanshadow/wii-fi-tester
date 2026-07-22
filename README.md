# Wii-Fi Tester

Wii-Fi Tester is a small Wii homebrew utility for validating a manually
installed original-Wii WLAN module on a Wii Mini. It talks directly to the
Hollywood SDIO1 controller at `0x0d080000` with Homebrew Channel AHB access, so
it does not need the WLAN driver that Nintendo removed from Wii Mini IOS. It
also runs on an original Wii as a known-good hardware baseline.

Its complete scope is native PowerPC hardware-transport validation:

- Checks that AHB access was preserved by the loader.
- Confirms the SDIO1 host-controller register block is sane.
- Samples CMD and DAT line levels for soldering diagnostics.
- Uses bounded reset, power, clock, and command waits; it should not hang on an
  absent, mis-seated, or badly soldered module.
- Tries a bounded Wii-specific/standards-style clock, reset, power, and CMD0
  bring-up matrix, stopping as soon as CMD5 answers.
- Runs SDIO CMD5/CMD3/CMD7 enumeration after a successful bring-up.
- Reads the standard CCCR and common/function CIS with read-only CMD52 calls.
- Temporarily enables function 1 and performs one bounded four-byte CMD53 PIO
  read to validate DAT0, then restores the original function-enable state.
- Opens the native SSB address window and enumerates raw ChipCommon/core IDs.
- Reports raw status and a stage-specific diagnosis.
- Restores the host-controller control registers after the probe.
- Saves a complete report as `wii-fi-tester-report.txt` on SD or USB.
- Treats an unattempted later stage as `SKIP`, rather than reporting a
  misleading failure after an expected earlier stop.

It does **not** write NAND, install/reload/patch IOS, alter network settings,
load WLAN firmware, activate RF, receive or transmit packets, scan SSIDs, or
leave an SDIO function enabled. The direct probe performs only the small CMD53
reads required for DAT0 and SSB identity validation. No IOS WLAN device is
opened or called.

## Build

Install devkitPro's Wii toolchain and libogc, then:

```sh
make
make check
make package
```

The installable directory is `dist/apps/wii-fi-tester`. Copy that directory to
the `apps` directory of the FAT device used by the Homebrew Channel. On Wii
Mini this will normally be USB storage.

## Source layout

- `source/probe.c` owns all SDHCI/SDIO/SSB access and guarantees bounded waits
  plus restoration of the original host-controller state.
- `source/diagnosis.c` converts raw probe evidence into stable diagnostic codes.
- `source/presentation.c` renders the human-readable console view.
- `source/report.c` writes the machine-readable report, trying SD and then USB
  and checking both stream errors and the final close.
- `source/main.c` contains only application startup and controller flow.

Launch it directly from a current Homebrew Channel. The supplied `meta.xml`
contains both `ahb_access` and its older `no_ios_reload` alias. A forwarder may
discard that request, so the program refuses direct SDIO access unless
`HW_AHBPROT` reads back as `0xffffffff`.

## Interpreting failures

| Last good stage | Likely area |
| --- | --- |
| AHB check fails | Loader did not preserve direct hardware access |
| SDIO1 registers invalid | Host access or Hollywood clock/power gating; not yet a module-solder verdict |
| Host reset/clock fails | SDIO controller setup or clock gate |
| CMD5 times out in every matrix entry | Module seating, GND, 3.3/1.8 V, CLK, CMD, or associated passives |
| CMD5 CRC/index/end-bit error | CLK/CMD bridge, joint, series resistor, or signal integrity |
| CMD5 works but card never becomes ready | Marginal power or damaged module |
| CMD3/CMD7 fails | CMD/CLK quality or power stability under enumeration |
| CCCR/CIS succeeds but CMD53 fails | DAT0 or its series resistor/joints |
| CMD53 succeeds but SSB identity is invalid | Function-1 window mapping or unstable module |
| SSB core enumeration succeeds | Native SDIO hardware transport passed |

The host `card-detect` bit is shown only as raw evidence. The internal WLAN
module is non-removable and does not have a useful card-detect switch, so a
clear bit alone is not a fault.

## Scope boundary

Success means the SDIO function, DAT0 data path, and native SSB cores enumerate
reliably. Wii-Fi Tester is a hardware and soldering diagnostic, not a Wi-Fi
driver or networking application. Firmware loading, PHY/MAC initialization,
radio operation, network scanning, and association are intentionally outside
the project.

For console validation, use the one-run
[`docs/TEST_CHECKLIST.md`](docs/TEST_CHECKLIST.md). Release changes are tracked
in [`CHANGELOG.md`](CHANGELOG.md).

The project is self-contained: building and running it requires no IOS binaries,
WAD extraction, console keys, Python tools, or WLAN firmware. Do not add IOS
binaries, extracted Nintendo modules, keys, WADs, or firmware blobs to this
repository. Installing a normal System Menu IOS on a Wii Mini is outside this
project's design and can make the menu hang when WLAN is absent.

## License

The project source is available under the [MIT License](LICENSE). Builds link
third-party devkitPro libraries that remain under their own terms; their
required attribution is collected in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) and included by `make
package`.

This is an unofficial homebrew project and is not affiliated with or endorsed
by Nintendo or Broadcom. Wii is a trademark of Nintendo.

## Technical references

- WiiBrew: Hollywood 802.11 controller (`0x0d080000`, reversed little endian)
- WiiBrew: `HW_AHBPROT` and SD1 access rights
- SD Host Controller Simplified Specification
- SDIO Simplified Specification
- Linux `sdhci-of-hlwd.c` (32-bit byte-swap access and 5 us write delay)
