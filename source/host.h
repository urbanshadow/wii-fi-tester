// SPDX-License-Identifier: MIT

#ifndef WLAN_PROBE_HOST_H
#define WLAN_PROBE_HOST_H

#include <gccore.h>

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

void reset_host_line(u8 line);
void take_snapshot(host_snapshot *snapshot);
void restore_snapshot(const host_snapshot *snapshot);
bool configure_clock_divider(u16 encoded);

#endif
