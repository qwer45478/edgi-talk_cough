#include "common_network.h"

#include <string.h>

#include "common_power.h"

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