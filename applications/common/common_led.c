#include "common_led.h"

#include <board.h>
#include <rtdevice.h>

#define DBG_TAG "common.led"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define LED_PIN             GET_PIN(16, 5)
#define LED_BLINK_SLOW_MS   500
#define LED_BLINK_FAST_MS   125
#define LED_FLASH_ONCE_MS   200

static common_led_mode_t s_mode = LED_MODE_OFF;
static rt_timer_t s_timer = RT_NULL;
static rt_bool_t  s_led_on = RT_FALSE;
static rt_uint32_t s_flash_count = 0;

static void led_set_output(rt_bool_t on)
{
    s_led_on = on;
    rt_pin_write(LED_PIN, on ? PIN_LOW : PIN_HIGH);
}

static void led_timer_callback(void *parameter)
{
    RT_UNUSED(parameter);

    switch (s_mode)
    {
    case LED_MODE_BLINK_SLOW:
    case LED_MODE_BLINK_FAST:
        led_set_output(!s_led_on);
        break;

    case LED_MODE_BLINK_ONCE:
        led_set_output(RT_FALSE);
        rt_timer_stop(s_timer);
        s_mode = LED_MODE_OFF;
        break;

    default:
        break;
    }
}

int common_led_init(void)
{
    rt_pin_mode(LED_PIN, PIN_MODE_OUTPUT);
    led_set_output(RT_FALSE);

    s_timer = rt_timer_create("led_tmr", led_timer_callback, RT_NULL,
                              rt_tick_from_millisecond(LED_BLINK_SLOW_MS),
                              RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);
    if (s_timer == RT_NULL)
    {
        LOG_E("create led timer failed");
        return -RT_ENOMEM;
    }

    LOG_I("led service ready on pin %d", LED_PIN);
    return RT_EOK;
}

int common_led_set_mode(common_led_mode_t mode)
{
    s_mode = mode;

    if (s_timer != RT_NULL)
    {
        rt_timer_stop(s_timer);
    }

    switch (mode)
    {
    case LED_MODE_OFF:
        led_set_output(RT_FALSE);
        break;

    case LED_MODE_ON:
        led_set_output(RT_TRUE);
        break;

    case LED_MODE_BLINK_SLOW:
        rt_timer_control(s_timer, RT_TIMER_CTRL_SET_TIME,
                         &(rt_tick_t){rt_tick_from_millisecond(LED_BLINK_SLOW_MS)});
        led_set_output(RT_TRUE);
        rt_timer_start(s_timer);
        break;

    case LED_MODE_BLINK_FAST:
        rt_timer_control(s_timer, RT_TIMER_CTRL_SET_TIME,
                         &(rt_tick_t){rt_tick_from_millisecond(LED_BLINK_FAST_MS)});
        led_set_output(RT_TRUE);
        rt_timer_start(s_timer);
        break;

    case LED_MODE_BLINK_ONCE:
        rt_timer_control(s_timer, RT_TIMER_CTRL_SET_TIME,
                         &(rt_tick_t){rt_tick_from_millisecond(LED_FLASH_ONCE_MS)});
        led_set_output(RT_TRUE);
        rt_timer_start(s_timer);
        break;
    }

    return RT_EOK;
}

common_led_mode_t common_led_get_mode(void)
{
    return s_mode;
}
