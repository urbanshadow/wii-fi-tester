# Release test checklist

Use the same packaged `boot.dol` on both consoles. Each probe is bounded and
writes its report before showing the final HOME prompt; powering off is not
required to finish the report.

## Original Wii

1. Launch directly from the Homebrew Channel, not through a forwarder.
2. Confirm `AHB access OK`, then press A once.
3. Expected final diagnosis: `passed`.
4. Preserve `wii-fi-tester-report.txt` from SD or USB.

## Wii Mini without a WLAN module

1. Launch the same build from FAT-formatted USB storage.
2. Confirm `AHB access OK`, then press A once.
3. Expected diagnosis: `cmd5-no-response` after all five bounded bring-up
   attempts. DAT0 and SSB should show `SKIP`.
4. Confirm the report is saved as `usb:/wii-fi-tester-report.txt`.

## Wii Mini after hardware installation

Run the same procedure once. Do not repeat a failed test before inspecting the
report: its raw line levels, five bring-up attempts, command status, DAT0 stage,
and SSB stage are intended to localize the next check from a single run.
