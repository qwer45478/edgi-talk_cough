/*
 * common_config.c — Persistent configuration via FlashDB KVDB on NOR Flash
 *
 * FAL partition "config" (64 KB, 16 × 4 KB sectors) is used by FlashDB
 * to store key-value pairs with wear leveling and power-fail safety.
 */

#include "common_config.h"
#include "common_network.h"
#include "common_display.h"
#include "../cough_detect/cough_detect.h"
#include "../cough_detect/cough_remind.h"

#include <rtthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fal.h>
#include <flashdb.h>

#define DBG_TAG    "common.config"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

/* ── FlashDB instance ───────────────────────────────────────────── */
static struct fdb_kvdb s_kvdb;
static rt_bool_t s_inited = RT_FALSE;

/* Default KV table — FlashDB writes these on first boot (virgin flash) */
static struct fdb_default_kv_node s_default_kv_table[] =
{
    { CFG_KEY_THRESHOLD,  "0.35",  0 },
    { CFG_KEY_BRIGHTNESS, "80",    0 },
    { CFG_KEY_UPLOAD_EN,  "1",     0 },
    { CFG_KEY_WIFI_SSID,  COMMON_NETWORK_DEFAULT_SSID,     0 },
    { CFG_KEY_WIFI_PASS,  COMMON_NETWORK_DEFAULT_PASSWORD,  0 },
    { CFG_KEY_SERVER_URL, "",      0 },
    { CFG_KEY_DEVICE_ID,  "",      0 },
};

#define DEFAULT_KV_COUNT  (sizeof(s_default_kv_table) / sizeof(s_default_kv_table[0]))

/* ── Device ID generation ───────────────────────────────────────── */
static char s_device_id[16];

static void generate_device_id(void)
{
    /* Use rt_tick + a few hardware-derived bits as seed */
    rt_uint32_t seed = rt_tick_get();
    seed ^= (rt_uint32_t)(rt_ubase_t)&s_kvdb;  /* ASLR-like entropy */

    srand(seed);
    for (int i = 0; i < 12; i++)
    {
        static const char hex[] = "0123456789abcdef";
        s_device_id[i] = hex[rand() % 16];
    }
    s_device_id[12] = '\0';
}

/* ── Init ───────────────────────────────────────────────────────── */
int common_config_init(void)
{
    if (s_inited)
        return RT_EOK;

    struct fdb_default_kv default_kv;
    default_kv.kvs = s_default_kv_table;
    default_kv.num = DEFAULT_KV_COUNT;

    /* "config" must match the FAL partition name in fal_cfg.h */
    fdb_err_t result = fdb_kvdb_init(&s_kvdb, "config", "config", &default_kv, RT_NULL);
    if (result != FDB_NO_ERR)
    {
        LOG_E("FlashDB KVDB init failed! err=%d", (int)result);
        return -RT_ERROR;
    }

    s_inited = RT_TRUE;
    LOG_I("FlashDB KVDB initialized on partition 'config'");

    /* Ensure device_id exists */
    char id_buf[16] = {0};
    if (common_config_get_str(CFG_KEY_DEVICE_ID, id_buf, sizeof(id_buf)) == 0
        || id_buf[0] == '\0')
    {
        generate_device_id();
        common_config_set_str(CFG_KEY_DEVICE_ID, s_device_id);
        LOG_I("Generated new device ID: %s", s_device_id);
    }
    else
    {
        rt_strncpy(s_device_id, id_buf, sizeof(s_device_id) - 1);
        LOG_I("Device ID: %s", s_device_id);
    }

    return RT_EOK;
}

/* ── Generic string get/set ─────────────────────────────────────── */
int common_config_get_str(const char *key, char *buf, int buf_size)
{
    if (!s_inited || key == RT_NULL || buf == RT_NULL || buf_size <= 0)
        return 0;

    struct fdb_blob blob;
    int len = (int)fdb_kv_get_blob(&s_kvdb, key,
                                    fdb_blob_make(&blob, buf, (size_t)buf_size - 1));
    if (len < 0) len = 0;
    if (len >= buf_size) len = buf_size - 1;
    buf[len] = '\0';
    return len;
}

int common_config_set_str(const char *key, const char *value)
{
    if (!s_inited || key == RT_NULL || value == RT_NULL)
        return -RT_EINVAL;

    fdb_err_t err = fdb_kv_set(&s_kvdb, key, value);
    return (err == FDB_NO_ERR) ? RT_EOK : -RT_ERROR;
}

/* ── Typed helpers ──────────────────────────────────────────────── */
int common_config_get_int(const char *key, int default_val)
{
    char buf[16];
    if (common_config_get_str(key, buf, sizeof(buf)) == 0)
        return default_val;
    return atoi(buf);
}

void common_config_set_int(const char *key, int value)
{
    char buf[16];
    rt_snprintf(buf, sizeof(buf), "%d", value);
    common_config_set_str(key, buf);
}

float common_config_get_float(const char *key, float default_val)
{
    char buf[16];
    if (common_config_get_str(key, buf, sizeof(buf)) == 0)
        return default_val;
    /* atof may not be available on all embedded toolchains;
     * parse "0.XX" manually for robustness */
    int whole = 0, frac = 0, frac_digits = 0;
    const char *p = buf;
    int sign = 1;
    if (*p == '-') { sign = -1; p++; }
    while (*p >= '0' && *p <= '9') { whole = whole * 10 + (*p - '0'); p++; }
    if (*p == '.')
    {
        p++;
        while (*p >= '0' && *p <= '9') { frac = frac * 10 + (*p - '0'); p++; frac_digits++; }
    }
    float divisor = 1.0f;
    for (int i = 0; i < frac_digits; i++) divisor *= 10.0f;
    return sign * (whole + frac / divisor);
}

