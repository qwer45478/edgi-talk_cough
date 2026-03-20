#include "common_env.h"

#include <rtdevice.h>
#include <string.h>

#define DBG_TAG "common.env"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/*
 * AHT20 register-level I2C driver.
 * Compatible with AHT10/AHT20 sensors on I2C1 bus.
 */
#define AHT20_I2C_BUS_NAME     "i2c1"
#define AHT20_I2C_ADDR         0x38

/* AHT20 commands */
#define AHT20_CMD_INIT         0xBE
#define AHT20_CMD_TRIGGER      0xAC
#define AHT20_CMD_SOFT_RESET   0xBA
#define AHT20_CMD_STATUS       0x71

#define AHT20_STATUS_BUSY      (1 << 7)
#define AHT20_STATUS_CAL       (1 << 3)

static struct rt_i2c_bus_device *s_i2c_bus = RT_NULL;
static common_env_sample_t s_cached_sample;
static rt_timer_t s_periodic_timer = RT_NULL;
static rt_bool_t s_initialized = RT_FALSE;

static rt_err_t aht20_write_cmd(rt_uint8_t cmd, rt_uint8_t p1, rt_uint8_t p2)
{
    rt_uint8_t buf[3] = { cmd, p1, p2 };
    struct rt_i2c_msg msg;

    msg.addr  = AHT20_I2C_ADDR;
    msg.flags = RT_I2C_WR;
    msg.buf   = buf;
    msg.len   = 3;

    if (rt_i2c_transfer(s_i2c_bus, &msg, 1) != 1)
    {
        return -RT_EIO;
    }
    return RT_EOK;
}

static rt_err_t aht20_read_raw(rt_uint8_t *buf, rt_size_t len)
{
    struct rt_i2c_msg msg;

    msg.addr  = AHT20_I2C_ADDR;
    msg.flags = RT_I2C_RD;
    msg.buf   = buf;
    msg.len   = len;

    if (rt_i2c_transfer(s_i2c_bus, &msg, 1) != 1)
    {
        return -RT_EIO;
    }
    return RT_EOK;
}

static void env_periodic_callback(void *parameter)
{
    RT_UNUSED(parameter);
    common_env_read(&s_cached_sample);
}

int common_env_init(void)
{
    rt_uint8_t status;

    s_i2c_bus = (struct rt_i2c_bus_device *)rt_device_find(AHT20_I2C_BUS_NAME);
    if (s_i2c_bus == RT_NULL)
    {
        LOG_W("I2C bus '%s' not found, env sensor disabled", AHT20_I2C_BUS_NAME);
        return RT_EOK;
    }

    /* Soft reset */
    rt_uint8_t reset_cmd = AHT20_CMD_SOFT_RESET;
    struct rt_i2c_msg msg = { AHT20_I2C_ADDR, RT_I2C_WR, 1, &reset_cmd };
    rt_i2c_transfer(s_i2c_bus, &msg, 1);
    rt_thread_mdelay(20);

    /* Read status to check calibration */
    if (aht20_read_raw(&status, 1) != RT_EOK)
    {
        LOG_W("AHT20 status read failed, sensor may be absent");
        return RT_EOK;
    }

    /* If not calibrated, send init command */
    if (!(status & AHT20_STATUS_CAL))
    {
        aht20_write_cmd(AHT20_CMD_INIT, 0x08, 0x00);
        rt_thread_mdelay(10);
    }

    s_initialized = RT_TRUE;
    rt_memset(&s_cached_sample, 0, sizeof(s_cached_sample));
    LOG_I("AHT20 environment sensor ready on %s", AHT20_I2C_BUS_NAME);
    return RT_EOK;
}

int common_env_read(common_env_sample_t *sample)
{
    rt_uint8_t buf[7];
    rt_uint32_t raw_hum, raw_temp;

    if (sample == RT_NULL)
    {
        return -RT_EINVAL;
    }

    if (!s_initialized || (s_i2c_bus == RT_NULL))
    {
        sample->temperature_c = 0.0f;
        sample->humidity_pct = 0.0f;
        sample->valid = RT_FALSE;
        return -RT_ENOSYS;
    }

    /* Trigger measurement */
    if (aht20_write_cmd(AHT20_CMD_TRIGGER, 0x33, 0x00) != RT_EOK)
    {
        sample->valid = RT_FALSE;
        return -RT_EIO;
    }

    /* Wait for measurement to complete (typically 80ms) */
    rt_thread_mdelay(80);

    /* Read 7 bytes: status + 2.5 bytes humidity + 2.5 bytes temperature + CRC */
    if (aht20_read_raw(buf, 7) != RT_EOK)
    {
        sample->valid = RT_FALSE;
        return -RT_EIO;
    }

    if (buf[0] & AHT20_STATUS_BUSY)
    {
        /* Still busy, wait more and retry once */
        rt_thread_mdelay(40);
        if (aht20_read_raw(buf, 7) != RT_EOK)
        {
            sample->valid = RT_FALSE;
            return -RT_EIO;
        }
    }

    /* Parse raw data */
    raw_hum  = ((rt_uint32_t)buf[1] << 12) |
               ((rt_uint32_t)buf[2] << 4)  |
               ((rt_uint32_t)(buf[3] >> 4) & 0x0F);

    raw_temp = (((rt_uint32_t)(buf[3] & 0x0F)) << 16) |
               ((rt_uint32_t)buf[4] << 8)  |
               ((rt_uint32_t)buf[5]);

    sample->humidity_pct  = (float)raw_hum / 1048576.0f * 100.0f;
    sample->temperature_c = (float)raw_temp / 1048576.0f * 200.0f - 50.0f;
    sample->valid = RT_TRUE;

    return RT_EOK;
}

int common_env_start_periodic(rt_uint32_t interval_ms)
{
    if (s_periodic_timer != RT_NULL)
    {
        rt_timer_stop(s_periodic_timer);
        rt_timer_delete(s_periodic_timer);
    }

    s_periodic_timer = rt_timer_create("env_tmr", env_periodic_callback, RT_NULL,
                                       rt_tick_from_millisecond(interval_ms),
                                       RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);
    if (s_periodic_timer == RT_NULL)
    {
        return -RT_ENOMEM;
    }

    /* Do first read immediately */
    common_env_read(&s_cached_sample);
    return rt_timer_start(s_periodic_timer);
}

int common_env_stop_periodic(void)
{
    if (s_periodic_timer != RT_NULL)
    {
        rt_timer_stop(s_periodic_timer);
        rt_timer_delete(s_periodic_timer);
        s_periodic_timer = RT_NULL;
    }
    return RT_EOK;
}

const common_env_sample_t *common_env_get_cached(void)
{
    return &s_cached_sample;
}