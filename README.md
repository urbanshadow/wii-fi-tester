# Wii-Fi Tester

Wii-Fi Tester is a Wii homebrew app able to check whether an original Wii Wi-Fi module is correctly
installed by communicating directly with it. It is intended for Wii Mini Wi-Fi
installation mod, but it also provides troubleshooting Wi-Fi failures on original Wiis.

The app talks directly to the SDIO controller and is standalone, so it does not depend
on the WLAN driver missing in Wii Mini IOS.

## Usage

Launch Wii-Fi Tester directly from a current Homebrew Channel. Forwarders may
drop the required AHB access.

- Press A to run the test.
- The test result will show on screen almost instantly.
- Press 1 to save the report.
- Press HOME to return to the Homebrew Channel.

The report is saved as `wii-fi-tester-report.txt`. The app tries SD first, then
USB.

## What it checks

- SDIO access and line state
- IOS80-style initialization, including the one full retry
- CMD5, CMD3, CMD7, CMD52, and CMD53 communication
- CCCR and CIS data
- Function 1 startup
- Broadcom SSB identity and core enumeration (including PCI/PCMCIA cores)

Passing confirms the physical connection with the module is reliable enough
to initialize it. It does not test the antennas or radio performance.

Wii-Fi Tester does not install or patch IOS, write to NAND, change network
settings, load WLAN firmware, scan for networks, or transmit anything. The
controller state is restored after the test.

## Build

Install devkitPro's Wii toolchain and libogc, then run:

```sh
make
make check
make package
```

The packaged app is placed in `dist/apps/wii-fi-tester`. Copy that directory
into the `apps` directory on the SD or USB device used by the Homebrew Channel.

Run `make format` to apply the project's clang-format rules.

See [CHANGELOG.md](CHANGELOG.md) for release history.

## License

Wii-Fi Tester is available under the [MIT License](LICENSE). Third-party
library notices are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
