/*
 * wifi_config_cgi.h - WiFi configuration CGI header
 */

#ifndef WIFI_CONFIG_CGI_H
#define WIFI_CONFIG_CGI_H

#include <rtthread.h>

/**
 * Initialize WiFi configuration CGI handlers.
 * Called automatically via INIT_APP_EXPORT.
 */
int wifi_config_cgi_init(void);

#endif /* WIFI_CONFIG_CGI_H */
