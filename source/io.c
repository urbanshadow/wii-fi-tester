// SPDX-License-Identifier: MIT

#include <unistd.h>

#include "constants.h"
#include "io.h"

static volatile u8 *const sdio = (volatile u8 *)WLAN_SDIO_BASE;

u32 mmio_read32(u32 reg) { return *(volatile u32 *)(sdio + reg); }

u16 mmio_read16(u32 reg) { return *(volatile u16 *)(sdio + (reg ^ 2u)); }

u8 mmio_read8(u32 reg) { return *(volatile u8 *)(sdio + (reg ^ 3u)); }

void mmio_write32(u32 reg, u32 value)
{
    *(volatile u32 *)(sdio + reg) = value;
    usleep(10); /* Hollywood requires at least 5 us after every write. */
}

void mmio_write16(u32 reg, u16 value)
{
    const u32 base = reg & ~3u;
    const u32 shift = (reg & 2u) * 8u;
    u32 word = mmio_read32(base);

    word &= ~(0xFFFFu << shift);
    word |= (u32)value << shift;
    mmio_write32(base, word);
}

void mmio_write8(u32 reg, u8 value)
{
    const u32 base = reg & ~3u;
    const u32 shift = (reg & 3u) * 8u;
    u32 word = mmio_read32(base);

    word &= ~(0xFFu << shift);
    word |= (u32)value << shift;
    mmio_write32(base, word);
}

bool wait16_set(u32 reg, u16 mask, unsigned int iterations, useconds_t delay_us)
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

bool wait8_clear(u32 reg, u8 mask, unsigned int iterations, useconds_t delay_us)
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
