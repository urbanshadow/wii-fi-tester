// SPDX-License-Identifier: MIT

#ifndef WLAN_PROBE_TYPES_H
#define WLAN_PROBE_TYPES_H

#include <gccore.h>

#include "constants.h"

typedef struct
{
    u32 status;
    u32 response;
    u32 argument;
    u8 index;
    bool complete;
} command_result;

typedef struct
{
    bool attempted;
    bool enable_ok;
    bool ready_ok;
    bool bus_width_ok;
    bool window_ok;
    bool read_ok;
    u8 original_io_enable;
    u8 original_bus_interface;
    u8 ready_value;
    u32 read_value;
    u32 status;
    u32 response;
} data_path_result;

typedef struct
{
    bool attempted;
    bool window_ok;
    bool chipcommon_ok;
    u32 chipcommon_idhigh;
    u32 chip_id_register;
    u8 reported_core_count;
    u8 cores_read;
    u32 core_idhigh[WLAN_PROBE_MAX_SSB_CORES];
} ssb_probe_result;

typedef struct
{
    const char *name;
    u16 clock_divider;
    bool write_power;
    bool send_cmd0;
    bool reset_ok;
    bool power_ok;
    bool clock_ok;
    u8 power_before;
    u8 power_after;
    u16 clock_after;
    u32 present_after;
    command_result cmd0;
    command_result cmd3;
    command_result cmd5_inquiry;
    command_result cmd5;
    unsigned int cmd5_command_count;
    command_result cmd7;
    u16 rca;
} bringup_attempt;

typedef enum
{
    INITIALIZATION_STAGE_NOT_STARTED,
    INITIALIZATION_STAGE_BRINGUP,
    INITIALIZATION_STAGE_CCCR,
    INITIALIZATION_STAGE_FUNCTION_ENABLE,
    INITIALIZATION_STAGE_FUNCTION_READY,
    INITIALIZATION_STAGE_BUS_WIDTH,
    INITIALIZATION_STAGE_COMPLETE
} initialization_stage;

typedef struct
{
    bringup_attempt bringup;
    initialization_stage stage;
    command_result cmd52_last;
    bool complete;
} initialization_attempt;

typedef struct
{
    s32 ios_version;
    u32 ahbprot;
    u16 host_version;
    u32 capabilities;
    u32 capabilities_1;
    u32 present_before;
    u8 host_control_before;
    u8 power_control_before;
    u16 clock_control_before;
    bool controller_plausible;
    bool reset_ok;
    bool power_ok;
    bool clock_ok;
    bool cmd0_attempted;
    bool pre_reset_attempted;
    command_result pre_reset;
    command_result cmd0;
    command_result cmd5;
    command_result cmd3;
    command_result cmd7;
    command_result cmd52_last;
    u32 ocr;
    u16 rca;
    u8 cccr_revision;
    u8 sdio_revision;
    u8 io_enable;
    u8 io_ready;
    u8 bus_interface;
    u8 card_capabilities;
    u32 common_cis;
    u32 function1_cis;
    u16 manufacturer;
    u16 product;
    bool cccr_ok;
    bool cis_readable;
    bool manfid_found;
    u8 initialization_count;
    s8 winning_initialization;
    initialization_attempt init[WLAN_PROBE_MAX_INITIALIZATION_ATTEMPTS];
    data_path_result data_path;
    ssb_probe_result ssb;
} probe_result;

#endif
