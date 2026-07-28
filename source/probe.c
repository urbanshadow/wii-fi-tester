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
#include <unistd.h>

#include "app.h"
#include "probe.h"
#define WLAN_SDIO_BASE 0xCD080000u

/* SDHCI register offsets. */
#define SDHCI_ARGUMENT 0x08u
#define SDHCI_BLOCK_SIZE 0x04u
#define SDHCI_BLOCK_COUNT 0x06u
#define SDHCI_TRANSFER_MODE 0x0Cu
#define SDHCI_RESPONSE 0x10u
#define SDHCI_BUFFER 0x20u
#define SDHCI_PRESENT_STATE 0x24u
#define SDHCI_HOST_CONTROL 0x28u
#define SDHCI_POWER_CONTROL 0x29u
#define SDHCI_CLOCK_CONTROL 0x2Cu
#define SDHCI_TIMEOUT_CONTROL 0x2Eu
#define SDHCI_SOFTWARE_RESET 0x2Fu
#define SDHCI_INT_STATUS 0x30u
#define SDHCI_INT_ENABLE 0x34u
#define SDHCI_SIGNAL_ENABLE 0x38u
#define SDHCI_CAPABILITIES 0x40u
#define SDHCI_CAPABILITIES_1 0x44u
#define SDHCI_HOST_VERSION 0xFEu

#define SDHCI_CMD_INHIBIT 0x00000001u
#define SDHCI_DATA_INHIBIT 0x00000002u
#define SDHCI_CARD_PRESENT 0x00010000u
#define SDHCI_DATA_LVL_MASK 0x00F00000u
#define SDHCI_CMD_LVL 0x01000000u

#define SDHCI_POWER_ON 0x01u
#define SDHCI_POWER_300 0x0Cu
#define SDHCI_POWER_330 0x0Eu
#define SDHCI_CAN_VDD_330 0x01000000u
#define SDHCI_CAN_VDD_300 0x02000000u

#define SDHCI_CLOCK_CARD_EN 0x0004u
#define SDHCI_CLOCK_INT_STABLE 0x0002u
#define SDHCI_CLOCK_INT_EN 0x0001u

#define SDHCI_RESET_ALL 0x01u
#define SDHCI_RESET_CMD 0x02u
#define SDHCI_RESET_DATA 0x04u

#define SDHCI_INT_RESPONSE 0x00000001u
#define SDHCI_INT_DATA_END 0x00000002u
#define SDHCI_INT_DATA_AVAIL 0x00000020u
#define SDHCI_INT_ERROR 0x00008000u
#define SDHCI_INT_TIMEOUT 0x00010000u
#define SDHCI_INT_CRC 0x00020000u
#define SDHCI_INT_END_BIT 0x00040000u
#define SDHCI_INT_INDEX 0x00080000u
#define SDHCI_INT_DATA_TIMEOUT 0x00100000u
#define SDHCI_INT_DATA_CRC 0x00200000u
#define SDHCI_INT_DATA_END_BIT 0x00400000u
#define SDHCI_INT_CMD_ERRORS 0x000F0000u
#define SDHCI_INT_DATA_ERRORS 0x00700000u

#define SDHCI_CMD_RESP_NONE 0x00u
#define SDHCI_CMD_RESP_SHORT 0x02u
#define SDHCI_CMD_CRC 0x08u
#define SDHCI_CMD_INDEX 0x10u
#define SDHCI_CMD_DATA 0x20u

#define SDHCI_TRNS_READ 0x10u

#define SDIO_R5_ERRORS 0x0000CB00u
#define SDIO_R5_ERRORS_IOS80 0x0000FF00u
#define SDIO_R5_STATE_COMMAND 0x00001000u
#define SDIO_R6_ERRORS 0x0000E000u
#define SDIO_OCR_VDD_32_34 0x00300000u
#define SDIO_OCR_IOS80 0x00FFF000u
#define SDIO_CMD7_EXPECTED_STATUS 0x00001E00u

#define MAX_CIS_BYTES 256u
#define MAX_CIS_RETRIES 8u

#define WLAN_PROBE_EXTENDED_SSB 1

typedef struct
{
    u8 host_control;
    u8 power_control;
    u16 clock_control;
    u8 timeout_control;
    u32 int_enable;
    u32 signal_enable;
    bool valid;
} host_snapshot;

static volatile u8 *const sdio = (volatile u8 *)WLAN_SDIO_BASE;

/*
 * Hollywood is a big-endian host connected to a little-endian SDHCI block
 * through a 32-bit byte swapper.  These match Linux's sdhci_be32bs helpers.
 */
static inline u32 mmio_read32(u32 reg) { return *(volatile u32 *)(sdio + reg); }

