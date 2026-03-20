#include "common_key.h"

#include <board.h>
#include <rtdevice.h>

#define DBG_TAG "common.key"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define COMMON_KEY_PIN             GET_PIN(8, 3)
#define COMMON_KEY_DEBOUNCE_MS     30
#define COMMON_KEY_LONG_PRESS_MS   1000

static common_key_callback_t s_key_callback = RT_NULL;
static void *s_key_callback_user_data = RT_NULL;
static rt_tick_t s_key_down_tick = 0;
static rt_tick_t s_last_irq_tick = 0;
static rt_bool_t s_key_initialized = RT_FALSE;

static void notify_key(common_key_event_t event)
{
    if (s_key_callback != RT_NULL)
    {
        s_key_callback(event, s_key_callback_user_data);
    }
}

static void common_key_irq(void *args)
{
    rt_tick_t now = rt_tick_get();
    rt_base_t level;

    if ((now - s_last_irq_tick) < rt_tick_from_millisecond(COMMON_KEY_DEBOUNCE_MS))
    {
        return;
    }
    s_last_irq_tick = now;

    level = rt_pin_read(COMMON_KEY_PIN);
    if (level == PIN_LOW)
    {
        s_key_down_tick = now;
        notify_key(COMMON_KEY_EVENT_DOWN);
        return;
    }

    notify_key(COMMON_KEY_EVENT_UP);
    if (s_key_down_tick != 0)
    {
        if ((now - s_key_down_tick) >= rt_tick_from_millisecond(COMMON_KEY_LONG_PRESS_MS))
        {
            notify_key(COMMON_KEY_EVENT_LONG_PRESS);
        }
        else
        {
            notify_key(COMMON_KEY_EVENT_CLICK);
        }
    }
}

int common_key_init(void)
{
    rt_err_t result;

    if (s_key_initialized)
    {
        return RT_EOK;
    }

    rt_pin_mode(COMMON_KEY_PIN, PIN_MODE_INPUT_PULLUP);
    result = rt_pin_attach_irq(COMMON_KEY_PIN, PIN_IRQ_MODE_RISING_FALLING, common_key_irq, RT_NULL);
    if (result != RT_EOK)
    {
        LOG_E("attach key irq failed: %d", result);
        return result;
    }

    result = rt_pin_irq_enable(COMMON_KEY_PIN, PIN_IRQ_ENABLE);
    if (result != RT_EOK)
    {
        LOG_E("enable key irq failed: %d", result);
        return result;
    }

    s_key_initialized = RT_TRUE;
    LOG_I("key service ready on pin %d", COMMON_KEY_PIN);
    return RT_EOK;
}

int common_key_register_callback(common_key_callback_t callback, void *user_data)
{
    s_key_callback = callback;
    s_key_callback_user_data = user_data;
    return RT_EOK;
}

rt_base_t common_key_read_level(void)
{
    return rt_pin_read(COMMON_KEY_PIN);
}