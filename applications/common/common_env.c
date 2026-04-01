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
#define AHT20_I2C_BUS_NAME     "i2c2"
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
static volatile rt_bool_t s_read_pending = RT_FALSE;
static rt_bool_t s_bus_found = RT_FALSE;  /* bus located but sensor not yet probed */

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
    /* NOTE: Do NOT call common_env_read() here!
     * This callback runs in the timer thread whose stack is small.
     * common_env_read() does I2C + rt_thread_mdelay(80) + float math,
     * which will overflow the timer stack and block all other soft timers.
     * Instead, just set a flag; the actual read is done by the caller's
     * periodic refresh mechanism (ui_refresh_callback -> control thread). */
    s_read_pending = RT_TRUE;
}

/**
 * Probe and calibrate the AHT20 sensor.
 * Can be called multiple times; once it succeeds, s_initialized stays RT_TRUE.
 */
static rt_err_t aht20_probe(void)
{
    rt_uint8_t status;

    if (s_initialized)
        return RT_EOK;

    if (s_i2c_bus == RT_NULL)
        return -RT_ENOSYS;

    /* AHT20 needs >= 40ms after power-on before it accepts commands.
     * When called at boot, the system has usually been up long enough,
     * but add a small safety margin. */
    rt_thread_mdelay(50);

    /* Read status byte — this doubles as a presence check */
    if (aht20_read_raw(&status, 1) != RT_EOK)
    {
        LOG_D("AHT20 probe: no ACK on status read");
        return -RT_EIO;
    }

    /* If not calibrated, send initialization (calibration) command */
    if (!(status & AHT20_STATUS_CAL))
    {
        LOG_D("AHT20 not calibrated (status=0x%02X), sending init cmd", status);
        if (aht20_write_cmd(AHT20_CMD_INIT, 0x08, 0x00) != RT_EOK)
        {
            return -RT_EIO;
        }
        rt_thread_mdelay(10);

        /* Re-read status to verify calibration bit is set */
        if (aht20_read_raw(&status, 1) != RT_EOK)
        {
            return -RT_EIO;
        }
        if (!(status & AHT20_STATUS_CAL))
        {
            LOG_W("AHT20 calibration failed (status=0x%02X)", status);
            return -RT_ERROR;
        }
    }

    s_initialized = RT_TRUE;
    rt_memset(&s_cached_sample, 0, sizeof(s_cached_sample));
    LOG_I("AHT20 environment sensor ready on %s", AHT20_I2C_BUS_NAME);
    return RT_EOK;
}

int common_env_init(void)
{
    s_i2c_bus = (struct rt_i2c_bus_device *)rt_device_find(AHT20_I2C_BUS_NAME);
    if (s_i2c_bus == RT_NULL)
    {
        LOG_W("I2C bus '%s' not found, env sensor disabled", AHT20_I2C_BUS_NAME);
        return RT_EOK;
    }
    s_bus_found = RT_TRUE;

    /* Try to probe sensor now; if it fails, poll() will retry later */
    if (aht20_probe() != RT_EOK)
    {
        LOG_W("AHT20 not ready at boot, will retry on first periodic read");
    }

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

    /* Do first read immediately (retry probe if needed) */
    if (s_bus_found && !s_initialized)
    {
        aht20_probe();
    }
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

void common_env_poll(void)
{
    if (s_read_pending)
    {
        s_read_pending = RT_FALSE;

        /* Deferred probe: if bus is found but sensor wasn't ready at boot */
        if (s_bus_found && !s_initialized)
        {
            aht20_probe();
        }

        common_env_read(&s_cached_sample);
    }
}