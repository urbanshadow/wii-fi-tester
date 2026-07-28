// SPDX-License-Identifier: MIT

#include "cis.h"
#include "sdio.h"

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

bool read_cis_pointer(u32 base, u32 *pointer, command_result *last)
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

bool scan_cis(u32 pointer, probe_result *result)
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
