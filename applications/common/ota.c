/*
 * ota.c - OTA (Over-The-Air) firmware update check
 *
 * Queries cloud /api/v1/device-api/ota/check endpoint
 * to check for available firmware updates.
 */  // @yyc ÐÂÔö

#include "ota.h"
#include "common_network.h"

#include <rtthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define DBG_TAG "ota"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/* Current firmware version - should match project version */
#define OTA_FIRMWARE_VERSION "1.0.0"

/* Buffer sizes */
#define OTA_RESP_BUF_SIZE  512

/* JSON field extraction helpers */
static int json_parse_string(const char *json, const char *field, char *out, int out_size)
{
    char pattern[64];
    const char *p, *start, *end;
    int len;

    rt_snprintf(pattern, sizeof(pattern), "\"%s\"", field);
    p = strstr(json, pattern);
    if (p == RT_NULL)
        return -1;

    p = strchr(p, ':');
    if (p == RT_NULL)
        return -1;
    p++;

    /* Skip whitespace */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;

    /* Handle string value */
    if (*p != '"')
        return -1;
    p++;
    start = p;
    end = strchr(p, '"');
    if (end == RT_NULL)
        return -1;

    len = (int)(end - start);
    if (len >= out_size)
        len = out_size - 1;

    strncpy(out, start, len);
    out[len] = '\0';
    return len;
}

static int json_parse_int(const char *json, const char *field, int *out)
{
    char pattern[64];
    const char *p;
    char buf[32];

    rt_snprintf(pattern, sizeof(pattern), "\"%s\"", field);
    p = strstr(json, pattern);
    if (p == RT_NULL)
        return -1;

    p = strchr(p, ':');
    if (p == RT_NULL)
        return -1;
    p++;

    /* Skip whitespace */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;

    /* Read number */
    int len = 0;
    while (*p >= '0' && *p <= '9' && len < (int)sizeof(buf) - 1)
    {
        buf[len++] = *p++;
    }
    buf[len] = '\0';

    if (len == 0)
        return -1;

    *out = atoi(buf);
    return 0;
}

int ota_init(void)
{
    LOG_I("OTA module initialized (firmware: %s)", OTA_FIRMWARE_VERSION);
    return RT_EOK;
}

const char *ota_get_firmware_version(void)
{
    return OTA_FIRMWARE_VERSION;
}

int ota_check_for_update(ota_check_result_t *result)
{
    char resp_buf[OTA_RESP_BUF_SIZE];
    int ret;

    if (result == RT_NULL)
        return -RT_EINVAL;

    /* Initialize result to no update */
    memset(result, 0, sizeof(*result));
    result->has_update = RT_FALSE;

    if (!common_network_is_ready())
    {
        LOG_W("Network not ready for OTA check");
        return -RT_EBUSY;
    }

    /* Perform GET request to /api/v1/device-api/ota/check */
    ret = common_network_get_json("/api/v1/device-api/ota/check", resp_buf, sizeof(resp_buf));
    if (ret < 0)
    {
        LOG_W("OTA check request failed: %d", ret);
        return ret;
    }

    if (ret == 0 || resp_buf[0] == '\0' || resp_buf[0] == 'n')  /* null or "null" */
    {
        LOG_I("No OTA update available");
        return RT_EOK;
    }

    /* Parse response */
    char version[32] = {0};
    char url[256] = {0};
    int file_size = 0;

    json_parse_string(resp_buf, "version", version, sizeof(version));
    json_parse_string(resp_buf, "download_url", url, sizeof(url));
    json_parse_int(resp_buf, "file_size", &file_size);

    /* Check if version is newer than current */
    if (version[0] != '\0' && strcmp(version, OTA_FIRMWARE_VERSION) > 0)
    {
        result->has_update = RT_TRUE;
        rt_strncpy(result->version, version, sizeof(result->version) - 1);
        rt_strncpy(result->download_url, url, sizeof(result->download_url) - 1);
        result->file_size = (rt_uint32_t)file_size;
        LOG_I("OTA update available: %s (size=%u)", version, file_size);
    }
    else
    {
        LOG_I("Firmware is up to date");
    }

    return RT_EOK;
}
