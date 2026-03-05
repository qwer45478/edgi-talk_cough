/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-02-26     edgi-talk    Cough/snore detection project entry
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>

#define DBG_TAG    "main"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

/* Cough detection module */
extern int cough_detect_init(void);
extern void cough_ui_init(void);
extern rt_err_t cough_ui_wait_ready(rt_int32_t timeout);

#define UI_INIT_TIMEOUT_MS  5000

/*****************************************************************************
 * Main Entry (Cortex-M55 core)
 *****************************************************************************/
int main(void)
{
    LOG_I("Cough/Snore Detection System starting...");

    cough_ui_init();
    if (cough_ui_wait_ready(rt_tick_from_millisecond(UI_INIT_TIMEOUT_MS)) != RT_EOK)
    {
        LOG_W("UI initialization timeout");
    }

    /* Initialize the cough detection subsystem:
     *   - Registers the button IRQ
     *   - Initializes the microphone
     *   - Starts the audio processing thread
     *   - Starts the Edge Impulse inference thread
     */
    if (cough_detect_init() != 0)
    {
        LOG_E("Failed to initialize cough detection!");
        return -1;
    }

    LOG_I("System ready. Press the button to calibrate noise baseline.");
    return 0;
}

