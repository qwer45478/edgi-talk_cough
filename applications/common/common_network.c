#include "common_network.h"

#include <string.h>

#include "common_power.h"
#include "common_config.h"

#define DBG_TAG "common.net"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#ifdef RT_USING_WIFI
#include <lwip/ip_addr.h>
#include <wlan_mgnt.h>
#include <wlan_cfg.h>
#include <wlan_prot.h>
#include <netdev.h>
#endif

#ifdef PKG_USING_WEBCLIENT
#include <webclient.h>
#endif

#ifdef PKG_NETUTILS_NTP
#include <ntp.h>
#endif

static common_network_t s_network;

#ifdef RT_USING_WIFI
static void wifi_ready_handler(int event, struct rt_wlan_buff *buff, void *parameter)
{
    RT_UNUSED(event);
    RT_UNUSED(buff);
    RT_UNUSED(parameter);
    s_network.state = NETWORK_STATE_CONNECTED;
    s_network.is_ready = RT_TRUE;
    LOG_I("WiFi connected, network ready");

    /* Trigger NTP sync via control thread event (can't block in event handler) */
    extern void cough_detect_send_event(rt_uint32_t event_set);
    cough_detect_send_event(1 << 8);  /* CD_EVENT_NTP_SYNC */
}

static void wifi_disconnect_handler(int event, struct rt_wlan_buff *buff, void *parameter)
{
    RT_UNUSED(event);
    RT_UNUSED(buff);
    RT_UNUSED(parameter);
    s_network.state = NETWORK_STATE_DISCONNECTED;
    s_network.is_ready = RT_FALSE;
    LOG_W("WiFi disconnected");
}
#endif

int common_network_init(void)
{
    common_power_set_wifi(RT_TRUE);
    rt_memset(&s_network, 0, sizeof(s_network));
    s_network.state = NETWORK_STATE_DISCONNECTED;
    s_network.is_ready = RT_FALSE;

#ifdef RT_USING_WIFI
    /* Register WiFi event handlers */
    rt_wlan_register_event_handler(RT_WLAN_EVT_READY, wifi_ready_handler, RT_NULL);
    rt_wlan_register_event_handler(RT_WLAN_EVT_STA_DISCONNECTED,
                                   wifi_disconnect_handler, RT_NULL);

    /* Enable auto-reconnect */
    rt_wlan_config_autoreconnect(RT_TRUE);

    /* Auto-connect with default credentials if configured */
    if (COMMON_NETWORK_DEFAULT_SSID[0] != '\0')
    {
        common_network_configure(COMMON_NETWORK_DEFAULT_SSID,
                                 COMMON_NETWORK_DEFAULT_PASSWORD);
        common_network_connect();
    }
#endif

    LOG_I("network service initialized");
    return RT_EOK;
}

int common_network_configure(const char *ssid, const char *password)
{
    if (ssid != RT_NULL)
    {
        rt_strncpy(s_network.ssid, ssid, sizeof(s_network.ssid) - 1);
    }
    if (password != RT_NULL)
    {
        rt_strncpy(s_network.password, password, sizeof(s_network.password) - 1);
    }
    return RT_EOK;
}

int common_network_set_server(const char *url)
{
    if (url == RT_NULL)
    {
        return -RT_EINVAL;
    }
    rt_strncpy(s_network.server_url, url, sizeof(s_network.server_url) - 1);
    return RT_EOK;
}

int common_network_connect(void)
{
#ifdef RT_USING_WIFI
    rt_err_t result;

    if (s_network.ssid[0] == '\0')
    {
        LOG_W("WiFi SSID not configured");
        return -RT_EINVAL;
    }

    /* Wait for WLAN device to be ready (WHD driver loads firmware async) */
    for (int i = 0; i < 50; i++)  /* max 5 seconds */
    {
        if (rt_device_find("w0") != RT_NULL)
        {
            break;
        }
        rt_thread_mdelay(100);
    }

    if (rt_device_find("w0") == RT_NULL)
    {
        LOG_E("WLAN device w0 not found, WHD driver may not be ready");
        s_network.state = NETWORK_STATE_ERROR;
        return -RT_EIO;
    }

    s_network.state = NETWORK_STATE_CONNECTING;
    LOG_I("Connecting to WiFi: %s", s_network.ssid);

    result = rt_wlan_connect(s_network.ssid, s_network.password);
    if (result != RT_EOK)
    {
        LOG_E("WiFi connect failed: %d", result);
        s_network.state = NETWORK_STATE_ERROR;
        return result;
    }

    /* Wait for IP address (DHCP) */
    result = rt_wlan_is_ready();
    if (result)
    {
        s_network.state = NETWORK_STATE_CONNECTED;
        s_network.is_ready = RT_TRUE;
    }

    return RT_EOK;
#else
    return -RT_ENOSYS;
#endif
}

