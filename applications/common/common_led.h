#ifndef COMMON_LED_H
#define COMMON_LED_H

#include <rtthread.h>

typedef enum
{
    LED_MODE_OFF = 0,
    LED_MODE_ON,
    LED_MODE_BLINK_SLOW,     /* 1 Hz */
    LED_MODE_BLINK_FAST,     /* 4 Hz */
    LED_MODE_BLINK_ONCE,     /* single 200ms flash */
    /* @yyc add: 语音助手LED模式 */
    LED_MODE_LISTENING,      /* 蓝色闪烁 - 聆听中 */
    LED_MODE_PROCESSING,     /* 黄色闪烁 - 处理中 */
    LED_MODE_PLAYING,        /* 绿色闪烁 - 播放中 */
    LED_MODE_NORMAL,          /* 正常模式（恢复默认） */
} common_led_mode_t;

int common_led_init(void);
int common_led_set_mode(common_led_mode_t mode);
common_led_mode_t common_led_get_mode(void);

#endif
