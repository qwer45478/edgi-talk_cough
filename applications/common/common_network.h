#ifndef COMMON_NETWORK_H
#define COMMON_NETWORK_H

#include <rtthread.h>

/* WiFi credentials — can be overridden via MSH or cloud config */
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

#endif