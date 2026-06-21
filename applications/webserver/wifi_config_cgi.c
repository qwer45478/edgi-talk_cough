/*
 * wifi_config_cgi.c - WiFi configuration CGI handlers
 *
 * Provides CGI endpoints for WiFi scanning, connecting, and configuration page.
 */  // @yyc edit: WiFi≈‰Õ¯“≥√ÊCGI¥¶¿Ì∆˜

#include <rtthread.h>
#include <string.h>
#include <stdlib.h>
#include "common_network.h"

#define DBG_TAG "wifi.cgi"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/* ©§©§ HTML Page ©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§ */  // @yyc edit: WiFi≈‰Õ¯“≥√ÊHTML

static const char s_html_page[] = 
"<!DOCTYPE html>\r\n"
"<html>\r\n"
"<head>\r\n"
"    <meta charset=\"UTF-8\">\r\n"
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\r\n"
"    <title>WiFi Configuration - CoughDetect</title>\r\n"
"    <style>\r\n"
"        * { margin: 0; padding: 0; box-sizing: border-box; }\r\n"
"        body {\r\n"
"            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;\r\n"
"            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);\r\n"
"            min-height: 100vh;\r\n"
"            display: flex;\r\n"
"            align-items: center;\r\n"
"            justify-content: center;\r\n"
"            padding: 20px;\r\n"
"        }\r\n"
"        .container {\r\n"
"            background: white;\r\n"
"            border-radius: 20px;\r\n"
"            box-shadow: 0 20px 60px rgba(0,0,0,0.3);\r\n"
"            width: 100%;\r\n"
"            max-width: 480px;\r\n"
"            overflow: hidden;\r\n"
"        }\r\n"
"        .header {\r\n"
"            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);\r\n"
"            color: white;\r\n"
"            padding: 30px;\r\n"
"            text-align: center;\r\n"
"        }\r\n"
"        .header h1 { font-size: 24px; margin-bottom: 8px; }\r\n"
"        .header p { font-size: 14px; opacity: 0.9; }\r\n"
"        .content { padding: 30px; }\r\n"
"        .info-box {\r\n"
"            background: #f0f4ff;\r\n"
"            border-left: 4px solid #667eea;\r\n"
"            padding: 15px;\r\n"
"            margin-bottom: 25px;\r\n"
"            border-radius: 0 8px 8px 0;\r\n"
"        }\r\n"
"        .info-box p { font-size: 13px; color: #666; margin-bottom: 5px; }\r\n"
"        .info-box strong { color: #333; }\r\n"
"        .btn {\r\n"
"            width: 100%;\r\n"
"            padding: 14px;\r\n"
"            border: none;\r\n"
"            border-radius: 10px;\r\n"
"            font-size: 16px;\r\n"
"            font-weight: 600;\r\n"
"            cursor: pointer;\r\n"
"            margin-bottom: 15px;\r\n"
"            transition: transform 0.2s, box-shadow 0.2s;\r\n"
"        }\r\n"
"        .btn:hover { transform: translateY(-2px); }\r\n"
"        .btn:active { transform: translateY(0); }\r\n"
"        .btn-primary {\r\n"
"            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);\r\n"
"            color: white;\r\n"
"        }\r\n"
"        .btn-primary:disabled {\r\n"
"            background: #ccc;\r\n"
"            cursor: not-allowed;\r\n"
"            transform: none;\r\n"
"        }\r\n"
"        .btn-secondary {\r\n"
"            background: #f0f0f0;\r\n"
"            color: #333;\r\n"
"        }\r\n"
"        .wifi-list {\r\n"
"            max-height: 250px;\r\n"
"            overflow-y: auto;\r\n"
"            margin-bottom: 20px;\r\n"
"        }\r\n"
"        .wifi-item {\r\n"
"            display: flex;\r\n"
"            align-items: center;\r\n"
"            padding: 12px;\r\n"
"            border: 2px solid #e0e0e0;\r\n"
"            border-radius: 10px;\r\n"
"            margin-bottom: 10px;\r\n"
"            cursor: pointer;\r\n"
"            transition: all 0.2s;\r\n"
"        }\r\n"
"        .wifi-item:hover { border-color: #667eea; background: #f8f9ff; }\r\n"
"        .wifi-item.selected { border-color: #667eea; background: #f0f4ff; }\r\n"
"        .wifi-icon { font-size: 24px; margin-right: 12px; }\r\n"
"        .wifi-info { flex: 1; }\r\n"
"        .wifi-ssid { font-weight: 600; font-size: 15px; color: #333; }\r\n"
"        .wifi-signal { font-size: 12px; color: #888; }\r\n"
"        .wifi-security { font-size: 11px; color: #666; background: #eee; padding: 2px 6px; border-radius: 4px; }\r\n"
"        .form-group { margin-bottom: 20px; }\r\n"
"        .form-group label { display: block; font-size: 14px; font-weight: 600; color: #333; margin-bottom: 8px; }\r\n"
"        .form-group input {\r\n"
"            width: 100%;\r\n"
"            padding: 12px;\r\n"
"            border: 2px solid #e0e0e0;\r\n"
"            border-radius: 10px;\r\n"
"            font-size: 14px;\r\n"
"            transition: border-color 0.2s;\r\n"
"        }\r\n"
"        .form-group input:focus { outline: none; border-color: #667eea; }\r\n"
"        .status {\r\n"
"            text-align: center;\r\n"
"            padding: 15px;\r\n"
"            border-radius: 10px;\r\n"
"            margin-top: 15px;\r\n"
"            display: none;\r\n"
"        }\r\n"
"        .status.success { background: #d4edda; color: #155724; display: block; }\r\n"
"        .status.error { background: #f8d7da; color: #721c24; display: block; }\r\n"
"        .status.loading { background: #fff3cd; color: #856404; display: block; }\r\n"
"        .hidden { display: none; }\r\n"
"        .spinner {\r\n"
"            display: inline-block;\r\n"
"            width: 20px;\r\n"
"            height: 20px;\r\n"
"            border: 3px solid rgba(255,255,255,0.3);\r\n"
"            border-radius: 50%;\r\n"
"            border-top-color: white;\r\n"
"            animation: spin 1s ease-in-out infinite;\r\n"
"            margin-right: 8px;\r\n"
"            vertical-align: middle;\r\n"
"        }\r\n"
"        @keyframes spin { to { transform: rotate(360deg); } }\r\n"
"    </style>\r\n"
"</head>\r\n"
"<body>\r\n"
"    <div class=\"container\">\r\n"
"        <div class=\"header\">\r\n"
"            <h1>WiFi Configuration</h1>\r\n"
"            <p>Cough Detection Device</p>\r\n"
"        </div>\r\n"
"        <div class=\"content\">\r\n"
"            <div class=\"info-box\">\r\n"
"                <p><strong>AP Info:</strong></p>\r\n"
"                <p>Connect to the device hotspot, then configure your home WiFi</p>\r\n"
"            </div>\r\n"
"\r\n"
"            <div id=\"step1\">\r\n"
"                <button class=\"btn btn-primary\" id=\"scanBtn\" onclick=\"scanWiFi()\">\r\n"
"                    Scan WiFi Networks\r\n"
"                </button>\r\n"
"                <div class=\"wifi-list\" id=\"wifiList\">\r\n"
"                    <div style=\"text-align: center; color: #888; padding: 20px;\">\r\n"
"                        Click \"Scan WiFi Networks\" to search\r\n"
"                    </div>\r\n"
"                </div>\r\n"
"            </div>\r\n"
"\r\n"
"            <div id=\"step2\" class=\"hidden\">\r\n"
"                <h3 style=\"margin-bottom: 15px; color: #333;\">Enter WiFi Password</h3>\r\n"
"                <div class=\"form-group\">\r\n"
"                    <label>Selected Network</label>\r\n"
"                    <div style=\"padding: 12px; background: #f0f4ff; border-radius: 8px; font-weight: 600;\" id=\"selectedSSID\">-</div>\r\n"
"                </div>\r\n"
"                <div class=\"form-group\">\r\n"
"                    <label>Password</label>\r\n"
"                    <input type=\"password\" id=\"password\" placeholder=\"Enter WiFi password\" autocomplete=\"off\">\r\n"
"                </div>\r\n"
"                <button class=\"btn btn-primary\" id=\"connectBtn\" onclick=\"connectWiFi()\">\r\n"
"                    Connect\r\n"
"                </button>\r\n"
"                <button class=\"btn btn-secondary\" onclick=\"backToScan()\">\r\n"
"                    Back to Scan\r\n"
"                </button>\r\n"
"            </div>\r\n"
"\r\n"
"            <div id=\"status\" class=\"status\"></div>\r\n"
"        </div>\r\n"
"    </div>\r\n"
"\r\n"
"    <script>\r\n"
"        var selectedSSID = '';\r\n"
"        \r\n"
"        function scanWiFi() {\r\n"
"            var btn = document.getElementById('scanBtn');\r\n"
"            var list = document.getElementById('wifiList');\r\n"
"            \r\n"
"            btn.disabled = true;\r\n"
"            btn.innerHTML = '<span class=\"spinner\"></span>Scanning...';\r\n"
"            list.innerHTML = '<div style=\"text-align: center; color: #888; padding: 20px;\">Scanning...</div>';\r\n"
"            \r\n"
"            fetch('/cgi/wifi_scan')\r\n"
"                .then(function(r) { return r.json(); })\r\n"
"                .then(function(data) {\r\n"
"                    btn.disabled = false;\r\n"
"                    btn.innerHTML = 'Scan WiFi Networks';\r\n"
"                    \r\n"
"                    if (data.code === 0 && data.networks && data.networks.length > 0) {\r\n"
"                        var html = '';\r\n"
"                        for (var i = 0; i < data.networks.length; i++) {\r\n"
"                            var n = data.networks[i];\r\n"
"                            html += '<div class=\"wifi-item\" onclick=\"selectWiFi(\\'' + n.ssid + '\\')\">' +\r\n"
"                                '<span class=\"wifi-icon\">&amp;#x1F4F6;</span>' +\r\n"
"                                '<div class=\"wifi-info\">' +\r\n"
"                                '<div class=\"wifi-ssid\">' + n.ssid + '</div>' +\r\n"
"                                '<div class=\"wifi-signal\">Signal: ' + n.rssi + ' dBm</div></div>' +\r\n"
"                                (n.encrypted ? '<span class=\"wifi-security\">Encrypted</span>' : '') +\r\n"
"                                '</div>';\r\n"
"                        }\r\n"
"                        list.innerHTML = html;\r\n"
"                    } else {\r\n"
"                        list.innerHTML = '<div style=\"text-align: center; color: #888; padding: 20px;\">No networks found. Click scan again.</div>';\r\n"
"                    }\r\n"
"                })\r\n"
"                .catch(function(err) {\r\n"
"                    btn.disabled = false;\r\n"
"                    btn.innerHTML = 'Scan WiFi Networks';\r\n"
"                    list.innerHTML = '<div style=\"text-align: center; color: #f00; padding: 20px;\">Scan failed. Try again.</div>';\r\n"
"                    console.error(err);\r\n"
"                });\r\n"
"        }\r\n"
"        \r\n"
"        function selectWiFi(ssid) {\r\n"
"            selectedSSID = ssid;\r\n"
"            var items = document.querySelectorAll('.wifi-item');\r\n"
"            for (var i = 0; i < items.length; i++) {\r\n"
"                items[i].classList.remove('selected');\r\n"
"            }\r\n"
"            event.currentTarget.classList.add('selected');\r\n"
"            \r\n"
"            document.getElementById('step1').classList.add('hidden');\r\n"
"            document.getElementById('step2').classList.remove('hidden');\r\n"
"            document.getElementById('selectedSSID').textContent = ssid;\r\n"
"            document.getElementById('password').value = '';\r\n"
"            document.getElementById('password').focus();\r\n"
"            hideStatus();\r\n"
"        }\r\n"
"        \r\n"
"        function backToScan() {\r\n"
"            document.getElementById('step2').classList.add('hidden');\r\n"
"            document.getElementById('step1').classList.remove('hidden');\r\n"
"            hideStatus();\r\n"
"        }\r\n"
"        \r\n"
"        function connectWiFi() {\r\n"
"            var password = document.getElementById('password').value;\r\n"
"            var btn = document.getElementById('connectBtn');\r\n"
"            \r\n"
"            if (!password) {\r\n"
"                showStatus('Please enter password', 'error');\r\n"
"                return;\r\n"
"            }\r\n"
"            \r\n"
"            btn.disabled = true;\r\n"
"            btn.innerHTML = '<span class=\"spinner\"></span>Connecting...';\r\n"
"            showStatus('Connecting...', 'loading');\r\n"
"            \r\n"
"            var body = 'ssid=' + encodeURIComponent(selectedSSID) + '&password=' + encodeURIComponent(password);\r\n"
"            fetch('/cgi/wifi_connect', {\r\n"
"                method: 'POST',\r\n"
"                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },\r\n"
"                body: body\r\n"
"            })\r\n"
"            .then(function(r) { return r.json(); })\r\n"
"            .then(function(data) {\r\n"
"                btn.disabled = false;\r\n"
"                btn.innerHTML = 'Connect';\r\n"
"                \r\n"
"                if (data.code === 0) {\r\n"
"                    showStatus('WiFi connected! Please return to device.', 'success');\r\n"
"                    btn.textContent = 'Connected!';\r\n"
"                    btn.style.background = '#28a745';\r\n"
"                } else {\r\n"
"                    showStatus(data.msg || 'Connection failed', 'error');\r\n"
"                    btn.textContent = 'Connect';\r\n"
"                }\r\n"
"            })\r\n"
"            .catch(function(err) {\r\n"
"                btn.disabled = false;\r\n"
"                btn.innerHTML = 'Connect';\r\n"
"                showStatus('Connection failed', 'error');\r\n"
"                console.error(err);\r\n"
"            });\r\n"
"        }\r\n"
"        \r\n"
"        function showStatus(msg, type) {\r\n"
"            var el = document.getElementById('status');\r\n"
"            el.textContent = msg;\r\n"
"            el.className = 'status ' + type;\r\n"
"        }\r\n"
"        \r\n"
"        function hideStatus() {\r\n"
"            document.getElementById('status').className = 'status';\r\n"
"        }\r\n"
"    </script>\r\n"
"</body>\r\n"
"</html>\r\n";

