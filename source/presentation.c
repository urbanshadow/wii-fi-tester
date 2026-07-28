// SPDX-License-Identifier: MIT

#include <gccore.h>
#include <stdio.h>

#include "app.h"
#include "types.h"
#include "diagnosis.h"
#include "probe.h"
#include "presentation.h"

static void print_diagnosis(const probe_result *result)
{
    const probe_diagnosis diagnosis = probe_classify(result);

    printf("\nDiagnosis [%s]: ", probe_diagnosis_code(diagnosis));

    switch (diagnosis)
    {
    case PROBE_DIAGNOSIS_AHB_MISSING:
        printf("AHB access was not preserved.\n");
        printf("Launch this exact folder from current HBC; do not use a "
               "forwarder.\n");
        break;
    case PROBE_DIAGNOSIS_CONTROLLER_UNREADABLE:
        printf("SDIO1 controller is not readable/plausible.\n");
        printf(
            "This is an access/clock-gate issue, not yet a solder verdict.\n");
        break;
    case PROBE_DIAGNOSIS_HOST_SETUP_FAILED:
        printf("Host controller setup failed before talking to the module.\n");
        break;
    case PROBE_DIAGNOSIS_CMD5_SIGNAL_ERROR:
        printf("Command response has CRC/index/end-bit errors.\n");
        printf("Inspect CLK/CMD integrity, bridges, and series resistors.\n");
        break;
    case PROBE_DIAGNOSIS_CMD5_NO_RESPONSE:
        printf("No SDIO OCR response in the bounded bring-up matrix.\n");
        printf("Inspect module seating, power, GND, CLK, CMD, and passives.\n");
        break;
    case PROBE_DIAGNOSIS_CARD_NOT_READY:
        printf("Module answers CMD5 but never becomes ready.\n");
        printf("Power is likely marginal or the module is held/reset "
               "incorrectly.\n");
        break;
    case PROBE_DIAGNOSIS_ENUMERATION_FAILED:
        printf("SDIO card answers, but enumeration fails (%s/%s).\n",
               wlan_command_label(&result->cmd3),
               wlan_command_label(&result->cmd7));
        printf("Recheck CLK/CMD signal quality and module power stability.\n");
        break;
    case PROBE_DIAGNOSIS_CCCR_FAILED:
        printf("Card enumerates, but CMD52 register reads fail (%s).\n",
               wlan_command_label(&result->cmd52_last));
        printf("Failed CMD%u: %s F%lu address=%05lX argument=%08lX.\n",
               (unsigned int)result->cmd52_last.index,
               (result->cmd52_last.argument & 0x80000000u) != 0u ? "write"
                                                                 : "read",
               (unsigned long)((result->cmd52_last.argument >> 28) & 7u),
               (unsigned long)((result->cmd52_last.argument >> 9) & 0x1FFFFu),
               (unsigned long)result->cmd52_last.argument);
        break;
    case PROBE_DIAGNOSIS_CIS_FAILED:
        printf("CCCR works; CIS pointer/data is invalid or unreadable.\n");
        break;
    case PROBE_DIAGNOSIS_FUNCTION_START_FAILED:
        printf("Function 1 did not enable/become ready.\n");
        printf("Module power or internal function startup is suspect.\n");
        break;
    case PROBE_DIAGNOSIS_SSB_WINDOW_FAILED:
        printf("Function 1 works, but SSB window CMD52 writes failed.\n");
        break;
    case PROBE_DIAGNOSIS_DATA_TRANSFER_FAILED:
        printf("CMD52/window works but CMD53 data transfer failed.\n");
        printf("Inspect DAT0 and its series resistor/joints.\n");
        break;
    case PROBE_DIAGNOSIS_SSB_IDENTITY_INVALID:
        printf("DAT0 works, but the SSB ChipCommon identity is invalid.\n");
        printf("Check SDIO function/window handling or module stability.\n");
        break;
    case PROBE_DIAGNOSIS_PASSED:
        printf("WLAN SDIO and native SSB enumeration passed.\n");
        printf("No WLAN IOS driver was used.\n");
        break;
    }
}

void presentation_print_results(const probe_result *result)
{
    printf("\x1b[2J\x1b[H");
    printf(APP_NAME " " APP_VERSION "\n\n");

    const probe_diagnosis diagnosis = probe_classify(result);

    if (diagnosis == PROBE_DIAGNOSIS_PASSED)
    {
        printf("TEST PASSED\n\n");
        printf("Communication with the Wi-Fi module was successful.\n");
        printf("The module started correctly and internal components "
               "responded.\n\n");
        printf("This confirms the wired hardware connection to the module.\n");
        printf("It does not test radio performance, so check antennas are "
               "well connected.\n\n");
    }
    else
    {
        printf("TEST FAILED\n\n");
        printf("An error was detected starting the Wi-Fi module\n\n");
        printf("Check the diagnostic result for clues. Sometimes simply\n");
        printf("reseating the module helps, but rechecking electrical\n");
        printf("connections is advised.\n\n");

        print_diagnosis(result);
    }
}
