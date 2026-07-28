// SPDX-License-Identifier: MIT

#ifndef WLAN_PROBE_CIS_H
#define WLAN_PROBE_CIS_H

#include "types.h"

bool read_cis_pointer(u32 base, u32 *pointer, command_result *last);
bool scan_cis(u32 pointer, probe_result *result);

#endif
