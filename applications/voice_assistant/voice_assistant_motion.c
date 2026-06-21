#include <rtdevice.h>
#include <rtthread.h>

#define DBG_TAG "voice.motion"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define VA_IMU_I2C_BUS_NAME        "i2c0"
#define VA_IMU_I2C_ADDR            0x6a
#define VA_IMU_WHO_AM_I            0x0f
#define VA_IMU_CTRL1_XL            0x10
#define VA_IMU_CTRL2_G             0x11
#define VA_IMU_CTRL3_C             0x12
#define VA_IMU_STATUS_REG          0x1e
#define VA_IMU_OUTX_L_G            0x22
#define VA_IMU_OUTX_L_XL           0x28
#define VA_IMU_ID                  0x6a

#define VA_IMU_THREAD_STACK        2048
#define VA_IMU_THREAD_PRIORITY     20
#define VA_IMU_SAMPLE_MS           50
#define VA_IMU_TRIGGER_COOLDOWN_MS 3000
#define VA_IMU_ACCEL_DELTA_SUM     18000
#define VA_IMU_GYRO_ABS_SUM        30000

extern void xiaozhi_trigger_toggle(void);

static struct rt_i2c_bus_device *s_imu_i2c;

static rt_err_t imu_write_u8(rt_uint8_t reg, rt_uint8_t value)
{
    rt_uint8_t buf[2] = {reg, value};
    struct rt_i2c_msg msg;

    msg.addr = VA_IMU_I2C_ADDR;
    msg.flags = RT_I2C_WR;
    msg.buf = buf;
    msg.len = sizeof(buf);

    return (rt_i2c_transfer(s_imu_i2c, &msg, 1) == 1) ? RT_EOK : -RT_ERROR;
}

static rt_err_t imu_read(rt_uint8_t reg, rt_uint8_t *buf, rt_uint16_t len)
{
    struct rt_i2c_msg msgs[2];

    msgs[0].addr = VA_IMU_I2C_ADDR;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf = &reg;
    msgs[0].len = 1;

    msgs[1].addr = VA_IMU_I2C_ADDR;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf = buf;
    msgs[1].len = len;

    return (rt_i2c_transfer(s_imu_i2c, msgs, 2) == 2) ? RT_EOK : -RT_ERROR;
}

static rt_int16_t le16(const rt_uint8_t *buf)
{
    return (rt_int16_t)((rt_uint16_t)buf[0] | ((rt_uint16_t)buf[1] << 8));
}

static rt_uint32_t abs_i32(rt_int32_t value)
{
    return (value < 0) ? (rt_uint32_t)(-value) : (rt_uint32_t)value;
}

static rt_err_t imu_hw_init(void)
{
    rt_uint8_t id = 0;

    s_imu_i2c = (struct rt_i2c_bus_device *)rt_device_find(VA_IMU_I2C_BUS_NAME);
    if (s_imu_i2c == RT_NULL)
    {
        LOG_W("LSM6DS3 i2c bus %s not found, gesture wake disabled", VA_IMU_I2C_BUS_NAME);
        return -RT_ENOSYS;
    }

    if (imu_read(VA_IMU_WHO_AM_I, &id, 1) != RT_EOK || id != VA_IMU_ID)
    {
        LOG_W("LSM6DS3 not detected on %s, id=0x%02x", VA_IMU_I2C_BUS_NAME, id);
        return -RT_ERROR;
    }

    imu_write_u8(VA_IMU_CTRL3_C, 0x44); /* BDU + auto-increment. */
    imu_write_u8(VA_IMU_CTRL1_XL, 0x40); /* Accel 104 Hz, +/-2 g. */
    imu_write_u8(VA_IMU_CTRL2_G, 0x4c); /* Gyro 104 Hz, 2000 dps. */

    LOG_I("LSM6DS3 gesture trigger ready");
    return RT_EOK;
}

static void voice_motion_thread(void *parameter)
{
    rt_uint8_t status;
    rt_uint8_t raw[6];
    rt_int16_t acc[3] = {0};
    rt_int16_t prev_acc[3] = {0};
    rt_bool_t have_prev = RT_FALSE;
    int hit_count = 0;
    rt_tick_t last_trigger = 0;

    RT_UNUSED(parameter);

    if (imu_hw_init() != RT_EOK)
    {
        return;
    }

    while (1)
    {
        rt_thread_mdelay(VA_IMU_SAMPLE_MS);

        if (imu_read(VA_IMU_STATUS_REG, &status, 1) != RT_EOK)
        {
            continue;
        }

        if (status & 0x02)
        {
            rt_uint32_t gyro_sum;

            if (imu_read(VA_IMU_OUTX_L_G, raw, sizeof(raw)) != RT_EOK)
            {
                continue;
            }

            gyro_sum = abs_i32(le16(&raw[0])) + abs_i32(le16(&raw[2])) + abs_i32(le16(&raw[4]));
            if (gyro_sum > VA_IMU_GYRO_ABS_SUM)
            {
                hit_count++;
            }
        }

        if (status & 0x01)
        {
            rt_uint32_t delta_sum;

            if (imu_read(VA_IMU_OUTX_L_XL, raw, sizeof(raw)) != RT_EOK)
            {
                continue;
            }

            acc[0] = le16(&raw[0]);
            acc[1] = le16(&raw[2]);
            acc[2] = le16(&raw[4]);

            if (!have_prev)
            {
                prev_acc[0] = acc[0];
                prev_acc[1] = acc[1];
                prev_acc[2] = acc[2];
                have_prev = RT_TRUE;
                continue;
            }

            delta_sum = abs_i32((rt_int32_t)acc[0] - prev_acc[0]) +
                        abs_i32((rt_int32_t)acc[1] - prev_acc[1]) +
                        abs_i32((rt_int32_t)acc[2] - prev_acc[2]);

            prev_acc[0] = acc[0];
            prev_acc[1] = acc[1];
            prev_acc[2] = acc[2];

            if (delta_sum > VA_IMU_ACCEL_DELTA_SUM)
            {
                hit_count++;
            }
            else if (hit_count > 0)
            {
                hit_count--;
            }
        }

        if (hit_count >= 2)
        {
            rt_tick_t now = rt_tick_get();
            if ((last_trigger == 0) ||
                ((now - last_trigger) >= rt_tick_from_millisecond(VA_IMU_TRIGGER_COOLDOWN_MS)))
            {
                last_trigger = now;
                LOG_I("gesture trigger toggles Xiaozhi");
                xiaozhi_trigger_toggle();
            }
            hit_count = 0;
        }
    }
}

int voice_assistant_motion_init(void)
{
    rt_thread_t tid = rt_thread_create("va_motion",
                                       voice_motion_thread,
                                       RT_NULL,
                                       VA_IMU_THREAD_STACK,
                                       VA_IMU_THREAD_PRIORITY,
                                       10);
    if (tid == RT_NULL)
    {
        LOG_W("create gesture trigger thread failed");
        return -RT_ENOMEM;
    }

    rt_thread_startup(tid);
    return RT_EOK;
}
