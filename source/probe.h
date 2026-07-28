// SPDX-License-Identifier: MIT

#ifndef WLAN_PROBE_H
#define WLAN_PROBE_H

#include "types.h"

const char *wlan_command_label(const command_result *command);
bool wlan_command_has_signal_error(const command_result *command);
bool wlan_probe_command_line_high(const probe_result *result);
u8 wlan_probe_data_line_levels(const probe_result *result);
bool wlan_probe_card_detect_set(const probe_result *result);

void wlan_probe_run(probe_result *result);

#endif
