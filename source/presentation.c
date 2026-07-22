// SPDX-License-Identifier: MIT

#include <stdio.h>

#include "app.h"
#include "diagnosis.h"
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

static void print_data_path(const probe_result *result)
{
    if (!result->data_path.attempted)
    {
        printf("DAT0: SKIP (probe stopped at an earlier stage)\n");
        return;
    }

    printf("DAT0: 1bit=%s enable=%s ready=%s window=%s CMD53=%s\n",
           result->data_path.bus_width_ok ? "OK" : "FAIL",
           result->data_path.enable_ok ? "OK" : "FAIL",
           result->data_path.ready_ok ? "OK" : "FAIL",
           result->data_path.window_ok ? "OK" : "FAIL",
           result->data_path.read_ok ? "OK" : "FAIL");
    printf("      value=%08lX status=%08lX response=%08lX\n",
           (unsigned long)result->data_path.read_value,
           (unsigned long)result->data_path.status,
           (unsigned long)result->data_path.response);
}

static void print_ssb(const probe_result *result)
{
    if (!result->ssb.attempted)
    {
        printf("SSB:  SKIP (probe stopped at an earlier stage)\n");
        return;
    }

    printf("SSB:  idhigh=%08lX chip=%08lX cores=%u/%u\n",
           (unsigned long)result->ssb.chipcommon_idhigh,
           (unsigned long)result->ssb.chip_id_register, result->ssb.cores_read,
           result->ssb.reported_core_count);
}

void presentation_print_results(const probe_result *result)
{
    printf("\x1b[2J\x1b[H");
    printf(APP_NAME " " APP_VERSION "\n");
    printf("No NAND writes / native SDIO+SSB / no WLAN IOS driver\n\n");
    printf("Running IOS: %ld (informational only)\n",
           (long)result->ios_version);
    printf("AHBPROT: %08lX\n", (unsigned long)result->ahbprot);

    if (result->ahbprot == HW_AHBPROT_ENABLED)
    {
        printf("Host: v%04X caps=%08lX caps1=%08lX\n", result->host_version,
               (unsigned long)result->capabilities,
               (unsigned long)result->capabilities_1);
        printf("Lines before probe: CMD=%s DAT[3:0]=%X card-detect=%s\n",
               wlan_probe_command_line_high(result) ? "high" : "LOW",
               wlan_probe_data_line_levels(result),
               wlan_probe_card_detect_set(result) ? "set" : "clear*");
        printf("Host before: control=%02X power=%02X clock=%04X\n",
               result->host_control_before, result->power_control_before,
               result->clock_control_before);
        printf("Pre-reset CMD52: %s\n",
               result->pre_reset_attempted
                   ? wlan_command_label(&result->pre_reset)
                   : "SKIP");
        printf("Host reset/power/clock: %s / %s / %s\n",
               result->reset_ok ? "OK" : "FAIL",
               result->power_ok ? "OK" : "FAIL",
               result->clock_ok ? "OK" : "FAIL");
        printf(
            "Bring-up: %s (%d/%u) CMD0=%s CMD5=%s",
            result->winning_attempt >= 0
                ? result->bringup[(u8)result->winning_attempt].name
                : "none",
            result->winning_attempt >= 0 ? (int)result->winning_attempt + 1 : 0,
            result->bringup_count,
            result->cmd0_attempted ? wlan_command_label(&result->cmd0) : "SKIP",
            wlan_command_label(&result->cmd5));
        if (result->cmd5.complete)
        {
            printf(" OCR=%08lX funcs=%lu ready=%s", (unsigned long)result->ocr,
                   (unsigned long)((result->ocr &
                                    WLAN_SDIO_OCR_NUM_FUNCTIONS_MASK) >>
                                   28),
                   (result->ocr & WLAN_SDIO_OCR_READY) != 0u ? "yes" : "no");
        }
        printf("\nCMD3=%s RCA=%04X CMD7=%s CMD52=%s\n",
               wlan_command_label(&result->cmd3), result->rca,
               wlan_command_label(&result->cmd7),
               wlan_command_label(&result->cmd52_last));
        if (result->cccr_ok)
        {
            printf("CCCR=%02X SDIO=%02X enable=%02X ready=%02X bus=%02X "
                   "caps=%02X\n",
                   result->cccr_revision, result->sdio_revision,
                   result->io_enable, result->io_ready, result->bus_interface,
                   result->card_capabilities);
            printf("CIS common=%06lX function1=%06lX",
                   (unsigned long)result->common_cis,
                   (unsigned long)result->function1_cis);
            if (result->manfid_found)
            {
                printf(" MANFID=%04X:%04X", result->manufacturer,
                       result->product);
            }
            printf("\n");
        }
        print_data_path(result);
        print_ssb(result);
        printf("*Card-detect is informational for internal WLAN.\n");
    }

    print_diagnosis(result);
    printf("\nHOME: return to Homebrew Channel\n");
}
