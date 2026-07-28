// SPDX-License-Identifier: MIT

#include <gccore.h>
#include <unistd.h>
#include "constants.h"
#include "io.h"
#include "host.h"
#include "sdio.h"

static bool wait_command_idle(void)
{
    unsigned int i;

    for (i = 0; i < 1000u; ++i)
    {
        if ((mmio_read32(SDHCI_PRESENT_STATE) & SDHCI_CMD_INHIBIT) == 0u)
        {
            return true;
        }
        usleep(100);
    }
    return false;
}

static bool ios_get_ocr(u32 argument, command_result *result,
                        unsigned int *command_count)
{
    unsigned int retry;

    for (retry = 0u; retry < 200u; ++retry)
    {
        *result = send_command(5u, argument, SDHCI_CMD_RESP_SHORT);
        ++*command_count;

        if (!result->complete)
        {
            return false;
        }
        if ((result->response & WLAN_SDIO_OCR_READY) != 0u)
        {
            return true;
        }
    }

    return false;
}

command_result send_command(u8 index, u32 argument, u8 flags)
{
    command_result result = {
        .status = 0u,
        .response = 0u,
        .argument = argument,
        .index = index,
        .complete = false,
    };
    unsigned int i;

    if (!wait_command_idle())
    {
        result.status = SDHCI_INT_TIMEOUT;
        reset_host_line(SDHCI_RESET_CMD);
        return result;
    }

    mmio_write32(SDHCI_INT_STATUS,
                 SDHCI_INT_RESPONSE | SDHCI_INT_ERROR | SDHCI_INT_CMD_ERRORS);
    mmio_write32(SDHCI_ARGUMENT, argument);

    /* Transfer mode (low half) and command (high half) must be one write. */
    mmio_write32(SDHCI_TRANSFER_MODE, (u32)(((u16)index << 8) | flags) << 16);

    for (i = 0; i < 2000u; ++i)
    {
        result.status = mmio_read32(SDHCI_INT_STATUS);
        if ((result.status & (SDHCI_INT_RESPONSE | SDHCI_INT_ERROR |
                              SDHCI_INT_CMD_ERRORS)) != 0u)
        {
            break;
        }
        usleep(100);
    }

    if (i == 2000u)
    {
        result.status |= SDHCI_INT_TIMEOUT;
    }

    if ((result.status & SDHCI_INT_RESPONSE) != 0u &&
        (result.status & SDHCI_INT_CMD_ERRORS) == 0u)
    {
        result.response = mmio_read32(SDHCI_RESPONSE);
        result.complete = true;
    }

    mmio_write32(SDHCI_INT_STATUS,
                 result.status & (SDHCI_INT_RESPONSE | SDHCI_INT_ERROR |
                                  SDHCI_INT_CMD_ERRORS));

    if (!result.complete)
    {
        reset_host_line(SDHCI_RESET_CMD);
    }

    return result;
}

bool cmd52_read(u8 function, u32 address, u8 *value, command_result *last)
{
    const u32 argument =
        ((u32)(function & 7u) << 28) | ((address & 0x1FFFFu) << 9);
    command_result command = send_command(
        52u, argument, SDHCI_CMD_RESP_SHORT | SDHCI_CMD_CRC | SDHCI_CMD_INDEX);

    *last = command;
    if (!command.complete || (command.response & SDIO_R5_ERRORS) != 0u)
    {
        return false;
    }
    *value = (u8)command.response;
    return true;
}

bool cmd52_write(u8 function, u32 address, u8 value, command_result *last)
{
    const u32 argument = 0x80000000u | ((u32)(function & 7u) << 28) |
                         ((address & 0x1FFFFu) << 9) | value;
    command_result command = send_command(
        52u, argument, SDHCI_CMD_RESP_SHORT | SDHCI_CMD_CRC | SDHCI_CMD_INDEX);

    *last = command;
    return command.complete && (command.response & SDIO_R5_ERRORS) == 0u;
}

