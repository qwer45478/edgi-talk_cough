/*
 * common_audio_capture.h — 公共音频采集模块头文件
 *
 * 功能：PDM麦克风采集，支持资源仲裁
 *
 * @yyc edit
 */

#ifndef COMMON_AUDIO_CAPTURE_H
#define COMMON_AUDIO_CAPTURE_H

#include <rtthread.h>

#define COMMON_AUDIO_CAPTURE_DEVICE_NAME   "mic0"

/* ===== 音频使用者类型 ===== */
typedef enum {
    AUDIO_USER_COUGH_DETECT,    /* 咳嗽检测 */
    AUDIO_USER_VOICE_ASSISTANT, /* 语音助手 */
} audio_user_t;

/* ===== 采集状态 ===== */
typedef struct
{
    rt_device_t device;
    rt_bool_t   is_open;
    rt_bool_t   is_exclusive;           /* 当前是否为独占模式 */
    audio_user_t exclusive_owner;       /* 独占模式下的所有者 */
    rt_mutex_t  mutex;                 /* 保护共享状态 */
} common_audio_capture_t;

/* ===== API ===== */

/* 基础API */
int common_audio_capture_init(void);
int common_audio_capture_open(void);
int common_audio_capture_close(void);
rt_size_t common_audio_capture_read(void *buffer, rt_size_t bytes);
const common_audio_capture_t *common_audio_capture_get(void);

/* ===== 资源仲裁API - @yyc add ===== */

/**
 * @brief 请求独占使用音频采集设备
 * @param user 使用者类型
 * @return RT_EOK: 成功, -RT_EBUSY: 设备被其他使用者占用
 *
 * 注意：
 * - 同一用户可以多次请求（会被忽略）
 * - 不同用户之间互斥
 */
int common_audio_capture_request_exclusive(audio_user_t user);

/**
 * @brief 释放独占使用的音频采集设备
 * @param user 释放的使用者
 *
 * 注意：
 * - 只有持有者才能正确释放
 * - 释放后其他用户可以使用
 */
void common_audio_capture_release_exclusive(audio_user_t user);

/**
 * @brief 检查音频采集设备是否可用
 * @param user 询问的使用者
 * @return RT_TRUE: 可用（未被独占或已被自己独占）, RT_FALSE: 不可用
 */
rt_bool_t common_audio_capture_is_available(audio_user_t user);

/**
 * @brief 获取当前独占者（调试用）
 * @return 当前独占者，如果不是独占模式返回 -1
 */
int common_audio_capture_get_exclusive_owner(void);

/**
 * @brief 检查语音助手是否正在使用麦克风（供咳嗽检测调用）
 * @return RT_TRUE: 语音助手正在使用, RT_FALSE: 未使用
 *
 * 注意：这是一个简单的检查，基于独占模式标志
 */
rt_bool_t common_audio_capture_is_voice_assistant_active(void);

#endif /* COMMON_AUDIO_CAPTURE_H */