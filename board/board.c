/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-06-29     Rbb666       first version
 * 2025-08-20     Hydevcode
 */

#include "board.h"
#define ES8388_CTRL             GET_PIN(16, 2)
#define SPEAKER_OE_CTRL         GET_PIN(21, 6)
#define WIFI_OE_CTRL            GET_PIN(16, 3)
#define WIFI_WL_REG_OE_CTRL     GET_PIN(11, 6)
#define CTRL                    GET_PIN(7, 2)

void cy_bsp_all_init(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
    
}

void _start(void)
{
    extern int entry(void);
    entry();
    while (1);
    __builtin_unreachable();
}

void poweroff(void)
{
    edgi_board_poweroff_system();
}

#ifdef RT_USING_MSH
    MSH_CMD_EXPORT(poweroff, The software enables the system to shut down. Simply press the button to restart it.);
#endif

//Mos管控制
#define ES8388_CTRL                 GET_PIN(16, 2)  //ES8388 电源 Enable引脚
#define SPEAKER_OE_CTRL             GET_PIN(21, 6)  //功放 Enable引脚
#define WIFI_OE_CTRL                GET_PIN(16, 3)  //WIFI Enable引脚
#define WIFI_WL_REG_OE_CTRL         GET_PIN(11, 6)  //WiFi寄存器开关
#define CTRL                        GET_PIN(7, 2)   //底板 3V3 DCDC电源控制
#define LCD_BL_GPIO_NUM             GET_PIN(15, 7)  //LCD 背光电源开关
#define LCD_DISP_GPIO_NUM           GET_PIN(15, 6)  //LCD IC电源开关
#define BL_PWM_DISP_CTRL            GET_PIN(20, 6)  //LCD PWM亮度调节
int en_gpio(void)
{
    return edgi_board_power_init();
}
INIT_BOARD_EXPORT(en_gpio);

static void board_write_output(rt_base_t pin, rt_base_t level)
{
    rt_pin_mode(pin, PIN_MODE_OUTPUT);
    rt_pin_write(pin, level);
}

int edgi_board_power_init(void)
{
    board_write_output(WIFI_OE_CTRL, PIN_HIGH);
    board_write_output(WIFI_WL_REG_OE_CTRL, PIN_HIGH);
    board_write_output(ES8388_CTRL, PIN_HIGH);
    board_write_output(SPEAKER_OE_CTRL, PIN_HIGH);
    board_write_output(CTRL, PIN_HIGH);
    board_write_output(BL_PWM_DISP_CTRL, PIN_HIGH);
    board_write_output(LCD_DISP_GPIO_NUM, PIN_HIGH);
    board_write_output(LCD_BL_GPIO_NUM, PIN_HIGH);
    return RT_EOK;
}

int edgi_board_set_power_domain(edgi_board_power_domain_t domain, rt_bool_t enabled)
{
    rt_base_t level = enabled ? PIN_HIGH : PIN_LOW;

    switch (domain)
    {
    case EDGI_BOARD_POWER_MAIN:
        board_write_output(CTRL, level);
        return RT_EOK;
    case EDGI_BOARD_POWER_WIFI:
        board_write_output(WIFI_OE_CTRL, level);
        board_write_output(WIFI_WL_REG_OE_CTRL, level);
        return RT_EOK;
    case EDGI_BOARD_POWER_AUDIO_CODEC:
        board_write_output(ES8388_CTRL, level);
        return RT_EOK;
    case EDGI_BOARD_POWER_SPEAKER:
        board_write_output(SPEAKER_OE_CTRL, level);
        return RT_EOK;
    case EDGI_BOARD_POWER_DISPLAY:
        board_write_output(LCD_DISP_GPIO_NUM, level);
        return RT_EOK;
    case EDGI_BOARD_POWER_DISPLAY_BACKLIGHT:
        board_write_output(LCD_BL_GPIO_NUM, level);
        board_write_output(BL_PWM_DISP_CTRL, level);
        return RT_EOK;
    default:
        return -RT_EINVAL;
    }
}

int edgi_board_set_display_brightness(rt_uint8_t percent)
{
    return edgi_board_set_power_domain(EDGI_BOARD_POWER_DISPLAY_BACKLIGHT, percent > 0 ? RT_TRUE : RT_FALSE);
}

int edgi_board_poweroff_system(void)
{
    edgi_board_set_power_domain(EDGI_BOARD_POWER_WIFI, RT_FALSE);
    edgi_board_set_power_domain(EDGI_BOARD_POWER_AUDIO_CODEC, RT_FALSE);
    edgi_board_set_power_domain(EDGI_BOARD_POWER_SPEAKER, RT_FALSE);
    edgi_board_set_power_domain(EDGI_BOARD_POWER_DISPLAY_BACKLIGHT, RT_FALSE);
    edgi_board_set_power_domain(EDGI_BOARD_POWER_DISPLAY, RT_FALSE);
    edgi_board_set_power_domain(EDGI_BOARD_POWER_MAIN, RT_FALSE);

    Cy_SysClk_PllDisable(SRSS_DPLL_LP_0_PATH_NUM);
    Cy_SysPm_SystemEnterHibernate();
    return RT_EOK;
}
