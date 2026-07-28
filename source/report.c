// SPDX-License-Identifier: MIT

#include <gccore.h>
#include <fat.h>
#include <stdio.h>

#include "app.h"
#include "types.h"
#include "diagnosis.h"
#include "probe.h"
#include "report.h"

static void write_command(FILE *file, const char *name,
                          const command_result *command)
{
    fprintf(file,
            "%s index=%u argument=%08lX status=%08lX response=%08lX "
            "complete=%u\n",
            name, (unsigned int)command->index,
            (unsigned long)command->argument,
            (unsigned long)command->status,
            (unsigned long)command->response, command->complete);
}

static void write_report(FILE *file, const probe_result *result)
{
    unsigned int i;

    fprintf(file, APP_NAME " " APP_VERSION "\n");
    fprintf(file, "IOS=%ld AHBPROT=%08lX\n", (long)result->ios_version,
            (unsigned long)result->ahbprot);
    fprintf(file, "diagnosis=%s\n",
            probe_diagnosis_code(probe_classify(result)));
    fprintf(file, "host_version=%04X caps=%08lX caps1=%08lX present=%08lX\n",
            result->host_version, (unsigned long)result->capabilities,
            (unsigned long)result->capabilities_1,
            (unsigned long)result->present_before);
    fprintf(file, "host_before control=%02X power=%02X clock=%04X\n",
            result->host_control_before, result->power_control_before,
            result->clock_control_before);
    fprintf(
        file,
        "pre_reset attempted=%u status=%08lX response=%08lX complete=%u\n",
        result->pre_reset_attempted, (unsigned long)result->pre_reset.status,
        (unsigned long)result->pre_reset.response, result->pre_reset.complete);
    fprintf(file, "lines cmd=%u dat=%X reset=%u power=%u clock=%u\n",
            wlan_probe_command_line_high(result),
            wlan_probe_data_line_levels(result), result->reset_ok,
            result->power_ok, result->clock_ok);
    write_command(file, "cmd0", &result->cmd0);
    fprintf(file, "bringup_count=%u winner=%d cmd0_attempted=%u\n",
            result->bringup_count, (int)result->winning_attempt,
            result->cmd0_attempted);
    for (i = 0u; i < result->bringup_count; ++i)
    {
        const bringup_attempt *entry = &result->bringup[i];

        fprintf(file,
                "bringup[%u] name=%s divider=%04X write_power=%u cmd0=%u "
                "reset=%u power_ok=%u clock_ok=%u power=%02X->%02X "
                "clock=%04X present=%08lX cmd0_status=%08lX "
                "cmd5_status=%08lX cmd5_response=%08lX complete=%u\n",
                i, entry->name, entry->clock_divider, entry->write_power,
                entry->send_cmd0, entry->reset_ok, entry->power_ok,
                entry->clock_ok, entry->power_before, entry->power_after,
                entry->clock_after, (unsigned long)entry->present_after,
                (unsigned long)entry->cmd0.status,
                (unsigned long)entry->cmd5.status,
                (unsigned long)entry->cmd5.response, entry->cmd5.complete);
        fprintf(file,
                "bringup[%u]_cmd5 inquiry_status=%08lX "
                "inquiry_response=%08lX inquiry_complete=%u commands=%u\n",
                i, (unsigned long)entry->cmd5_inquiry.status,
                (unsigned long)entry->cmd5_inquiry.response,
                entry->cmd5_inquiry.complete, entry->cmd5_command_count);
        fprintf(file,
                "bringup[%u]_select cmd3_status=%08lX cmd3_response=%08lX "
                "rca=%04X cmd7_status=%08lX cmd7_response=%08lX\n",
                i, (unsigned long)entry->cmd3.status,
                (unsigned long)entry->cmd3.response, entry->rca,
                (unsigned long)entry->cmd7.status,
                (unsigned long)entry->cmd7.response);
    }
    fprintf(file, "cmd5 status=%08lX response=%08lX complete=%u ocr=%08lX\n",
            (unsigned long)result->cmd5.status,
            (unsigned long)result->cmd5.response, result->cmd5.complete,
            (unsigned long)result->ocr);
    fprintf(file, "cmd3 status=%08lX response=%08lX complete=%u rca=%04X\n",
            (unsigned long)result->cmd3.status,
            (unsigned long)result->cmd3.response, result->cmd3.complete,
            result->rca);
    write_command(file, "cmd7", &result->cmd7);
    write_command(file, "cmd52", &result->cmd52_last);
    fprintf(
        file,
        "cccr_ok=%u cccr=%02X sdio=%02X ioe=%02X ior=%02X bus=%02X caps=%02X\n",
        result->cccr_ok, result->cccr_revision, result->sdio_revision,
        result->io_enable, result->io_ready, result->bus_interface,
        result->card_capabilities);
    fprintf(file,
            "cis_common=%06lX cis_f1=%06lX cis_readable=%u manfid=%04X:%04X "
            "found=%u\n",
            (unsigned long)result->common_cis,
            (unsigned long)result->function1_cis, result->cis_readable,
            result->manufacturer, result->product, result->manfid_found);
    fprintf(
        file,
        "dat0 attempted=%u bus_width=%u original_bus=%02X enable=%u "
        "ready=%u window=%u read=%u value=%08lX ready_value=%02X "
        "status=%08lX response=%08lX\n",
        result->data_path.attempted, result->data_path.bus_width_ok,
        result->data_path.original_bus_interface, result->data_path.enable_ok,
        result->data_path.ready_ok, result->data_path.window_ok,
        result->data_path.read_ok, (unsigned long)result->data_path.read_value,
        result->data_path.ready_value, (unsigned long)result->data_path.status,
        (unsigned long)result->data_path.response);
    fprintf(file,
            "ssb attempted=%u window=%u chipcommon=%u idhigh=%08lX "
            "chipid_reg=%08lX reported_cores=%u cores_read=%u\n",
            result->ssb.attempted, result->ssb.window_ok,
            result->ssb.chipcommon_ok,
            (unsigned long)result->ssb.chipcommon_idhigh,
            (unsigned long)result->ssb.chip_id_register,
            result->ssb.reported_core_count, result->ssb.cores_read);
    for (i = 0u; i < result->ssb.cores_read; ++i)
    {
        const u32 idhigh = result->ssb.core_idhigh[i];
        const u32 core_code = (idhigh & 0x00008FF0u) >> 4;
        const u32 revision = (idhigh & 0xFu) | ((idhigh & 0x7000u) >> 8);

        fprintf(file,
                "ssb_core[%u] idhigh=%08lX vendor=%04lX core=%03lX rev=%02lX\n",
                i, (unsigned long)idhigh, (unsigned long)(idhigh >> 16),
                (unsigned long)core_code, (unsigned long)revision);
    }
}

