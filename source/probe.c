// SPDX-License-Identifier: MIT

/*
 * Wii Mini WLAN SDIO probe
 *
 * This program stops before firmware loading or RF activity. It initializes
 * the SDIO host, enumerates the card, validates DAT0 with CMD53, and reads the
 * BCM4318 Sonics Silicon Backplane directly from the PowerPC.
 */

#include <gccore.h>
#include <string.h>

#include "app.h"
#include "constants.h"
#include "io.h"
#include "sdio.h"
#include "host.h"
#include "cis.h"
#include "ssb.h"
#include "init.h"
#include "probe.h"

bool wlan_command_has_signal_error(const command_result *command)
{
    return (command->status &
            (SDHCI_INT_CRC | SDHCI_INT_END_BIT | SDHCI_INT_INDEX)) != 0u;
}

const char *wlan_command_label(const command_result *command)
{
    if (command->complete)
    {
        return "OK";
    }
    if ((command->status & SDHCI_INT_TIMEOUT) != 0u)
    {
        return "TIMEOUT";
    }
    if (wlan_command_has_signal_error(command))
    {
        return "CRC/LINE";
    }
    return "FAILED";
}

bool wlan_probe_command_line_high(const probe_result *result)
{
    return (result->present_before & SDHCI_CMD_LVL) != 0u;
}

u8 wlan_probe_data_line_levels(const probe_result *result)
{
    return (u8)((result->present_before & SDHCI_DATA_LVL_MASK) >> 20);
}

bool wlan_probe_card_detect_set(const probe_result *result)
{
    return (result->present_before & SDHCI_CARD_PRESENT) != 0u;
}

void wlan_probe_run(probe_result *result)
{
    s32 ios_version;
    u32 ahbprot;
    host_snapshot snapshot = {0};
    u8 power_value;

    if (result == NULL || result->ahbprot != HW_AHBPROT_ENABLED)
    {
        return;
    }

    ios_version = result->ios_version;
    ahbprot = result->ahbprot;
    memset(result, 0, sizeof(*result));
    result->ios_version = ios_version;
    result->ahbprot = ahbprot;

    result->host_version = mmio_read16(SDHCI_HOST_VERSION);
    result->capabilities = mmio_read32(SDHCI_CAPABILITIES);
    result->capabilities_1 = mmio_read32(SDHCI_CAPABILITIES_1);
    result->present_before = mmio_read32(SDHCI_PRESENT_STATE);
    result->host_control_before = mmio_read8(SDHCI_HOST_CONTROL);
    result->power_control_before = mmio_read8(SDHCI_POWER_CONTROL);
    result->clock_control_before = mmio_read16(SDHCI_CLOCK_CONTROL);
    result->winning_initialization = -1;
    /* Version 0x0000 is SDHCI specification 1.0 and is valid on Hollywood. */
    result->controller_plausible = result->host_version != 0xFFFFu &&
                                   result->capabilities != 0u &&
                                   result->capabilities != 0xFFFFFFFFu;
    if (!result->controller_plausible)
    {
        return;
    }

    take_snapshot(&snapshot);

    // disable host signals
    mmio_write32(SDHCI_SIGNAL_ENABLE, 0u);

    // reset IO functions following IOS code
    if ((result->present_before & SDHCI_CARD_PRESENT) != 0u &&
        (snapshot.clock_control & (SDHCI_CLOCK_INT_EN | SDHCI_CLOCK_CARD_EN)) ==
            (SDHCI_CLOCK_INT_EN | SDHCI_CLOCK_CARD_EN))

    {
        result->pre_reset_attempted = true;
        (void)cmd52_write(0u, 0x06u, 0x08u, &result->pre_reset);
    }

    if ((result->capabilities & SDHCI_CAN_VDD_330) != 0u)
    {
        power_value = SDHCI_POWER_330;
    }
    else if ((result->capabilities & SDHCI_CAN_VDD_300) != 0u)
    {
        power_value = SDHCI_POWER_300;
    }
    else
    {
        goto restore_host;
    }

    if (!run_card_initialization(result, power_value))
    {
        goto restore_card;
    }

    if (result->data_path.bus_width_ok)
    {
        bool common_ok = scan_cis(result->common_cis, result);
        bool function_ok = false;

        // avoid hammering cis scans
        if (common_ok || result->cmd52_last.complete)
        {
            function_ok = scan_cis(result->function1_cis, result);
        }

        result->cis_readable = common_ok || function_ok;
    }

    if (result->cis_readable)
    {
        result->data_path.window_ok =
            ssb_set_window(0x18000000u, &result->cmd52_last);
    }

    if (result->data_path.window_ok)
    {
        result->data_path.read_ok = cmd53_read_pio(
            1u, 0x8FFCu, 4u, &result->data_path.read_value,
            &result->data_path.status, &result->data_path.response);
    }

    if (result->data_path.read_ok)
    {
        run_ssb_probe(result);
    }

restore_card:
    if (result->cccr_ok)
    {
        command_result restore_command = {0};

        cmd52_write(0u, 0x02u, result->data_path.original_io_enable,
                    &restore_command);
        cmd52_write(0u, 0x07u, result->data_path.original_bus_interface,
                    &restore_command);
    }

restore_host:
    restore_snapshot(&snapshot);
}
