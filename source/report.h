// SPDX-License-Identifier: MIT

#ifndef WLAN_PROBE_REPORT_H
#define WLAN_PROBE_REPORT_H

#include "types.h"

typedef enum
{
    REPORT_SAVE_OK,
    REPORT_SAVE_NO_FAT_VOLUME,
    REPORT_SAVE_OPEN_FAILED,
    REPORT_SAVE_WRITE_FAILED
} report_save_status;

typedef struct
{
    report_save_status status;
    const char *path;
} report_save_result;

report_save_result report_save(const probe_result *probe);
const char *report_save_status_label(report_save_status status);

#endif