/* ©§©§ CGI Handler Support ©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§©§ */  // @yyc edit: CGI¥¶¿Ì∆˜∏®÷˙∫Ø ˝

#ifdef PKG_USING_WEBNET
#include <webnet.h>
#include <wn_session.h>
#include <wn_request.h>
#include <wn_module.h>
#ifdef RT_USING_WIFI
#include <wlan_mgnt.h>
#endif

/* Forward declarations */
static void wifi_scan_cgi_handler(struct webnet_session *session);
static void wifi_connect_cgi_handler(struct webnet_session *session);
static void wifi_config_index_handler(struct webnet_session *session);

#ifdef RT_USING_WIFI
#define WIFI_SCAN_MAX_RESULTS 10

struct wifi_scan_cache
{
    int count;
    struct rt_wlan_info info[WIFI_SCAN_MAX_RESULTS];
};

static void wifi_scan_report_callback(int event, struct rt_wlan_buff *buff, void *parameter)
{
    struct wifi_scan_cache *cache = (struct wifi_scan_cache *)parameter;
    struct rt_wlan_info *info;

    if (event != RT_WLAN_EVT_SCAN_REPORT || buff == RT_NULL || buff->data == RT_NULL || cache == RT_NULL)
    {
        return;
    }

    if (buff->len != sizeof(struct rt_wlan_info) || cache->count >= WIFI_SCAN_MAX_RESULTS)
    {
        return;
    }

    info = (struct rt_wlan_info *)buff->data;
    if (info->ssid.len == 0 || info->ssid.val[0] == '\0')
    {
        return;
    }

    rt_memcpy(&cache->info[cache->count], info, sizeof(struct rt_wlan_info));
    cache->count++;
}
#endif

