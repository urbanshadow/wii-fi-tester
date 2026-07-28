// SPDX-License-Identifier: MIT

#include "sdio.h"
#include "ssb.h"

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

bool ssb_set_window(u32 address, command_result *last)
{
    return cmd52_write(1u, 0x1000Au, (u8)((address >> 8) & 0x80u), last) &&
           cmd52_write(1u, 0x1000Bu, (u8)(address >> 16), last) &&
           cmd52_write(1u, 0x1000Cu, (u8)(address >> 24), last);
}

void run_ssb_probe(probe_result *result)
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
