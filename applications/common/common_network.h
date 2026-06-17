/**
 * common_network.h - Common network utilities for EdgiTalk Cough Detection
 *
 * Provides WiFi connection, NTP sync, and AP mode configuration utilities.
 */  // @yyc edit

#ifndef COMMON_NETWORK_H
#define COMMON_NETWORK_H

#include <rtthread.h>

/* WiFi credentials - can be overridden via MSH or cloud config */
#define COMMON_NETWORK_DEFAULT_SSID     "qwer"
#define COMMON_NETWORK_DEFAULT_PASSWORD "bzjn7944"

typedef enum
{
    NETWORK_STATE_DISCONNECTED = 0,
    NETWORK_STATE_CONNECTING,
    NETWORK_STATE_CONNECTED,
    NETWORK_STATE_ERROR,
} common_network_state_t;

typedef struct
{
    char ssid[32];
    char password[64];
    char server_url[128];
    common_network_state_t state;
    rt_bool_t is_ready;
} common_network_t;

int common_network_init(void);
int common_network_configure(const char *ssid, const char *password);
int common_network_set_server(const char *url);
int common_network_connect(void);

rt_bool_t common_network_is_ready(void);
common_network_state_t common_network_get_state(void);

/* Upload JSON payload via HTTP POST */
int common_network_upload_json(const char *path, const char *json_payload);

/* Upload file (for WAV upload) */
int common_network_upload_file(const char *path, const char *file_path);

const common_network_t *common_network_get(void);

/**
 * Synchronize system time via NTP.
 * Blocking call - must be called from a thread context (not timer/ISR).
 * Returns RT_EOK on success, negative on failure.
 */
int common_network_ntp_sync(void);

/* NTP sync configuration */  // @yyc edit
#define NTP_SYNC_MAX_RETRIES     3       /* Max retry attempts on failure */
#define NTP_RESYNC_INTERVAL_MS   1800000 /* Periodic resync interval (30 min) */

/**
 * Check if NTP has ever synced successfully.
 * Returns RT_TRUE if synced, RT_FALSE otherwise.
 */
rt_bool_t common_network_ntp_is_synced(void);

/* NTP status notification callback type */  // @yyc edit
typedef void (*common_network_ntp_status_cb_t)(rt_bool_t synced);

/**
 * Register a callback to be notified when NTP sync status changes.
 * Only one callback can be registered at a time.
 * callback: function to call when NTP status changes (pass RT_NULL to unregister)
 */
void common_network_ntp_set_status_callback(common_network_ntp_status_cb_t callback);

/**
 * Set the periodic NTP resync interval.
 * Must be called before starting the resync timer.
 * interval_ms: interval in milliseconds (default: NTP_RESYNC_INTERVAL_MS)
 */
void common_network_ntp_set_resync_interval(rt_uint32_t interval_ms);

/**
 * Start periodic NTP resync timer.
 * This will automatically sync time every interval_ms milliseconds.
 * Returns RT_EOK on success, negative on failure.
 */
int common_network_ntp_resync_timer_start(void);

/**
 * Stop periodic NTP resync timer.
 * Returns RT_EOK on success, negative on failure.
 */
int common_network_ntp_resync_timer_stop(void);

/* AP mode for WiFi configuration */
typedef enum
{
    AP_MODE_INACTIVE = 0,
    AP_MODE_ACTIVE,
    AP_MODE_CONNECTING,   /* User selected WiFi, connecting */
    AP_MODE_SUCCESS,      /* Connection successful, exit AP */
    AP_MODE_FAILED,       /* Connection failed */
} common_ap_state_t;

/**
 * Start AP mode for WiFi configuration.
 * This will switch the device to AP mode, allowing users to connect
 * and configure WiFi via web browser.
 * Returns RT_EOK on success, negative on failure.
 */
int common_network_start_ap_mode(void);

/**
 * Stop AP mode and switch back to STA mode.
 * Returns RT_EOK on success, negative on failure.
 */
int common_network_stop_ap_mode(void);

/**
 * Get current AP mode state.
 */
common_ap_state_t common_network_get_ap_state(void);

/**
 * Callback when WiFi connection from AP mode succeeds.
 * Called internally when device successfully connects to user WiFi.
 */
void common_network_ap_connect_success(void);

/**
 * Callback when WiFi connection from AP mode fails.
 * Called internally when connection fails.
 */
void common_network_ap_connect_failed(void);

/**
 * Set pending WiFi credentials from web server CGI.
 * Internal function, called by CGI handlers.
 */
void common_network_set_pending_wifi(const char *ssid, const char *password);

#endif