report_save_result report_save(const probe_result *probe)
{
    static const char *const paths[] = {"sd:/" APP_REPORT_FILENAME,
                                        "usb:/" APP_REPORT_FILENAME};
    report_save_result result = {REPORT_SAVE_NO_FAT_VOLUME, NULL};
    unsigned int i;

    if (!fatInitDefault())
    {
        return result;
    }

    result.status = REPORT_SAVE_OPEN_FAILED;
    for (i = 0u; i < sizeof(paths) / sizeof(paths[0]); ++i)
    {
        bool write_failed;
        FILE *file = fopen(paths[i], "w");

        if (file == NULL)
        {
            continue;
        }
        write_report(file, probe);
        write_failed = ferror(file) != 0;
        if (fclose(file) != 0)
        {
            write_failed = true;
        }
        if (write_failed)
        {
            result.status = REPORT_SAVE_WRITE_FAILED;
            continue;
        }
        result.status = REPORT_SAVE_OK;
        result.path = paths[i];
        return result;
    }
    return result;
}

const char *report_save_status_label(report_save_status status)
{
    static const char *const labels[] = {"OK", "no FAT volume mounted",
                                         "could not open report file",
                                         "report write/flush failed"};

    if ((unsigned int)status >= sizeof(labels) / sizeof(labels[0]))
    {
        return "unknown error";
    }
    return labels[status];
}
