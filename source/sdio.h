// SPDX-License-Identifier: MIT

#ifndef WLAN_PROBE_SDIO_H
#define WLAN_PROBE_SDIO_H

#include "types.h"

command_result send_command(u8 index, u32 argument, u8 flags);
bool cmd52_read(u8 function, u32 address, u8 *value, command_result *last);
bool cmd52_write(u8 function, u32 address, u8 value, command_result *last);
bool cmd53_read_pio(u8 function, u32 address, u16 count, u32 *value,
                    u32 *status, u32 *response);
bool run_bringup_attempt(bringup_attempt *attempt, u8 power_value);

#endif
