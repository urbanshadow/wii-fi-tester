// SPDX-License-Identifier: MIT

#ifndef WLAN_PROBE_SSB_H
#define WLAN_PROBE_SSB_H

#include "types.h"

bool ssb_set_window(u32 address, command_result *last);
void run_ssb_probe(probe_result *result);

#endif