static inline u16 mmio_read16(u32 reg)
{
    return *(volatile u16 *)(sdio + (reg ^ 2u));
}

static void mmio_write32(u32 reg, u32 value)
{
    *(volatile u32 *)(sdio + reg) = value;
    usleep(10); /* Hollywood requires at least 5 us after every write. */
}

static void mmio_write16(u32 reg, u16 value)
{
    const u32 base = reg & ~3u;
    const u32 shift = (reg & 2u) * 8u;
    u32 word = mmio_read32(base);

    word &= ~(0xFFFFu << shift);
    word |= (u32)value << shift;
    mmio_write32(base, word);
}

static void mmio_write8(u32 reg, u8 value)
{
    const u32 base = reg & ~3u;
    const u32 shift = (reg & 3u) * 8u;
    u32 word = mmio_read32(base);

    word &= ~(0xFFu << shift);
    word |= (u32)value << shift;
    mmio_write32(base, word);
}

static inline u8 mmio_read8(u32 reg)
{
    return *(volatile u8 *)(sdio + (reg ^ 3u));
}

static bool wait8_clear(u32 reg, u8 mask, unsigned int iterations,
                        useconds_t delay_us)
{
    while (iterations-- != 0u)
    {
        if ((mmio_read8(reg) & mask) == 0u)
        {
            return true;
        }
        usleep(delay_us);
    }
    return false;
}

static bool wait16_set(u32 reg, u16 mask, unsigned int iterations,
                       useconds_t delay_us)
{
    while (iterations-- != 0u)
    {
        if ((mmio_read16(reg) & mask) == mask)
        {
            return true;
        }
        usleep(delay_us);
    }
    return false;
}

static void reset_host_line(u8 line)
{
    mmio_write8(SDHCI_SOFTWARE_RESET, line);
    wait8_clear(SDHCI_SOFTWARE_RESET, line, 1000u, 100u);
}

static void take_snapshot(host_snapshot *snapshot)
{
    snapshot->host_control = mmio_read8(SDHCI_HOST_CONTROL);
    snapshot->power_control = mmio_read8(SDHCI_POWER_CONTROL);
    snapshot->clock_control = mmio_read16(SDHCI_CLOCK_CONTROL);
    snapshot->timeout_control = mmio_read8(SDHCI_TIMEOUT_CONTROL);
    snapshot->int_enable = mmio_read32(SDHCI_INT_ENABLE);
    snapshot->signal_enable = mmio_read32(SDHCI_SIGNAL_ENABLE);
    snapshot->valid = true;
}

static void restore_snapshot(const host_snapshot *snapshot)
{
    if (!snapshot->valid)
    {
        return;
    }

    mmio_write32(SDHCI_SIGNAL_ENABLE, 0u);
    mmio_write8(SDHCI_POWER_CONTROL, 0u);
    mmio_write8(SDHCI_SOFTWARE_RESET, SDHCI_RESET_ALL);
    wait8_clear(SDHCI_SOFTWARE_RESET, SDHCI_RESET_ALL, 1000u, 100u);
    mmio_write8(SDHCI_HOST_CONTROL, snapshot->host_control);
    mmio_write8(SDHCI_TIMEOUT_CONTROL, snapshot->timeout_control);
    mmio_write16(SDHCI_CLOCK_CONTROL, snapshot->clock_control);
    mmio_write8(SDHCI_POWER_CONTROL, snapshot->power_control);
    mmio_write32(SDHCI_INT_ENABLE, snapshot->int_enable);
    mmio_write32(SDHCI_SIGNAL_ENABLE, snapshot->signal_enable);
}

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