rt_bool_t common_network_is_ready(void)
{
    return s_network.is_ready;
}

common_network_state_t common_network_get_state(void)
{
    return s_network.state;
}

int common_network_upload_json(const char *path, const char *json_payload)
{
#ifdef PKG_USING_WEBCLIENT
    struct webclient_session *session = RT_NULL;
    char url[256];
    int result = -RT_ERROR;
    int resp_status;

    if (path == RT_NULL || json_payload == RT_NULL)
    {
        return -RT_EINVAL;
    }

    if (!s_network.is_ready)
    {
        LOG_W("Network not ready, caching upload");
        return -RT_EBUSY;
    }

    if (s_network.server_url[0] == '\0')
    {
        LOG_W("Server URL not configured");
        return -RT_EINVAL;
    }

    /* Build full URL */
    rt_snprintf(url, sizeof(url), "%s%s", s_network.server_url, path);

    session = webclient_session_create(1024);
    if (session == RT_NULL)
    {
        LOG_E("webclient session create failed");
        return -RT_ENOMEM;
    }

    webclient_header_fields_add(session, "Content-Type: application/json\r\n");
    webclient_header_fields_add(session, "Content-Length: %d\r\n", rt_strlen(json_payload));

    resp_status = webclient_post(session, url, json_payload, rt_strlen(json_payload));
    if (resp_status == 200 || resp_status == 201)
    {
        LOG_I("Upload OK: %s (status=%d)", path, resp_status);
        result = RT_EOK;
    }
    else
    {
        LOG_W("Upload failed: %s (status=%d)", path, resp_status);
        result = -RT_ERROR;
    }

    webclient_close(session);
    return result;
#else
    RT_UNUSED(path);
    RT_UNUSED(json_payload);
    return -RT_ENOSYS;
#endif
}

int common_network_upload_file(const char *path, const char *file_path)
{
    /* File upload via multipart form-data — left for cloud integration team */
    RT_UNUSED(path);
    RT_UNUSED(file_path);
    LOG_W("File upload not yet implemented (cloud team integration point)");
    return -RT_ENOSYS;
}

const common_network_t *common_network_get(void)
{
    return &s_network;
}

/* ── NTP time sync ────────────────────────────────────────────────── */  // @yyc edit: NTP增强：重试+定时校准
static rt_bool_t s_ntp_synced = RT_FALSE;
static int s_ntp_retry_count = 0;
static common_network_ntp_status_cb_t s_ntp_status_cb = RT_NULL;  /* NTP status callback */  // @yyc edit: NTP状态通知UI

/* Periodic resync timer */
static struct rt_timer s_ntp_resync_timer;
static rt_uint32_t s_ntp_resync_interval_ms = NTP_RESYNC_INTERVAL_MS;

/* Forward declarations */
static void ntp_resync_timer_callback(void *parameter);

rt_bool_t common_network_ntp_is_synced(void)
{
    return s_ntp_synced;
}

void common_network_ntp_set_status_callback(common_network_ntp_status_cb_t callback)  // @yyc edit
{
    s_ntp_status_cb = callback;
}

void common_network_ntp_set_resync_interval(rt_uint32_t interval_ms)
{
    s_ntp_resync_interval_ms = interval_ms;
}

int common_network_ntp_resync_timer_start(void)
{
    if (s_ntp_resync_timer.parent.flag & RT_TIMER_FLAG_ACTIVATED)
    {
        LOG_W("NTP: resync timer already started");
        return RT_EOK;
    }

    rt_timer_start(&s_ntp_resync_timer);
    LOG_I("NTP: resync timer started (interval=%ums)", s_ntp_resync_interval_ms);
    return RT_EOK;
}

