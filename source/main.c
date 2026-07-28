// SPDX-License-Identifier: MIT

#include <gccore.h>
#include <ogc/ios.h>
#include <stdio.h>
#include <string.h>
#include <wiiuse/wpad.h>

#include "app.h"
#include "types.h"
#include "presentation.h"
#include "probe.h"
#include "report.h"

static void init_video(void)
{
    GXRModeObj *mode;
    void *framebuffer;

    VIDEO_Init();
    mode = VIDEO_GetPreferredMode(NULL);
    framebuffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
    console_init(framebuffer, 20, 20, mode->fbWidth, mode->xfbHeight,
                 mode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(mode);
    VIDEO_SetNextFramebuffer(framebuffer);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if ((mode->viTVMode & VI_NON_INTERLACE) != 0u)
    {
        VIDEO_WaitVSync();
    }
}

static void print_intro(const probe_result *result)
{
    printf("\x1b[2J\x1b[H");
    printf(APP_NAME " " APP_VERSION "\n\n");
    printf("This performs transient SDIO reset/enumeration and read-only\n");
    printf("CCCR/CIS reads. It does NOT write NAND, install IOS, load WLAN\n");
    printf("firmware, use the IOS WLAN driver, or alter network settings.\n\n");

    if (result->ahbprot != HW_AHBPROT_ENABLED)
    {
        printf("AHB access missing (%08lX). Active probe is disabled.\n",
               (unsigned long)result->ahbprot);
        printf("Use the supplied meta.xml and launch directly from HBC.\n");
        printf("\nHOME: return\n");
        return;
    }

    printf("AHB access OK.\n");
    printf("Press A to run the test\n");
    printf("Press HOME to leave without touching SDIO1.\n");
}

static u32 wait_for_button(u32 buttons)
{
    for (;;)
    {
        u32 pressed;

        WPAD_ScanPads();
        pressed = WPAD_ButtonsDown(0);
        if ((pressed & buttons) != 0u)
        {
            return pressed;
        }
        VIDEO_WaitVSync();
    }
}

int main(void)
{
    probe_result probe;
    u32 pressed;

    memset(&probe, 0, sizeof(probe));
    init_video();
    WPAD_Init();

    probe.ios_version = IOS_GetVersion();
    probe.ahbprot = *(volatile u32 *)HW_AHBPROT_ADDRESS;
    print_intro(&probe);

    pressed = wait_for_button(WPAD_BUTTON_A | WPAD_BUTTON_HOME);
    if ((pressed & WPAD_BUTTON_HOME) != 0u ||
        probe.ahbprot != HW_AHBPROT_ENABLED)
    {
        return 0;
    }

    printf("\nTesting...\n");
    wlan_probe_run(&probe);
    presentation_print_results(&probe);

    printf("\nPress 1 to save the report\n");
    printf("Press HOME to return to the Homebrew Channel.\n");
    pressed = wait_for_button(WPAD_BUTTON_1 | WPAD_BUTTON_HOME);

    if ((pressed & WPAD_BUTTON_1) != 0)
    {
        const report_save_result saved = report_save(&probe);

        if (saved.status == REPORT_SAVE_OK)
        {
            printf("Report saved: %s\n", saved.path);
        }
        else
        {
            printf("Report save failed: %s.\n",
                   report_save_status_label(saved.status));
        }

        printf("\nPress HOME to return to the Homebrew Channel.\n");
        wait_for_button(WPAD_BUTTON_HOME);
    }

    return 0;
}
