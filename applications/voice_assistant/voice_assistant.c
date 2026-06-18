/*
 * voice_assistant.c — 小智语音助手封装实现
 *
 * 功能：封装官方SDK的初始化，集成到主程序
 *
 * @yyc add
 */

#include "voice_assistant.h"
#include "../common/common_audio_capture.h"
#include "xiaozhi/xiaozhi.h"
#include "xiaozhi/xiaozhi_ui.h"

#define DBG_TAG "voice_asst"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/* 外部函数声明 */
extern int ws_xiaozhi_init(void);
extern void xiaozhi_ui_init(void);

int voice_assistant_init(void)
{
    LOG_I("Initializing Voice Assistant...");

    /* 1. 初始化公共音频采集模块（资源仲裁） */
    if (common_audio_capture_init() != RT_EOK)
    {
        LOG_E("Failed to initialize common audio capture!");
        return -RT_ERROR;
    }

    /* 2. 初始化小智UI */
    xiaozhi_ui_init();
    LOG_I("Xiaozhi UI initialized");

    /* 3. 启动小智主线程（包含WiFi连接、WebSocket、按键处理等） */
    if (ws_xiaozhi_init() != RT_EOK)
    {
        LOG_E("Failed to initialize xiaozhi main thread!");
        return -RT_ERROR;
    }

    LOG_I("Voice Assistant initialized successfully");
    return RT_EOK;
}