void common_config_set_float(const char *key, float value)
{
    char buf[16];
    int whole = (int)value;
    int frac = (int)((value - whole) * 100 + 0.5f);
    if (frac < 0) frac = -frac;
    rt_snprintf(buf, sizeof(buf), "%d.%02d", whole, frac);
    common_config_set_str(key, buf);
}

/* ── Device ID ──────────────────────────────────────────────────── */
const char *common_config_get_device_id(void)
{
    return s_device_id;
}

/* ── Load ALL persisted settings ────────────────────────────────── */
void common_config_load_all(void)
{
    if (!s_inited)
        return;

    char buf[128];

    /* --- WiFi credentials --- */
    char ssid[32] = {0};
    char pass[64] = {0};
    common_config_get_str(CFG_KEY_WIFI_SSID, ssid, sizeof(ssid));
    common_config_get_str(CFG_KEY_WIFI_PASS, pass, sizeof(pass));
    if (ssid[0] != '\0')
    {
        common_network_configure(ssid, pass);
        LOG_I("WiFi config loaded: SSID=%s", ssid);
    }

    /* --- Server URL --- */
    if (common_config_get_str(CFG_KEY_SERVER_URL, buf, sizeof(buf)) > 0 && buf[0] != '\0')
    {
        common_network_set_server(buf);
        LOG_I("Server URL loaded: %s", buf);
    }

    /* --- Brightness --- */
    int brightness = common_config_get_int(CFG_KEY_BRIGHTNESS, CFG_DEFAULT_BRIGHTNESS);
    common_display_set_brightness((rt_uint8_t)brightness);
    LOG_I("Brightness loaded: %d%%", brightness);

    /* --- Cough detection threshold --- */
    float threshold = common_config_get_float(CFG_KEY_THRESHOLD, CFG_DEFAULT_THRESHOLD);
    cough_detect_set_threshold(threshold);
    LOG_I("Cough threshold loaded: %d.%02d",
          (int)threshold, (int)((threshold - (int)threshold) * 100));

    /* --- Reminder slots --- */
    for (int i = 0; i < COUGH_REMIND_MAX_SLOTS; i++)
    {
        char key[16];
        rt_snprintf(key, sizeof(key), CFG_KEY_REMIND_FMT, i);
        if (common_config_get_str(key, buf, sizeof(buf)) > 0 && buf[0] != '\0')
        {
            /* Parse "HH:MM,enabled,label" */
            int hour = 0, minute = 0, enabled = 1;
            char label[24] = {0};
            /* sscanf-free parsing for robustness */
            const char *p = buf;
            hour = (p[0] - '0') * 10 + (p[1] - '0');
            minute = (p[3] - '0') * 10 + (p[4] - '0');
            p += 5;
            if (*p == ',') p++;
            enabled = (*p == '1') ? 1 : 0;
            p++;
            if (*p == ',') p++;
            rt_strncpy(label, p, sizeof(label) - 1);

            cough_remind_set(i, (rt_uint8_t)hour, (rt_uint8_t)minute, label);
            cough_remind_enable(i, enabled ? RT_TRUE : RT_FALSE);
        }
    }
    LOG_I("Reminder slots loaded from flash");
}

/* ── Save a single remind slot ──────────────────────────────────── */
void common_config_save_remind(int index)
{
    if (!s_inited || index < 0 || index >= COUGH_REMIND_MAX_SLOTS)
        return;

    const cough_remind_slot_t *slot = cough_remind_get_slot(index);
    if (slot == RT_NULL)
        return;

    char key[16];
    char val[48];
    rt_snprintf(key, sizeof(key), CFG_KEY_REMIND_FMT, index);
    rt_snprintf(val, sizeof(val), "%02d:%02d,%d,%s",
                slot->hour, slot->minute,
                slot->enabled ? 1 : 0,
                slot->label);
    common_config_set_str(key, val);
}

/* ── MSH debug command ──────────────────────────────────────────── */
static void cfg_dump(int argc, char **argv)
{
    if (!s_inited)
    {
        rt_kprintf("config not initialized\n");
        return;
    }

    struct fdb_kv_iterator iterator;
    fdb_kv_t cur_kv;
    struct fdb_blob blob;
    char val_buf[128];

    fdb_kv_iterator_init(&s_kvdb, &iterator);

    rt_kprintf("=== FlashDB Config ===\n");
    while (fdb_kv_iterate(&s_kvdb, &iterator))
    {
        cur_kv = &(iterator.curr_kv);
        int len = (int)fdb_blob_read((fdb_db_t)&s_kvdb,
                               fdb_kv_to_blob(cur_kv, fdb_blob_make(&blob, val_buf, sizeof(val_buf) - 1)));
        if (len < 0) len = 0;
        if (len >= (int)sizeof(val_buf)) len = (int)sizeof(val_buf) - 1;
        val_buf[len] = '\0';
        rt_kprintf("  %-14s = %s\n", cur_kv->name, val_buf);
    }
    rt_kprintf("======================\n");
}
MSH_CMD_EXPORT_ALIAS(cfg_dump, cfg_dump, Dump all FlashDB config keys);

static void cfg_set(int argc, char **argv)
{
    if (argc < 3)
    {
        rt_kprintf("Usage: cfg_set <key> <value>\n");
        return;
    }
    if (common_config_set_str(argv[1], argv[2]) == RT_EOK)
        rt_kprintf("OK: %s = %s\n", argv[1], argv[2]);
    else
        rt_kprintf("FAILED\n");
}
MSH_CMD_EXPORT_ALIAS(cfg_set, cfg_set, Set a config key value);

