// SPDX-License-Identifier: MIT

#ifndef WLAN_PROBE_IO_H
#define WLAN_PROBE_IO_H

#include <gccore.h>
#include <unistd.h>

u8 mmio_read8(u32 reg);
u16 mmio_read16(u32 reg);
u32 mmio_read32(u32 reg);

void mmio_write8(u32 reg, u8 value);
void mmio_write16(u32 reg, u16 value);
void mmio_write32(u32 reg, u32 value);

bool wait16_set(u32 reg, u16 mask, unsigned int iterations,
                useconds_t delay_us);
bool wait8_clear(u32 reg, u8 mask, unsigned int iterations,
                 useconds_t delay_us);

#endif
