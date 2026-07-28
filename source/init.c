// SPDX-License-Identifier: MIT

#include <unistd.h>
#include "types.h"
#include "sdio.h"
#include "cis.h"
#include "io.h"
#include "constants.h"
#include "init.h"

static void copy_bringup_result(probe_result *result,
                                const bringup_attempt *attempt)
{
    result->reset_ok = attempt->reset_ok;
    result->power_ok = attempt->power_ok;
    result->clock_ok = attempt->clock_ok;
    result->cmd0_attempted = attempt->send_cmd0;
    result->cmd0 = attempt->cmd0;
    result->cmd5 = attempt->cmd5;
    result->cmd3 = attempt->cmd3;
    result->rca = attempt->rca;
    result->cmd7 = attempt->cmd7;
    result->ocr = attempt->cmd5.response;
}

static void clear_card_initialization_result(probe_result *result)
{
    result->cmd52_last = (command_result){0};
    result->cccr_revision = 0u;
    result->sdio_revision = 0u;
    result->io_enable = 0u;
    result->io_ready = 0u;
    result->bus_interface = 0u;
    result->card_capabilities = 0u;
    result->common_cis = 0u;
    result->function1_cis = 0u;
    result->cccr_ok = false;
    result->data_path = (data_path_result){0};
}

static bool run_initialization_attempt(probe_result *result,
                                       initialization_attempt *initialization,
                                       u8 power_value)
{
    bringup_attempt *bringup = &initialization->bringup;
    unsigned int ready_attempt;

    clear_card_initialization_result(result);
    initialization->stage = INITIALIZATION_STAGE_BRINGUP;
    initialization->cmd52_last = (command_result){0};
    initialization->complete = false;

    if (!run_bringup_attempt(bringup, power_value))
    {
        copy_bringup_result(result, bringup);
        return false;
    }

    copy_bringup_result(result, bringup);

    initialization->stage = INITIALIZATION_STAGE_CCCR;
    result->cccr_ok =
        cmd52_read(0u, 0x00u, &result->cccr_revision, &result->cmd52_last) &&
        cmd52_read(0u, 0x01u, &result->sdio_revision, &result->cmd52_last) &&
        cmd52_read(0u, 0x02u, &result->io_enable, &result->cmd52_last) &&
        cmd52_read(0u, 0x03u, &result->io_ready, &result->cmd52_last) &&
        cmd52_read(0u, 0x07u, &result->bus_interface, &result->cmd52_last) &&
        cmd52_read(0u, 0x08u, &result->card_capabilities,
                   &result->cmd52_last) &&
        read_cis_pointer(0x00u, &result->common_cis, &result->cmd52_last) &&
        read_cis_pointer(0x100u, &result->function1_cis, &result->cmd52_last);

    initialization->cmd52_last = result->cmd52_last;
    if (!result->cccr_ok)
    {
        return false;
    }

    result->data_path.attempted = true;
    result->data_path.original_io_enable = result->io_enable;
    result->data_path.original_bus_interface = result->bus_interface;

    initialization->stage = INITIALIZATION_STAGE_FUNCTION_ENABLE;
    result->data_path.enable_ok =
        cmd52_write(0u, 0x02u, result->io_enable | 0x02u, &result->cmd52_last);
    initialization->cmd52_last = result->cmd52_last;

    if (!result->data_path.enable_ok)
    {
        return false;
    }

    initialization->stage = INITIALIZATION_STAGE_FUNCTION_READY;
    for (ready_attempt = 0u; ready_attempt < 100u; ++ready_attempt)
    {
        if (!cmd52_read(0u, 0x03u, &result->data_path.ready_value,
                        &result->cmd52_last))
        {
            break;
        }

        if ((result->data_path.ready_value & 0x02u) != 0u)
        {
            result->data_path.ready_ok = true;
            break;
        }

        usleep(10000);
    }

    initialization->cmd52_last = result->cmd52_last;
    if (!result->data_path.ready_ok)
    {
        return false;
    }

    initialization->stage = INITIALIZATION_STAGE_BUS_WIDTH;
    result->data_path.bus_width_ok = cmd52_write(
        0u, 0x07u, result->bus_interface & (u8)0xFCu, &result->cmd52_last);
    initialization->cmd52_last = result->cmd52_last;

    if (!result->data_path.bus_width_ok)
    {
        return false;
    }

    mmio_write8(SDHCI_HOST_CONTROL,
                mmio_read8(SDHCI_HOST_CONTROL) & (u8)~0x02u);

    initialization->stage = INITIALIZATION_STAGE_COMPLETE;
    initialization->complete = true;
    return true;
}

bool run_card_initialization(probe_result *result, u8 power_value)
{
    static const struct
    {
        const char *name;
        u16 divider;
        bool write_power;
        bool send_cmd0;
    } configurations[WLAN_PROBE_MAX_INITIALIZATION_ATTEMPTS] = {
        {"ios-exact", 0x0100u, true, false},
        {"ios-retry", 0x0100u, true, false}};
    unsigned int attempt_index;

    for (attempt_index = 0u;
         attempt_index < WLAN_PROBE_MAX_INITIALIZATION_ATTEMPTS;
         ++attempt_index)
    {
        initialization_attempt *current = &result->init[attempt_index];
        bringup_attempt *bringup = &current->bringup;

        if (attempt_index != 0u &&
            (mmio_read16(SDHCI_CLOCK_CONTROL) &
             (SDHCI_CLOCK_INT_EN | SDHCI_CLOCK_CARD_EN)) ==
                (SDHCI_CLOCK_INT_EN | SDHCI_CLOCK_CARD_EN))
        {
            command_result retry_reset = {0};

            cmd52_write(0u, 0x06u, 0x08u, &retry_reset);
        }

        bringup->name = configurations[attempt_index].name;
        bringup->clock_divider = configurations[attempt_index].divider;
        bringup->write_power = configurations[attempt_index].write_power;
        bringup->send_cmd0 = configurations[attempt_index].send_cmd0;
        ++result->initialization_count;

        if (run_initialization_attempt(result, current, power_value))
        {
            result->winning_initialization = (s8)attempt_index;
            return true;
        }
    }

    return false;
}
