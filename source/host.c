// SPDX-License-Identifier: MIT

#include <gccore.h>
#include <unistd.h>
#include "constants.h"
#include "io.h"
#include "host.h"

void reset_host_line(u8 line)
{
    mmio_write8(SDHCI_SOFTWARE_RESET, line);
    wait8_clear(SDHCI_SOFTWARE_RESET, line, 1000u, 100u);
}

void take_snapshot(host_snapshot *snapshot)
{
    snapshot->host_control = mmio_read8(SDHCI_HOST_CONTROL);
    snapshot->power_control = mmio_read8(SDHCI_POWER_CONTROL);
    snapshot->clock_control = mmio_read16(SDHCI_CLOCK_CONTROL);
    snapshot->timeout_control = mmio_read8(SDHCI_TIMEOUT_CONTROL);
    snapshot->int_enable = mmio_read32(SDHCI_INT_ENABLE);
    snapshot->signal_enable = mmio_read32(SDHCI_SIGNAL_ENABLE);
    snapshot->valid = true;
}

void restore_snapshot(const host_snapshot *snapshot)
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

bool configure_clock_divider(u16 encoded)
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
