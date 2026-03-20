#include <rtthread.h>

#include "common_audio_playback.h"
#include "common_env.h"
#include "common_network.h"

#define DBG_TAG "common.test"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#ifdef RT_USING_MSH
#include <finsh.h>

/*
 * Test placeholders for modules that are intentionally not fully implemented
 * yet. These commands verify interface wiring and return-code contracts.
 */

static void common_test_network(int argc, char **argv)
{
    int result;
    const common_network_t *network;

    RT_UNUSED(argc);
    RT_UNUSED(argv);

    result = common_network_init();
    LOG_I("network init: %d", result);

    result = common_network_configure("demo-ssid", "demo-password");
    LOG_I("network configure: %d", result);

    network = common_network_get();
    LOG_I("network ready flag: %d (expected 0 before driver binding)",
          network ? network->is_ready : -1);

    result = common_network_upload_json("/api/test", "{\"ping\":1}");
    LOG_I("network upload json: %d (expected -RT_ENOSYS until implemented)", result);
}
MSH_CMD_EXPORT(common_test_network, common network placeholder test);

static void common_test_env(int argc, char **argv)
{
    int result;
    common_env_sample_t sample;

    RT_UNUSED(argc);
    RT_UNUSED(argv);

    result = common_env_init();
    LOG_I("env init: %d", result);

    result = common_env_read(&sample);
    LOG_I("env read: %d (expected -RT_ENOSYS until sensor binding)", result);
    LOG_I("env sample: valid=%d temp=%.2f hum=%.2f",
          sample.valid, sample.temperature_c, sample.humidity_pct);
}
MSH_CMD_EXPORT(common_test_env, common environment placeholder test);

static void common_test_playback(int argc, char **argv)
{
    int result;
    const common_audio_playback_t *playback;

    RT_UNUSED(argc);
    RT_UNUSED(argv);

    result = common_audio_playback_init();
    LOG_I("playback init: %d", result);

    result = common_audio_playback_set_volume(65);
    LOG_I("set volume(65): %d", result);

    result = common_audio_playback_mute(RT_TRUE);
    LOG_I("mute on: %d", result);

    rt_thread_mdelay(100);

    result = common_audio_playback_mute(RT_FALSE);
    LOG_I("mute off: %d", result);

    result = common_audio_playback_play_pcm(RT_NULL, 0, 16000);
    LOG_I("play pcm: %d (expected -RT_ENOSYS until codec stream path is implemented)", result);

    playback = common_audio_playback_get();
    if (playback)
    {
        LOG_I("playback state: volume=%d powered=%d", playback->volume, playback->powered);
    }
}
MSH_CMD_EXPORT(common_test_playback, common audio playback placeholder test);

static void common_test_stub_all(int argc, char **argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);

    LOG_I("running placeholder tests: playback + network + env");
    common_test_playback(0, RT_NULL);
    common_test_network(0, RT_NULL);
    common_test_env(0, RT_NULL);
}
MSH_CMD_EXPORT(common_test_stub_all, run all placeholder module tests);

#endif /* RT_USING_MSH */
