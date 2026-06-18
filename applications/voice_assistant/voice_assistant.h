/*
 * voice_assistant.h — 小智语音助手封装头文件
 *
 * 功能：封装官方SDK的初始化，集成到主程序
 *
 * @yyc add
 */

#ifndef VOICE_ASSISTANT_H
#define VOICE_ASSISTANT_H

#include <rtthread.h>

/**
 * @brief 初始化小智语音助手
 * @return RT_EOK: 成功, 其他: 失败
 *
 * 调用此函数会：
 * - 初始化小智UI
 * - 初始化WiFi连接
 * - 创建小智主线程
 * - 初始化按键和事件处理
 */
int voice_assistant_init(void);

#endif /* VOICE_ASSISTANT_H */