int common_network_ntp_resync_timer_stop(void)
{
    if (!(s_ntp_resync_timer.parent.flag & RT_TIMER_FLAG_ACTIVATED))
    {
        return RT_EOK;
    }

    rt_timer_stop(&s_ntp_resync_timer);
    LOG_I("NTP: resync timer stopped");
    return RT_EOK;
}

static void ntp_resync_timer_callback(void *parameter)
{
    (void)parameter;
    LOG_I("NTP: periodic resync triggered");
    int result = common_network_ntp_sync();
    if (result != RT_EOK)
    {
        LOG_W("NTP: periodic resync failed, will retry next interval");
    }
}

int common_network_ntp_sync(void)
{
#ifdef PKG_NETUTILS_NTP
    if (!s_network.is_ready)
    {
        LOG_W("NTP: network not ready");
        return -RT_EBUSY;
    }

    LOG_I("NTP: syncing time... (attempt %d/%d)", s_ntp_retry_count + 1, NTP_SYNC_MAX_RETRIES);
    time_t now = ntp_sync_to_rtc(RT_NULL);   /* uses servers from rtconfig.h */
    if (now > 0)
    {
        s_ntp_synced = RT_TRUE;
        s_ntp_retry_count = 0;  /* Reset retry counter on success */
        struct tm *t = localtime(&now);
        LOG_I("NTP: time synced — %04d-%02d-%02d %02d:%02d:%02d",
              t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
              t->tm_hour, t->tm_min, t->tm_sec);

        /* Notify UI of NTP sync success */  // @yyc edit
        if (s_ntp_status_cb)
        {
            s_ntp_status_cb(RT_TRUE);
        }

        /* Start periodic resync timer on first successful sync */
        if (!(s_ntp_resync_timer.parent.flag & RT_TIMER_FLAG_ACTIVATED))
        {
            rt_timer_init(&s_ntp_resync_timer, "ntp_resync",
                          ntp_resync_timer_callback, RT_NULL,
                          rt_tick_from_millisecond(s_ntp_resync_interval_ms),
                          RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);
            common_network_ntp_resync_timer_start();
        }

        return RT_EOK;
    }
    else
    {
        s_ntp_retry_count++;
        if (s_ntp_retry_count < NTP_SYNC_MAX_RETRIES)
        {
            LOG_W("NTP: sync failed, retrying in 2s... (%d/%d)",
                  s_ntp_retry_count, NTP_SYNC_MAX_RETRIES);
            rt_thread_mdelay(2000);
            return common_network_ntp_sync();  /* Recursive retry */
        }
        else
        {
            LOG_E("NTP: sync failed after %d attempts", NTP_SYNC_MAX_RETRIES);
            s_ntp_retry_count = 0;  /* Reset for next manual sync */
            return -RT_ERROR;
        }
    }
#else
    LOG_W("NTP: not compiled in (PKG_NETUTILS_NTP not defined)");
    return -RT_ENOSYS;
#endif
}

