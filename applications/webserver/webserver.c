/*
 * webserver.c - Web server initialization for AP mode
 *
 * Starts webnet server when device is in AP mode for WiFi configuration.
 */  // @yyc edit: AP模式Web服务器初始化

#include <rtthread.h>
#include "wifi_config_cgi.h"

#ifdef PKG_USING_WEBNET
#include <webnet.h>
#include <wn_session.h>
#endif

#define DBG_TAG "webserver"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

static rt_thread_t s_webserver_thread = RT_NULL;
static rt_bool_t s_webserver_running = RT_FALSE;

/* Web server thread entry */
static void webserver_thread_entry(void *parameter)
{
    (void)parameter;

    LOG_I("Web server: starting...");

#ifdef PKG_USING_WEBNET
    /* Initialize CGI handlers */
    wifi_config_cgi_init();

    /* Start webnet server */
    webnet_init();

    s_webserver_running = RT_TRUE;
    LOG_I("Web server: running on port 80");

    /* Keep thread alive while AP mode is active */
    while (s_webserver_running)
    {
        rt_thread_mdelay(1000);
    }

    /* Cleanup */
    webnet_exit();
    LOG_I("Web server: stopped");

#else
    LOG_W("Web server: webnet not available");
#endif

    s_webserver_thread = RT_NULL;
}

/**
 * Start web server for AP mode configuration.
 */
int webserver_start(void)
{
    if (s_webserver_running)
    {
        LOG_W("Web server: already running");
        return RT_EOK;
    }

    /* Create web server thread */
    s_webserver_thread = rt_thread_create("web_svr",
                                          webserver_thread_entry,
                                          RT_NULL,
                                          4096,
                                          15,
                                          10);
    if (s_webserver_thread != RT_NULL)
    {
        rt_thread_startup(s_webserver_thread);
        LOG_I("Web server: thread started");
        return RT_EOK;
    }
    else
    {
        LOG_E("Web server: failed to create thread");
        return -RT_ENOMEM;
    }
}

/**
 * Stop web server.
 */
int webserver_stop(void)
{
    if (!s_webserver_running)
    {
        return RT_EOK;
    }

    LOG_I("Web server: stopping...");
    s_webserver_running = RT_FALSE;

    /* Wait for thread to finish */
    if (s_webserver_thread != RT_NULL)
    {
        rt_thread_mdelay(1500);
    }

    return RT_EOK;
}

/**
 * Check if web server is running.
 */
rt_bool_t webserver_is_running(void)
{
    return s_webserver_running;
}
