// SPDX-License-Identifier: MIT

#include "app.h"
#include "diagnosis.h"

probe_diagnosis probe_classify(const probe_result *result)
{
    if (result->ahbprot != HW_AHBPROT_ENABLED)
    {
        return PROBE_DIAGNOSIS_AHB_MISSING;
    }
    if (!result->controller_plausible)
    {
        return PROBE_DIAGNOSIS_CONTROLLER_UNREADABLE;
    }
    if (!result->reset_ok || !result->power_ok || !result->clock_ok)
    {
        return PROBE_DIAGNOSIS_HOST_SETUP_FAILED;
    }
    if (!result->cmd5.complete)
    {
        return wlan_command_has_signal_error(&result->cmd5)
                   ? PROBE_DIAGNOSIS_CMD5_SIGNAL_ERROR
                   : PROBE_DIAGNOSIS_CMD5_NO_RESPONSE;
    }
    if ((result->ocr & WLAN_SDIO_OCR_READY) == 0u)
    {
        return PROBE_DIAGNOSIS_CARD_NOT_READY;
    }
    if (!result->cmd3.complete || !result->cmd7.complete)
    {
        return PROBE_DIAGNOSIS_ENUMERATION_FAILED;
    }
    if (!result->cccr_ok)
    {
        return PROBE_DIAGNOSIS_CCCR_FAILED;
    }
    if (!result->cis_readable)
    {
        return PROBE_DIAGNOSIS_CIS_FAILED;
    }
    if (!result->data_path.enable_ok || !result->data_path.ready_ok)
    {
        return PROBE_DIAGNOSIS_FUNCTION_START_FAILED;
    }
    if (!result->data_path.window_ok)
    {
        return PROBE_DIAGNOSIS_SSB_WINDOW_FAILED;
    }
    if (!result->data_path.read_ok)
    {
        return PROBE_DIAGNOSIS_DATA_TRANSFER_FAILED;
    }
    if (!result->ssb.chipcommon_ok)
    {
        return PROBE_DIAGNOSIS_SSB_IDENTITY_INVALID;
    }
    return PROBE_DIAGNOSIS_PASSED;
}

const char *probe_diagnosis_code(probe_diagnosis diagnosis)
{
    static const char *const codes[] = {
        "ahb-missing",          "controller-unreadable",
        "host-setup-failed",    "cmd5-signal-error",
        "cmd5-no-response",     "card-not-ready",
        "enumeration-failed",   "cccr-failed",
        "cis-failed",           "function-start-failed",
        "ssb-window-failed",    "data-transfer-failed",
        "ssb-identity-invalid", "passed"};

    if ((unsigned int)diagnosis >= sizeof(codes) / sizeof(codes[0]))
    {
        return "unknown";
    }
    return codes[diagnosis];
}
