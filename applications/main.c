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

#include "common/app_common.h"
#include "common/common_env.h"
#include "common/common_storage.h"
#include "cough_detect/cough_detect.h"
#include "cough_ui/cough_ui.h"

#define DBG_TAG    "main"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

/* Cough detection module — header included above */

#define UI_INIT_TIMEOUT_MS  5000
#define ENV_SAMPLE_INTERVAL_MS  30000   /* read AHT20 every 30s */
#define UI_REFRESH_INTERVAL_MS  5000    /* push env/stats to UI every 5s */

/* ── Periodic UI refresh timer ──────────────────────────────────── */
static struct rt_timer s_ui_refresh_timer;

static void ui_refresh_callback(void *param)
{
    (void)param;
    /* Only send event — actual work (env I2C read, snprintf, mq send)
     * runs in the control thread to avoid timer stack overflow. */
    cough_detect_send_event(CD_EVENT_UI_REFRESH);
}

/*****************************************************************************
 * Main Entry (Cortex-M55 core)
 *****************************************************************************/
int main(void)
{
    LOG_I("Cough/Snore Detection System starting...");

    app_common_init(APP_COMMON_INIT_BASE);
    app_common_dump_status();

    /* Ensure storage directories exist */
    common_storage_ensure_dirs();

    /* Start periodic environment sampling */
    common_env_start_periodic(ENV_SAMPLE_INTERVAL_MS);

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

    /* Start periodic UI refresh (env + stats → display) */
    rt_timer_init(&s_ui_refresh_timer, "ui_rfsh",
                  ui_refresh_callback, RT_NULL,
                  rt_tick_from_millisecond(UI_REFRESH_INTERVAL_MS),
                  RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);
    rt_timer_start(&s_ui_refresh_timer);

    LOG_I("System ready. Press the button to calibrate noise baseline.");
    return 0;
}