/* ── MSH command: http_test ──────────────────────────────────── */
#ifdef PKG_USING_WEBCLIENT
#include <webclient.h>
static void http_test(int argc, char **argv)
{
    struct webclient_session *session;
    const char *url;
    int resp_status;
    char buf[512];
    int read_len;

    if (argc < 2)
    {
        rt_kprintf("Usage:\n");
        rt_kprintf("  http_test get <url>              - HTTP GET\n");
        rt_kprintf("  http_test post <url> <json_body> - HTTP POST\n");
        rt_kprintf("Example:\n");
        rt_kprintf("  http_test get http://httpbin.org/get\n");
        rt_kprintf("  http_test post http://httpbin.org/post {\"cough\":5}\n");
        return;
    }

    if (rt_strcmp(argv[1], "get") == 0 && argc >= 3)
    {
        url = argv[2];
        session = webclient_session_create(1024);
        if (!session) { rt_kprintf("session create failed\n"); return; }

        resp_status = webclient_get(session, url);
        rt_kprintf("GET %s -> status %d\n", url, resp_status);

        if (resp_status == 200)
        {
            while ((read_len = webclient_read(session, buf, sizeof(buf) - 1)) > 0)
            {
                buf[read_len] = '\0';
                rt_kprintf("%s", buf);
            }
            rt_kprintf("\n");
        }
        webclient_close(session);
    }
    else if (rt_strcmp(argv[1], "post") == 0 && argc >= 4)
    {
        url = argv[2];
        const char *body = argv[3];
        session = webclient_session_create(1024);
        if (!session) { rt_kprintf("session create failed\n"); return; }

        webclient_header_fields_add(session, "Content-Type: application/json\r\n");
        webclient_header_fields_add(session, "Content-Length: %d\r\n", rt_strlen(body));

        resp_status = webclient_post(session, url, body, rt_strlen(body));
        rt_kprintf("POST %s -> status %d\n", url, resp_status);

        if (resp_status == 200 || resp_status == 201)
        {
            while ((read_len = webclient_read(session, buf, sizeof(buf) - 1)) > 0)
            {
                buf[read_len] = '\0';
                rt_kprintf("%s", buf);
            }
            rt_kprintf("\n");
        }
        webclient_close(session);
    }
    else
    {
        rt_kprintf("Invalid args. Run http_test without args for usage.\n");
    }
}
MSH_CMD_EXPORT(http_test, HTTP GET/POST test command);
#endif