static void wifi_config_index_handler(struct webnet_session *session)
{
    static const char *mimetype = "text/html";

    session->request->result_code = 200;
    webnet_session_set_header(session, mimetype, 200, "Ok", rt_strlen(s_html_page));
    webnet_session_write(session, (const rt_uint8_t *)s_html_page, rt_strlen(s_html_page));
}

static void wifi_scan_cgi_handler(struct webnet_session *session)
{
    static char response[2048];
    int response_len = 0;

    LOG_I("CGI: WiFi scan request");

#ifdef RT_USING_WIFI
    struct wifi_scan_cache cache;
    char *p = response;
    int len = sizeof(response);
    int written;
    int found;

    rt_memset(&cache, 0, sizeof(cache));
    rt_wlan_register_event_handler(RT_WLAN_EVT_SCAN_REPORT, wifi_scan_report_callback, &cache);
    if (rt_wlan_scan() != RT_EOK)
    {
        rt_wlan_unregister_event_handler(RT_WLAN_EVT_SCAN_REPORT);
        response_len = rt_snprintf(response, sizeof(response),
                    "{\"code\":-1,\"msg\":\"Scan start failed\",\"networks\":[]}");
        session->request->result_code = 200;
        webnet_session_set_header(session, "application/json", 200, "Ok", response_len);
        webnet_session_write(session, (const rt_uint8_t *)response, response_len);
        return;
    }

    rt_thread_mdelay(3000);
    rt_wlan_unregister_event_handler(RT_WLAN_EVT_SCAN_REPORT);

    written = rt_snprintf(p, len, "{\"code\":0,\"msg\":\"OK\",\"num\":%d,\"networks\":[", cache.count);
    p += written;
    len -= written;

    for (found = 0; found < cache.count; found++)
    {
        struct rt_wlan_info *info = &cache.info[found];
        char ssid_str[34] = {0};
        int ssid_len = info->ssid.len;
        int is_encrypted = (info->security != SECURITY_OPEN);

        if (ssid_len >= (int)sizeof(ssid_str))
        {
            ssid_len = sizeof(ssid_str) - 1;
        }
        rt_memcpy(ssid_str, info->ssid.val, ssid_len);
        ssid_str[ssid_len] = '\0';

        if (found > 0)
        {
            written = rt_snprintf(p, len, ",");
            p += written;
            len -= written;
        }

        written = rt_snprintf(p, len,
            "{\"ssid\":\"%s\",\"rssi\":%d,\"encrypted\":%s}",
            ssid_str,
            info->rssi,
            is_encrypted ? "true" : "false");
        p += written;
        len -= written;
    }

    written = rt_snprintf(p, len, "]}");
    response_len = p - response + written;

    LOG_I("CGI: WiFi scan returned %d networks", cache.count);
#else
    rt_strncpy(response, "{\"code\":-2,\"msg\":\"WiFi not available\",\"networks\":[]}", sizeof(response) - 1);
    response_len = rt_strlen(response);
#endif

    session->request->result_code = 200;
    webnet_session_set_header(session, "application/json", 200, "Ok", response_len);
    webnet_session_write(session, (const rt_uint8_t *)response, response_len);
}
static void wifi_copy_field(char *dst, int dst_size, const char *src)
{
    if (dst == RT_NULL || dst_size <= 0)
    {
        return;
    }

    dst[0] = '\0';
    if (src == RT_NULL)
    {
        return;
    }

    rt_strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void wifi_parse_json_field(const char *body, const char *name, char *dst, int dst_size)
{
    const char *start;
    const char *end;
    char pattern[32];
    int len;

    if (body == RT_NULL || name == RT_NULL || dst == RT_NULL || dst_size <= 0)
    {
        return;
    }

    rt_snprintf(pattern, sizeof(pattern), "\"%s\"", name);
    start = strstr(body, pattern);
    if (start == RT_NULL)
    {
        return;
    }

    start = strchr(start, ':');
    if (start == RT_NULL)
    {
        return;
    }

    start = strchr(start, '"');
    if (start == RT_NULL)
    {
        return;
    }
    start++;

    end = strchr(start, '"');
    if (end == RT_NULL)
    {
        return;
    }

    len = end - start;
    if (len <= 0 || len >= dst_size)
    {
        return;
    }

    rt_strncpy(dst, start, len);
    dst[len] = '\0';
}

static void wifi_connect_cgi_handler(struct webnet_session *session)
{
    static char response[256];
    int response_len;
    char ssid[64] = {0};
    char password[128] = {0};
    const char *body = RT_NULL;
    int body_len = 0;
    const char *form_ssid;
    const char *form_password;
    char body_buf[256];

    /* Only accept POST method */
    if (session->request->method != WEBNET_POST)
    {
        response_len = rt_snprintf(response, sizeof(response),
                    "{\"code\":-1,\"msg\":\"Method not allowed\"}");
        session->request->result_code = 405;
        webnet_session_set_header(session, "application/json", 405, "Method Not Allowed", response_len);
        webnet_session_write(session, (const rt_uint8_t *)response, response_len);
        return;
    }

    form_ssid = webnet_request_get_query(session->request, "ssid");
    form_password = webnet_request_get_query(session->request, "password");
    if (form_ssid != RT_NULL)
    {
        wifi_copy_field(ssid, sizeof(ssid), form_ssid);
        wifi_copy_field(password, sizeof(password), form_password);
        LOG_I("CGI: WiFi connect form received, content_length=%d", session->request->content_length);
    }
    else
    {
        if (session->request->query != RT_NULL && session->request->content_length > 0)
        {
            body = session->request->query;
            body_len = session->request->content_length;
        }
        else if (session->buffer_offset > 0)
        {
            body = (const char *)session->buffer;
            body_len = session->buffer_offset;
        }

        if (body_len > 0 && body != RT_NULL)
        {
            if (body_len >= (int)sizeof(body_buf))
            {
                body_len = sizeof(body_buf) - 1;
            }
            rt_memcpy(body_buf, body, body_len);
            body_buf[body_len] = '\0';
            LOG_I("CGI: WiFi connect raw body received, body_len=%d", body_len);
            wifi_parse_json_field(body_buf, "ssid", ssid, sizeof(ssid));
            wifi_parse_json_field(body_buf, "password", password, sizeof(password));
        }
    }

    /* Validate */
    if (ssid[0] == '\0')
    {
        response_len = rt_snprintf(response, sizeof(response),
                    "{\"code\":-3,\"msg\":\"Missing SSID\"}");
        session->request->result_code = 400;
        webnet_session_set_header(session, "application/json", 400, "Bad Request", response_len);
        webnet_session_write(session, (const rt_uint8_t *)response, response_len);
        return;
    }

    LOG_I("CGI: Received WiFi connect request for SSID: %s", ssid);

    /* Set pending WiFi credentials (will be picked up by AP config thread) */
    common_network_set_pending_wifi(ssid, password);

    response_len = rt_snprintf(response, sizeof(response),
                "{\"code\":0,\"msg\":\"WiFi credentials received, connecting...\"}");
    session->request->result_code = 200;
    webnet_session_set_header(session, "application/json", 200, "Ok", response_len);
    webnet_session_write(session, (const rt_uint8_t *)response, response_len);
}

/* CGI Route Registration */
int wifi_config_cgi_init(void)
{
    webnet_cgi_set_root("/cgi/");
    /* Register CGI handlers */
    webnet_cgi_register("wifi_scan", wifi_scan_cgi_handler);
    webnet_cgi_register("wifi_connect", wifi_connect_cgi_handler);
    webnet_cgi_register("index", wifi_config_index_handler);
    
    LOG_I("WiFi config CGI initialized");
    return 0;
}
INIT_APP_EXPORT(wifi_config_cgi_init);

#endif /* PKG_USING_WEBNET */