bool cmd53_read_pio(u8 function, u32 address, u16 count, u32 *value,
                    u32 *status, u32 *response)
{
    const u32 argument = ((u32)(function & 7u) << 28) |
                         0x04000000u | /* Incrementing address. */
                         ((address & 0x1FFFFu) << 9) | (count & 0x1FFu);
    const u16 command = (u16)((53u << 8) | SDHCI_CMD_RESP_SHORT |
                              SDHCI_CMD_CRC | SDHCI_CMD_INDEX | SDHCI_CMD_DATA);
    const u16 transfer = SDHCI_TRNS_READ;
    bool command_complete = false;
    bool response_ok = false;
    bool got_byte = false;
    unsigned int i;

    *status = 0u;
    *response = 0u;

    for (i = 0; i < 1000u; ++i)
    {
        if ((mmio_read32(SDHCI_PRESENT_STATE) &
             (SDHCI_CMD_INHIBIT | SDHCI_DATA_INHIBIT)) == 0u)
        {
            break;
        }
        usleep(100);
    }

    if (i == 1000u)
    {
        u32 present = mmio_read32(SDHCI_PRESENT_STATE);

        if ((present & SDHCI_CMD_INHIBIT) != 0u)
        {
            reset_host_line(SDHCI_RESET_CMD);
        }

        if ((present & SDHCI_DATA_INHIBIT) != 0u)
        {
            reset_host_line(SDHCI_RESET_DATA);
        }

        present = mmio_read32(SDHCI_PRESENT_STATE);
        if ((present & (SDHCI_CMD_INHIBIT | SDHCI_DATA_INHIBIT)) != 0u)
        {
            if ((present & SDHCI_CMD_INHIBIT) != 0u)
            {
                *status |= SDHCI_INT_TIMEOUT;
            }

            if ((present & SDHCI_DATA_INHIBIT) != 0u)
            {
                *status |= SDHCI_INT_DATA_TIMEOUT;
            }

            return false;
        }
    }

    mmio_write32(SDHCI_INT_STATUS, SDHCI_INT_RESPONSE | SDHCI_INT_DATA_END |
                                       SDHCI_INT_DATA_AVAIL | SDHCI_INT_ERROR |
                                       SDHCI_INT_CMD_ERRORS |
                                       SDHCI_INT_DATA_ERRORS);
    mmio_write16(SDHCI_BLOCK_SIZE, count);
    mmio_write16(SDHCI_BLOCK_COUNT, 1u);
    mmio_write32(SDHCI_INT_ENABLE, SDHCI_INT_RESPONSE | SDHCI_INT_DATA_END |
                                       SDHCI_INT_DATA_AVAIL | SDHCI_INT_ERROR |
                                       SDHCI_INT_CMD_ERRORS |
                                       SDHCI_INT_DATA_ERRORS);
    mmio_write32(SDHCI_ARGUMENT, argument);
    mmio_write32(SDHCI_TRANSFER_MODE, ((u32)command << 16) | transfer);

    for (i = 0; i < 3000u; ++i)
    {
        *status = mmio_read32(SDHCI_INT_STATUS);

        if ((*status & SDHCI_INT_RESPONSE) != 0u && !command_complete)
        {
            command_complete = true;
            *response = mmio_read32(SDHCI_RESPONSE);
            response_ok =
                (*response & SDIO_R5_ERRORS_IOS80) == SDIO_R5_STATE_COMMAND;

            if (!response_ok)
            {
                break;
            }
        }

        if ((*status & (SDHCI_INT_CMD_ERRORS | SDHCI_INT_DATA_ERRORS)) != 0u)
        {
            break;
        }

        if (!got_byte && (*status & SDHCI_INT_DATA_AVAIL) != 0u)
        {
            mmio_write32(SDHCI_INT_STATUS, SDHCI_INT_DATA_AVAIL);
            *value = mmio_read32(SDHCI_BUFFER);
            got_byte = true;
        }

        if (got_byte && (*status & SDHCI_INT_DATA_END) != 0u)
        {
            break;
        }

        usleep(100);
    }

    if (i == 3000u)
    {
        if (!command_complete)
        {
            *status |= SDHCI_INT_TIMEOUT;
        }
        else
        {
            *status |= SDHCI_INT_DATA_TIMEOUT;
        }
    }

    mmio_write32(SDHCI_INT_STATUS, *status);

    if ((*status & (SDHCI_INT_CMD_ERRORS | SDHCI_INT_DATA_ERRORS)) != 0u)
    {
        reset_host_line(SDHCI_RESET_DATA);
    }

    if (got_byte && count < 4u)
    {
        *value &= (1u << (count * 8u)) - 1u;
    }

    return command_complete && response_ok && got_byte &&
           (*status & SDHCI_INT_DATA_END) != 0u;
}