/* ── MSH command: ntp ────────────────────────────────────────────── */  // @yyc edit
#ifdef PKG_NETUTILS_NTP
#include <time.h>
static void ntp_status(void)
{
    time_t now = time(RT_NULL);
    struct tm *t = localtime(&now);

    rt_kprintf("=== NTP Status ===\n");
    rt_kprintf("Synced: %s\n", s_ntp_synced ? "YES" : "NO");
    rt_kprintf("Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec);
    rt_kprintf("Resync timer: %s\n",
                (s_ntp_resync_timer.parent.flag & RT_TIMER_FLAG_ACTIVATED) ? "ACTIVE" : "STOPPED");
    rt_kprintf("Resync interval: %u ms\n", s_ntp_resync_interval_ms);
}

static void ntp_sync_cmd(int argc, char **argv)
{
    if (argc == 1)
    {
        rt_kprintf("Usage: ntp_sync [force]\n");
        rt_kprintf("  Without args: normal sync with retry\n");
        rt_kprintf("  force: force sync even if already synced\n");
        return;
    }

    if (strcmp(argv[1], "force") == 0)
    {
        s_ntp_synced = RT_FALSE;
        s_ntp_retry_count = 0;
    }

    int result = common_network_ntp_sync();
    if (result == RT_EOK)
    {
        rt_kprintf("NTP sync: SUCCESS\n");
        ntp_status();
    }
    else
    {
        rt_kprintf("NTP sync: FAILED (%d)\n", result);
    }
}

static void ntp_resync_cmd(int argc, char **argv)
{
    if (argc == 1)
    {
        rt_kprintf("Usage: ntp_resync [start|stop|interval <ms>]\n");
        rt_kprintf("  start: start periodic resync timer\n");
        rt_kprintf("  stop: stop periodic resync timer\n");
        rt_kprintf("  interval <ms>: set resync interval in ms\n");
        return;
    }

    if (strcmp(argv[1], "start") == 0)
    {
        int result = common_network_ntp_resync_timer_start();
        rt_kprintf("NTP resync start: %s\n", result == RT_EOK ? "OK" : "FAILED");
    }
    else if (strcmp(argv[1], "stop") == 0)
    {
        int result = common_network_ntp_resync_timer_stop();
        rt_kprintf("NTP resync stop: %s\n", result == RT_EOK ? "OK" : "FAILED");
    }
    else if (strcmp(argv[1], "interval") == 0)
    {
        if (argc < 3)
        {
            rt_kprintf("Error: missing interval value\n");
            return;
        }
        rt_uint32_t interval = atoi(argv[2]);
        common_network_ntp_set_resync_interval(interval);
        rt_kprintf("NTP resync interval set to %u ms\n", interval);
    }
    else
    {
        rt_kprintf("Unknown command: %s\n", argv[1]);
    }
}

MSH_CMD_EXPORT(ntp_sync_cmd, ntp_sync - Sync NTP time [force]);
MSH_CMD_EXPORT(ntp_resync_cmd, ntp_resync - Control NTP periodic resync timer);
#endif

/* ── AP mode for WiFi configuration ──────────────────────────── */  // @yyc edit: AP模式WiFi配网功能
static common_ap_state_t s_ap_state = AP_MODE_INACTIVE;
static rt_thread_t s_ap_config_thread = RT_NULL;

/* AP mode configuration */
#define AP_MODE_SSID_PREFIX  "CoughDetect_"  /* @yyc edit: 配网热点SSID前缀 */
#define AP_MODE_PASSWORD     "12345678"      /* @yyc edit: 配网热点密码 */
#define AP_MODE_IP           "192.168.169.1"
#define AP_MODE_GATEWAY      "192.168.169.1"
#define AP_MODE_NETMASK      "255.255.255.0"

/* WiFi credentials received from web configuration */
static char s_pending_ssid[32] = {0};
static char s_pending_password[64] = {0};
static rt_sem_t s_wifi_connect_sem = RT_NULL;

static void ap_config_thread_entry(void *parameter)
{
    (void)parameter;

    LOG_I("AP mode: starting web server on %s", AP_MODE_IP);

    /* Start web server for configuration page */
    extern int webserver_start(void);
    webserver_start();

    /* Wait for user to configure WiFi via web browser */
    /* The web server will set s_pending_ssid and s_pending_password */
    /* When credentials are received, web server signals via sem */

    while (s_ap_state == AP_MODE_ACTIVE || s_ap_state == AP_MODE_CONNECTING)
    {
        rt_thread_mdelay(100);

        /* Check if we have received WiFi credentials from web server */
        if (s_pending_ssid[0] != '\0')
        {
            s_ap_state = AP_MODE_CONNECTING;

            /* Stop AP mode */
            LOG_I("AP mode: user configured WiFi, connecting to: %s", s_pending_ssid);
            rt_wlan_set_mode(RT_WLAN_DEVICE_STA_NAME, RT_WLAN_STATION);

            /* Disconnect from AP first */
            rt_wlan_disconnect();
            rt_thread_mdelay(500);

            /* Connect to user WiFi */
            if (rt_wlan_connect(s_pending_ssid, s_pending_password) == RT_EOK)
            {
                LOG_I("AP mode: WiFi connection initiated");
                /* Wait for connection */
                rt_thread_mdelay(3000);

                if (rt_wlan_is_ready())
                {
                    /* Save configuration to Flash */
                    common_config_set_string(CFG_KEY_WIFI_SSID, s_pending_ssid);
                    common_config_set_string(CFG_KEY_WIFI_PASS, s_pending_password);

                    s_ap_state = AP_MODE_SUCCESS;
                    LOG_I("AP mode: WiFi connected successfully!");
                    common_network_ap_connect_success();
                }
                else
                {
                    s_ap_state = AP_MODE_FAILED;
                    LOG_W("AP mode: WiFi connection failed");
                    common_network_ap_connect_failed();
                }
            }
            else
            {
                s_ap_state = AP_MODE_FAILED;
                LOG_E("AP mode: failed to start WiFi connection");
                common_network_ap_connect_failed();
            }

            /* Clear pending credentials */
            memset(s_pending_ssid, 0, sizeof(s_pending_ssid));
            memset(s_pending_password, 0, sizeof(s_pending_password));
        }
    }

    LOG_I("AP mode: exiting");
    s_ap_config_thread = RT_NULL;
}

int common_network_start_ap_mode(void)
{
    if (s_ap_state != AP_MODE_INACTIVE)
    {
        LOG_W("AP mode: already active");
        return RT_EOK;
    }

    /* Create semaphore for WiFi connection signaling */
    s_wifi_connect_sem = rt_sem_create("wifi_ap_sem", 0, RT_IPC_FLAG_FIFO);
    if (s_wifi_connect_sem == RT_NULL)
    {
        LOG_E("AP mode: failed to create semaphore");
        return -RT_ENOMEM;
    }

    /* Disconnect current WiFi if connected */
    rt_wlan_disconnect();

    /* Stop auto reconnect temporarily */
    rt_wlan_config_autoreconnect(RT_FALSE);

    /* Set AP mode */
    if (rt_wlan_set_mode(RT_WLAN_DEVICE_STA_NAME, RT_WLAN_AP) != RT_EOK)
    {
        LOG_E("AP mode: failed to set AP mode");
        rt_sem_delete(s_wifi_connect_sem);
        return -RT_ERROR;
    }

    /* Start AP */
    if (rt_wlan_start_ap(AP_MODE_SSID_PREFIX, AP_MODE_PASSWORD) != RT_EOK)
    {
        LOG_E("AP mode: failed to start AP");
        rt_wlan_set_mode(RT_WLAN_DEVICE_STA_NAME, RT_WLAN_STATION);
        rt_sem_delete(s_wifi_connect_sem);
        return -RT_ERROR;
    }

    s_ap_state = AP_MODE_ACTIVE;
    LOG_I("AP mode: started - SSID: %s, Password: %s", AP_MODE_SSID_PREFIX, AP_MODE_PASSWORD);
    LOG_I("AP mode: connect to http://%s in browser", AP_MODE_IP);

    /* Start configuration thread */
    s_ap_config_thread = rt_thread_create("ap_config",
                                           ap_config_thread_entry,
                                           RT_NULL,
                                           4096,
                                           15,
                                           10);
    if (s_ap_config_thread != RT_NULL)
    {
        rt_thread_startup(s_ap_config_thread);
    }
    else
    {
        LOG_E("AP mode: failed to create config thread");
        rt_wlan_stop_ap();
        rt_wlan_set_mode(RT_WLAN_DEVICE_STA_NAME, RT_WLAN_STATION);
        rt_sem_delete(s_wifi_connect_sem);
        s_ap_state = AP_MODE_INACTIVE;
        return -RT_ENOMEM;
    }

    return RT_EOK;
}

int common_network_stop_ap_mode(void)
{
    if (s_ap_state == AP_MODE_INACTIVE)
    {
        return RT_EOK;
    }

    LOG_I("AP mode: stopping...");

    /* Stop configuration thread */
    s_ap_state = AP_MODE_INACTIVE;

    /* Stop web server first */
    extern int webserver_stop(void);
    webserver_stop();

    if (s_ap_config_thread != RT_NULL)
    {
        rt_thread_mdelay(500);  /* Wait for thread to finish */
    }

    /* Stop AP */
    rt_wlan_stop_ap();

    /* Switch back to STA mode */
    rt_wlan_set_mode(RT_WLAN_DEVICE_STA_NAME, RT_WLAN_STATION);

    /* Re-enable auto reconnect */
    rt_wlan_config_autoreconnect(RT_TRUE);

    /* Delete semaphore */
    if (s_wifi_connect_sem != RT_NULL)
    {
        rt_sem_delete(s_wifi_connect_sem);
        s_wifi_connect_sem = RT_NULL;
    }

    /* Clear pending credentials */
    memset(s_pending_ssid, 0, sizeof(s_pending_ssid));
    memset(s_pending_password, 0, sizeof(s_pending_password));

    LOG_I("AP mode: stopped, switched to STA mode");

    return RT_EOK;
}

common_ap_state_t common_network_get_ap_state(void)
{
    return s_ap_state;
}

void common_network_ap_connect_success(void)
{
    LOG_I("AP mode: WiFi configuration completed successfully");
    /* Notify UI to update status */
    extern void page_settings_update_network(void);
    page_settings_update_network();
}

void common_network_ap_connect_failed(void)
{
    LOG_W("AP mode: WiFi configuration failed");
}

/* Called from web server CGI to set pending WiFi credentials */
void common_network_set_pending_wifi(const char *ssid, const char *password)
{
    if (ssid != RT_NULL && password != RT_NULL)
    {
        rt_strncpy(s_pending_ssid, ssid, sizeof(s_pending_ssid) - 1);
        rt_strncpy(s_pending_password, password, sizeof(s_pending_password) - 1);
        s_pending_ssid[sizeof(s_pending_ssid) - 1] = '\0';
        s_pending_password[sizeof(s_pending_password) - 1] = '\0';
        LOG_I("AP mode: received WiFi credentials from web");
    }
}