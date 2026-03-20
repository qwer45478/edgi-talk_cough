#ifndef COMMON_ENV_H
#define COMMON_ENV_H

#include <rtthread.h>

typedef struct
{
    float temperature_c;
    float humidity_pct;
    rt_bool_t valid;
} common_env_sample_t;

int common_env_init(void);
int common_env_read(common_env_sample_t *sample);

/* Periodic sampling: starts a background timer that refreshes cached data */
int common_env_start_periodic(rt_uint32_t interval_ms);
int common_env_stop_periodic(void);

/* Get the last cached sample (non-blocking) */
const common_env_sample_t *common_env_get_cached(void);

#endif