static command_result send_command(u8 index, u32 argument, u8 flags)
{
    command_result result = {0u, 0u, false};
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

static bool configure_clock_divider(u16 encoded)
{
    mmio_write16(SDHCI_CLOCK_CONTROL, 0u);
    mmio_write16(SDHCI_CLOCK_CONTROL, encoded | SDHCI_CLOCK_INT_EN);
    if (!wait16_set(SDHCI_CLOCK_CONTROL, SDHCI_CLOCK_INT_STABLE, 1000u, 100u))
    {
        return false;
    }
    mmio_write16(SDHCI_CLOCK_CONTROL,
                 encoded | SDHCI_CLOCK_INT_EN | SDHCI_CLOCK_CARD_EN);
    usleep(2);
    return true;
}

static bool cmd52_read(u8 function, u32 address, u8 *value,
                       command_result *last)
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

static bool cmd52_write(u8 function, u32 address, u8 value,
                        command_result *last)
{
    const u32 argument = 0x80000000u | ((u32)(function & 7u) << 28) |
                         ((address & 0x1FFFFu) << 9) | value;
    command_result command = send_command(
        52u, argument, SDHCI_CMD_RESP_SHORT | SDHCI_CMD_CRC | SDHCI_CMD_INDEX);

    *last = command;
    return command.complete && (command.response & SDIO_R5_ERRORS) == 0u;
}

static bool cmd53_read_pio(u8 function, u32 address, u16 count, u32 *value,
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

static bool ssb_set_window(u32 address, command_result *last)
{
    return cmd52_write(1u, 0x1000Au, (u8)((address >> 8) & 0x80u), last) &&
           cmd52_write(1u, 0x1000Bu, (u8)(address >> 16), last) &&
           cmd52_write(1u, 0x1000Cu, (u8)(address >> 24), last);
}

static bool ssb_scan_read32(u8 core_index, u16 offset, u32 *value)
{
    const u32 backplane_offset = (u32)core_index * 0x1000u + offset;
    const u32 sdio_address = 0x8000u | backplane_offset;
    u32 status = 0u;
    u32 response = 0u;

    if (backplane_offset >= 0x8000u)
    {
        return false;
    }

    return cmd53_read_pio(1u, sdio_address, 4u, value, &status, &response);
}

static void run_ssb_probe(probe_result *result)
{
    ssb_probe_result *ssb = &result->ssb;
    u8 count;
    u8 index;

    ssb->attempted = true;
    ssb->window_ok = ssb_set_window(0x18000000u, &result->cmd52_last);
    if (!ssb->window_ok ||
        !ssb_scan_read32(0u, 0x0FFCu, &ssb->chipcommon_idhigh))
    {
        return;
    }
    ssb->chipcommon_ok =
        ssb->chipcommon_idhigh != 0u && ssb->chipcommon_idhigh != 0xFFFFFFFFu;
    if (!ssb->chipcommon_ok ||
        !ssb_scan_read32(0u, 0x0000u, &ssb->chip_id_register))
    {
        return;
    }

    ssb->reported_core_count = (u8)(ssb->chip_id_register >> 24) & 0x0Fu;
    count = ssb->reported_core_count;
    if (count == 0u || count > WLAN_PROBE_MAX_SSB_CORES)
    {
        count = WLAN_PROBE_MAX_SSB_CORES;
    }
    for (index = 0u; index < count; ++index)
    {
        if (!ssb_scan_read32(index, 0x0FFCu, &ssb->core_idhigh[index]))
        {
            break;
        }
        ++ssb->cores_read;
    }
}

static bool read_cis_pointer(u32 base, u32 *pointer, command_result *last)
{
    u8 lo;
    u8 mid;
    u8 hi;

    if (!cmd52_read(0u, base + 0x09u, &lo, last) ||
        !cmd52_read(0u, base + 0x0Au, &mid, last) ||
        !cmd52_read(0u, base + 0x0Bu, &hi, last))
    {
        return false;
    }
    *pointer = (u32)lo | ((u32)mid << 8) | ((u32)hi << 16);
    return true;
}

// retry cis reads with budget
static bool cis_read_byte(u32 address, u8 *value, probe_result *result,
                          unsigned int *budget)
{
    do
    {
        if (cmd52_read(0u, address, value, &result->cmd52_last))
        {
            return true;
        }
        if (*budget == 0u)
        {
            return false;
        }
        --*budget;
    } while (*budget > 0);

    return false;
}

static bool scan_cis(u32 pointer, probe_result *result)
{
    unsigned int budget = MAX_CIS_RETRIES;
    u32 consumed = 0u;

    if (pointer == 0u || pointer > 0x1FFFFu)
    {
        return false;
    }

    while (consumed < MAX_CIS_BYTES && pointer <= 0x1FFFFu)
    {
        u8 code;
        u8 length;
        u8 data[4] = {0u, 0u, 0u, 0u};
        unsigned int i;

        if (!cis_read_byte(pointer++, &code, result, &budget))
        {
            return false;
        }
        ++consumed;
        if (code == 0xFFu)
        {
            return true;
        }
        if (code == 0x00u)
        {
            continue;
        }
        if (!cis_read_byte(pointer++, &length, result, &budget))
        {
            return false;
        }
        ++consumed;
        if ((u32)length > MAX_CIS_BYTES - consumed ||
            pointer + (u32)length > 0x20000u)
        {
            return false;
        }

        for (i = 0; i < (unsigned int)length; ++i)
        {
            u8 byte;
            if (!cis_read_byte(pointer + i, &byte, result, &budget))
            {
                return false;
            }
            if (i < sizeof(data))
            {
                data[i] = byte;
            }
        }
        if (code == 0x20u && length >= 4u)
        { /* CISTPL_MANFID */
            result->manufacturer = (u16)data[0] | ((u16)data[1] << 8);
            result->product = (u16)data[2] | ((u16)data[3] << 8);
            result->manfid_found = true;
        }
        pointer += length;
        consumed += length;
    }
    return true;
}

bool wlan_command_has_signal_error(const command_result *command)
{
    return (command->status &
            (SDHCI_INT_CRC | SDHCI_INT_END_BIT | SDHCI_INT_INDEX)) != 0u;
}

static bool run_bringup_attempt(bringup_attempt *attempt, u8 power_value)
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

void wlan_probe_run(probe_result *result)
{
    s32 ios_version;
    u32 ahbprot;
    host_snapshot snapshot = {0};
    static const struct
    {
        const char *name;
        u16 divider;
        bool write_power;
        bool send_cmd0;
    } configurations[WLAN_PROBE_MAX_BRINGUP_ATTEMPTS] = {
        {"ios-exact", 0x0100u, true, false},
        {"ios-retry", 0x0100u, true, false}};
    u8 power_value;
    unsigned int attempt;

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
    result->winning_attempt = -1;
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

    for (attempt = 0u; attempt < WLAN_PROBE_MAX_BRINGUP_ATTEMPTS; ++attempt)
    {
        bringup_attempt *current = &result->bringup[attempt];

        if (attempt != 0u && (mmio_read16(SDHCI_CLOCK_CONTROL) &
                              (SDHCI_CLOCK_INT_EN | SDHCI_CLOCK_CARD_EN)) ==
                                 (SDHCI_CLOCK_INT_EN | SDHCI_CLOCK_CARD_EN))
        {
            command_result retry_reset = {0u, 0u, false};
            cmd52_write(0u, 0x06u, 0x08u, &retry_reset);
        }

        current->name = configurations[attempt].name;
        current->clock_divider = configurations[attempt].divider;
        current->write_power = configurations[attempt].write_power;
        current->send_cmd0 = configurations[attempt].send_cmd0;
        ++result->bringup_count;
        if (run_bringup_attempt(current, power_value))
        {
            result->winning_attempt = (s8)attempt;
            result->reset_ok = current->reset_ok;
            result->power_ok = current->power_ok;
            result->clock_ok = current->clock_ok;
            result->cmd0_attempted = current->send_cmd0;
            result->cmd0 = current->cmd0;
            result->cmd5 = current->cmd5;
            result->cmd3 = current->cmd3;
            result->rca = current->rca;
            result->cmd7 = current->cmd7;
            break;
        }
    }
    if (result->winning_attempt < 0)
    {
        if (result->bringup_count != 0u)
        {
            const bringup_attempt *last =
                &result->bringup[result->bringup_count - 1u];
            result->reset_ok = last->reset_ok;
            result->power_ok = last->power_ok;
            result->clock_ok = last->clock_ok;
            result->cmd0_attempted = last->send_cmd0;
            result->cmd0 = last->cmd0;
            result->cmd5 = last->cmd5;
            result->cmd3 = last->cmd3;
            result->rca = last->rca;
            result->cmd7 = last->cmd7;
        }
        goto restore_host;
    }

    result->ocr = result->cmd5.response;

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

    if (result->cccr_ok)
    {
        result->data_path.attempted = true;
        result->data_path.original_io_enable = result->io_enable;
        result->data_path.original_bus_interface = result->bus_interface;

        result->data_path.enable_ok = cmd52_write(
            0u, 0x02u, result->io_enable | 0x02u, &result->cmd52_last);

        if (result->data_path.enable_ok)
        {
            for (attempt = 0u; attempt < 100u; ++attempt)
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
        }

        if (result->data_path.ready_ok)
        {
            // one bit mode preserving unrelated CCCR
            result->data_path.bus_width_ok =
                cmd52_write(0u, 0x07u, result->bus_interface & (u8)0xFCu,
                            &result->cmd52_last);

            if (result->data_path.bus_width_ok)
            {
                mmio_write8(SDHCI_HOST_CONTROL,
                            mmio_read8(SDHCI_HOST_CONTROL) & (u8)~0x02u);
            }
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

        if (WLAN_PROBE_EXTENDED_SSB != 0 && result->data_path.read_ok)
        {
            run_ssb_probe(result);
        }

        command_result restore_command = {0u, 0u, false};
        cmd52_write(0u, 0x02u, result->data_path.original_io_enable,
                    &restore_command);
        cmd52_write(0u, 0x07u, result->data_path.original_bus_interface,
                    &restore_command);
    }

restore_host:
    restore_snapshot(&snapshot);
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
