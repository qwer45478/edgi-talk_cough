/*
 * ota.h - OTA (Over-The-Air) firmware update check
 *
 * Provides cloud OTA check functionality to query the server
 * for available firmware updates.
 */  // @yyc ÐÂÔö

#ifndef OTA_H
#define OTA_H

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* OTA check result */
typedef struct
{
    rt_bool_t has_update;          /* RT_TRUE if update available */
    char version[32];              /* Available firmware version */
    char download_url[256];        /* URL to download firmware */
    rt_uint32_t file_size;         /* Firmware file size in bytes */
    char changelog[256];           /* Change log note */
} ota_check_result_t;

/**
 * Initialize OTA module.
 */
int ota_init(void);

/**
 * Check for firmware update from cloud.
 * @param result  Output structure for check result
 * @return  RT_EOK on success, negative on failure
 */
int ota_check_for_update(ota_check_result_t *result);

/**
 * Get firmware version string.
 * @return  Current firmware version string
 */
const char *ota_get_firmware_version(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_H */