bool run_bringup_attempt(bringup_attempt *attempt, u8 power_value)
{
    // reset all following IOS implementation
    mmio_write8(SDHCI_SOFTWARE_RESET,
                SDHCI_RESET_ALL | SDHCI_RESET_CMD | SDHCI_RESET_DATA);
    attempt->reset_ok = wait8_clear(
        SDHCI_SOFTWARE_RESET,
        SDHCI_RESET_ALL | SDHCI_RESET_CMD | SDHCI_RESET_DATA, 1000u, 100u);
    if (!attempt->reset_ok)
    {
        return false;
    }

    mmio_write16(SDHCI_INT_STATUS, 0x01FFu);
    mmio_write16(SDHCI_INT_STATUS + 2u, 0x0FFFu);
    mmio_write16(SDHCI_INT_ENABLE, 0x01FFu);
    mmio_write16(SDHCI_INT_ENABLE + 2u, 0xFFFFu);
    mmio_write16(SDHCI_SIGNAL_ENABLE, 0u);
    mmio_write8(SDHCI_HOST_CONTROL,
                mmio_read8(SDHCI_HOST_CONTROL) & (u8)~0x06u);
    mmio_write8(SDHCI_TIMEOUT_CONTROL, 0x0Eu);

    attempt->clock_ok = configure_clock_divider(attempt->clock_divider);
    attempt->clock_after = mmio_read16(SDHCI_CLOCK_CONTROL);
    if (!attempt->clock_ok)
    {
        attempt->present_after = mmio_read32(SDHCI_PRESENT_STATE);
        return false;
    }

    attempt->power_before = mmio_read8(SDHCI_POWER_CONTROL);
    if (attempt->write_power)
    {
        mmio_write8(SDHCI_POWER_CONTROL, power_value | SDHCI_POWER_ON);
        usleep(50000);
        attempt->power_after = mmio_read8(SDHCI_POWER_CONTROL);
        attempt->power_ok = (attempt->power_after & (SDHCI_POWER_ON | 0x0Eu)) ==
                            (u8)(power_value | SDHCI_POWER_ON);
    }
    else
    {
        /* IOS leaves this register alone on Hollywood. */
        attempt->power_after = attempt->power_before;
        attempt->power_ok = true;
    }

    attempt->present_after = mmio_read32(SDHCI_PRESENT_STATE);
    if (!attempt->power_ok)
    {
        return false;
    }

    if (attempt->send_cmd0)
    {
        attempt->cmd0 = send_command(0u, 0u, SDHCI_CMD_RESP_NONE);
        usleep(2000);
    }

    if (!ios_get_ocr(0u, &attempt->cmd5_inquiry, &attempt->cmd5_command_count))
    {
        attempt->cmd5 = attempt->cmd5_inquiry;
        return false;
    }

    attempt->cmd5 = attempt->cmd5_inquiry;

    if ((attempt->cmd5_inquiry.response & WLAN_SDIO_OCR_NUM_FUNCTIONS_MASK) ==
            0u ||
        (attempt->cmd5_inquiry.response & SDIO_OCR_VDD_32_34) == 0u)
    {
        return false;
    }

    if (!ios_get_ocr(SDIO_OCR_IOS80, &attempt->cmd5,
                     &attempt->cmd5_command_count))
    {
        return false;
    }

    attempt->cmd3 = send_command(3u, 0u, SDHCI_CMD_RESP_SHORT);
    if (!attempt->cmd3.complete ||
        (attempt->cmd3.response & SDIO_R6_ERRORS) != 0u)
    {
        return false;
    }

    attempt->rca = (u16)(attempt->cmd3.response >> 16);

    attempt->cmd7 =
        send_command(7u, (u32)attempt->rca << 16,
                     SDHCI_CMD_RESP_SHORT | SDHCI_CMD_CRC | SDHCI_CMD_INDEX);
    if (!attempt->cmd7.complete ||
        attempt->cmd7.response != SDIO_CMD7_EXPECTED_STATUS)
    {
        return false;
    }

    return true;
}
