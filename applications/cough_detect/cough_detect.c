/*
 * cough_detect.c — Cough / Snore detection core logic
 *
 * Architecture (three threads):
 *
 *   ┌──────────────┐  audio frames   ┌─────────────────┐
 *   │  mic_thread  │ ──ring_buffer──► │ infer_thread    │
 *   │  (reader)    │                  │ (EI classifier) │
 *   └──────────────┘                  └────────┬────────┘
 *                                              │ CD_EVENT_TRIGGERED
 *                                    ┌─────────▼────────┐
 *                                    │ control_thread   │
 *                                    │ (state machine)  │
 *                                    └──────────────────┘
 *
 * Stage 1: mic_thread + control_thread are functional.
 *          infer_thread calls the Edge Impulse stub (to be wired in Stage 2).
 */

#include "cough_detect.h"
#include <string.h>
#include "cough_ui.h"
#include "cough_infer.h"
#include "cough_stat.h"
#include "cough_record.h"
#include "cough_remind.h"
#include "../common/common_network.h"
#include "../common/common_led.h"

#define DBG_TAG "cough"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/* High-rate inference logs can flood UART; keep disabled by default. */
#define CD_STREAM_LOG_ENABLE 0

/* Periodic upload interval (10 minutes) */
#define CD_UPLOAD_INTERVAL_MS   (10 * 60 * 1000)

/* Minimum cough confidence score to trigger an event – kept in detect.h */

/* ─────────────────────────────────────────────────────────────────── */
/*  Ring buffer: mic_thread writes, infer_thread reads                 */
/* ─────────────────────────────────────────────────────────────────── */
/* Hold 1 second of audio.  Power-of-two size keeps the modulo cheap. */
#define RING_BUF_FRAMES     100                         /* ~1 s        */
#define RING_BUF_SAMPLES    (RING_BUF_FRAMES * CD_FRAME_SAMPLES)

static int16_t  s_ring_buf[RING_BUF_SAMPLES] __attribute__((section(".cy_socmem_bss")));
static uint32_t s_ring_wr  = 0;  /* write index (in samples)          */
static uint32_t s_ring_rd  = 0;  /* read  index (in samples)          */
static rt_mutex_t s_ring_mutex = RT_NULL;

/* ─────────────────────────────────────────────────────────────────── */
/*  Shared state                                                       */
/* ─────────────────────────────────────────────────────────────────── */
static cd_state_t   s_state    = CD_STATE_IDLE;
static float        s_baseline = 0.0f;
static rt_event_t   s_event    = RT_NULL;

void cough_detect_send_event(rt_uint32_t event_set)
{
    if (s_event != RT_NULL)
        rt_event_send(s_event, event_set);
}
static rt_device_t  s_mic_dev  = RT_NULL;
static rt_tick_t    s_last_ui_tick = 0;

/* ─────────────────────────────────────────────────────────────────── */
/*  Helpers                                                            */
/* ─────────────────────────────────────────────────────────────────── */

/** Compute mean energy (mean of squared samples) of a frame. */
static float frame_energy(const int16_t *samples, uint32_t n)
{
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++)
    {
        float s = (float)samples[i];
        sum += s * s;
    }
    return sum / (float)n;
}

static rt_uint16_t frame_peak_abs(const int16_t *samples, uint32_t n)
{
    rt_uint16_t peak = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        int32_t v = samples[i];
        if (v < 0)
        {
            v = -v;
        }
        if ((rt_uint16_t)v > peak)
        {
            peak = (rt_uint16_t)v;
        }
    }
    return peak;
}

/* ─────────────────────────────────────────────────────────────────── */
/*  Button ISR + debounce                                              */
/* ─────────────────────────────────────────────────────────────────── */
static void button_irq_handler(void *args)
{
    rt_base_t level = rt_pin_read(CD_BUTTON_PIN);

    /* RT-Thread pin IRQ fires on both edges; we distinguish by level  */
    if (level == PIN_LOW)
    {
        /* Key pressed → start calibration */
        rt_event_send(s_event, CD_EVENT_KEY_DOWN);
    }
    else
    {
        /* Key released → end calibration, enter listening */
        rt_event_send(s_event, CD_EVENT_KEY_UP);
    }
}

/* ─────────────────────────────────────────────────────────────────── */
/*  Mic thread — reads PDM frames and pushes to ring buffer            */
/* ─────────────────────────────────────────────────────────────────── */
static void mic_thread_entry(void *param)
{
    int16_t frame[CD_FRAME_SAMPLES];

    while (1)
    {
        /* Block until one frame is available from the PDM driver */
        rt_size_t bytes = rt_device_read(s_mic_dev, 0, frame, CD_FRAME_BYTES);
        if (bytes != CD_FRAME_BYTES)
        {
            rt_thread_mdelay(1);
            continue;
        }

        /* Push frame into the ring buffer */
        rt_mutex_take(s_ring_mutex, RT_WAITING_FOREVER);
        for (int i = 0; i < CD_FRAME_SAMPLES; i++)
        {
            s_ring_buf[s_ring_wr % RING_BUF_SAMPLES] = frame[i];
            s_ring_wr++;
        }
        rt_mutex_release(s_ring_mutex);

        /* Feed audio to recorder if recording is active */
        if (cough_record_get_state() == RECORD_STATE_RECORDING)
        {
            cough_record_feed(frame, CD_FRAME_SAMPLES);
        }

        rt_tick_t now = rt_tick_get();
        rt_uint16_t peak = frame_peak_abs(frame, CD_FRAME_SAMPLES);
        if ((now - s_last_ui_tick) >= rt_tick_from_millisecond(50))
        {
            s_last_ui_tick = now;
            cough_ui_push_level(peak);
        }
    }
}

/* ─────────────────────────────────────────────────────────────────── */
/*  Inference thread — accumulates audio, runs feature extraction +    */
/*  TFLite Micro classifier when energy gate is triggered.             */
/* ─────────────────────────────────────────────────────────────────── */

/* Buffer to accumulate audio for one inference window */
static int16_t s_infer_buf[COUGH_INFER_WINDOW_SAMPLES] __attribute__((section(".cy_socmem_bss")));

static void infer_thread_entry(void *param)
{
    int16_t frame[CD_FRAME_SAMPLES];
    uint32_t buf_pos = 0;
    rt_bool_t energy_triggered = RT_FALSE;

    /* Calibration accumulators */
    float    calib_sum = 0.0f;
    uint32_t calib_cnt = 0;

    /* ── Auto-calibration at startup ────────────────────────────── */
    /*  Wait for mic to stabilise, then sample ambient noise for    */
    /*  ~3 s to compute a baseline.  Removes the need to press the  */
    /*  button before detection can start.                           */
    LOG_I("[AUTO-CAL] Waiting 1 s for mic to stabilise...");
    rt_thread_mdelay(1000);

    LOG_I("[AUTO-CAL] Measuring ambient noise (~3 s, keep quiet)...");
    s_state = CD_STATE_CALIBRATE;
    cough_ui_set_state_text("CALIBRATE");

    {
        const uint32_t auto_cal_target = 300; /* 300 × 10 ms = 3 s */
        calib_sum = 0.0f;
        calib_cnt = 0;

        while (calib_cnt < auto_cal_target)
        {
            rt_mutex_take(s_ring_mutex, RT_WAITING_FOREVER);
            uint32_t avail = s_ring_wr - s_ring_rd;
            rt_mutex_release(s_ring_mutex);

            if (avail < CD_FRAME_SAMPLES)
            {
                rt_thread_mdelay(5);
                continue;
            }

            rt_mutex_take(s_ring_mutex, RT_WAITING_FOREVER);
            for (int i = 0; i < CD_FRAME_SAMPLES; i++)
            {
                frame[i] = s_ring_buf[s_ring_rd % RING_BUF_SAMPLES];
                s_ring_rd++;
            }
            rt_mutex_release(s_ring_mutex);

            calib_sum += frame_energy(frame, CD_FRAME_SAMPLES);
            calib_cnt++;
        }

        s_baseline = calib_sum / (float)calib_cnt;
        if (s_baseline < 1.0f)
        {
            s_baseline = 1.0f;  /* floor to avoid division issues */
        }
        LOG_I("[AUTO-CAL] Baseline = %.1f  (over %u frames)", s_baseline, calib_cnt);
        LOG_I("[AUTO-CAL] SNR threshold = %.1f  (baseline x %.1f)",
              s_baseline * CD_SNR_FACTOR, (double)CD_SNR_FACTOR);
    }

    s_state = CD_STATE_LISTENING;
    cough_ui_set_state_text("LISTEN");
    LOG_I("[STATE] -> LISTENING  (auto-start, no button needed)");

    /* Reset accumulators for any future manual calibration */
    calib_sum = 0.0f;
    calib_cnt = 0;

    while (1)
    {
        /* Wait until there is at least one frame to read */
        rt_mutex_take(s_ring_mutex, RT_WAITING_FOREVER);
        uint32_t avail = s_ring_wr - s_ring_rd;
        rt_mutex_release(s_ring_mutex);

        if (avail < CD_FRAME_SAMPLES)
        {
            rt_thread_mdelay(5);
            continue;
        }

        /* Copy one frame out of the ring buffer */
        rt_mutex_take(s_ring_mutex, RT_WAITING_FOREVER);
        for (int i = 0; i < CD_FRAME_SAMPLES; i++)
        {
            frame[i] = s_ring_buf[s_ring_rd % RING_BUF_SAMPLES];
            s_ring_rd++;
        }
        rt_mutex_release(s_ring_mutex);

        cd_state_t cur = s_state;

        /* ── Calibration: accumulate energy baseline ──────────────── */
        if (cur == CD_STATE_CALIBRATE)
        {
            calib_sum += frame_energy(frame, CD_FRAME_SAMPLES);
            calib_cnt++;

            if (calib_cnt >= CD_CALIB_FRAMES)
            {
                s_baseline = calib_sum / (float)calib_cnt;
                LOG_I("Noise baseline = %.1f  (over %u frames)", s_baseline, calib_cnt);
                calib_sum = 0.0f;
                calib_cnt = 0;
            }
            buf_pos = 0;
            energy_triggered = RT_FALSE;
        }
        /* ── Listening: accumulate audio + run classifier ─────────── */
        else if (cur == CD_STATE_LISTENING)
        {
            float energy = frame_energy(frame, CD_FRAME_SAMPLES);

            /* Soft energy log — informational only */
            if (s_baseline > 0.0f && energy > s_baseline * CD_SNR_FACTOR)
            {
                if (!energy_triggered)
                {
#if CD_STREAM_LOG_ENABLE
                    LOG_I("Energy gate OPEN: e=%.0f > thr=%.0f", energy, s_baseline * CD_SNR_FACTOR);
#endif
                }
                energy_triggered = RT_TRUE;
            }

            /* Accumulate audio into inference buffer */
            if (buf_pos + CD_FRAME_SAMPLES <= COUGH_INFER_WINDOW_SAMPLES)
            {
                rt_memcpy(&s_infer_buf[buf_pos], frame, CD_FRAME_BYTES);
                buf_pos += CD_FRAME_SAMPLES;
            }

            /*
             * Trigger inference when no more complete frames fit.
             * COUGH_INFER_WINDOW_SAMPLES (10496) is NOT a multiple of
             * CD_FRAME_SAMPLES (160): 10496/160 = 65.6, so buf_pos
             * maxes out at 65×160 = 10400.  We pad the remaining
             * 96 samples with silence.
             */
            if (buf_pos + CD_FRAME_SAMPLES > COUGH_INFER_WINDOW_SAMPLES)
            {
                /* Zero-pad the tail (10400..10495) */
                if (buf_pos < COUGH_INFER_WINDOW_SAMPLES)
                {
                    rt_memset(&s_infer_buf[buf_pos], 0,
                              (COUGH_INFER_WINDOW_SAMPLES - buf_pos) * sizeof(int16_t));
                }
                /* Always run inference — let the model decide noise vs cough.
                 * Relying solely on an energy gate risks missing coughs that
                 * straddle window boundaries or are quieter than 3× baseline. */
                {
                    float scores[COUGH_INFER_NUM_CLASSES];
                    int ret = cough_infer_classify(s_infer_buf,
                                                  COUGH_INFER_WINDOW_SAMPLES,
                                                  scores);
                    if (ret == 0)
                    {
#if CD_STREAM_LOG_ENABLE
                        /* High-rate tuning log (disabled by default). */
                        rt_kprintf("[INF] u=%.2f c=%.2f n=%.2f%s\r\n",
                                   scores[COUGH_INFER_IDX_UNLABELLED],
                                   scores[COUGH_INFER_IDX_COUGH],
                                   scores[COUGH_INFER_IDX_NOISE],
                                   energy_triggered ? " *" : "");
#endif

                        if (scores[COUGH_INFER_IDX_COUGH] > CD_COUGH_THRESHOLD)
                        {
                            LOG_I("*** COUGH DETECTED! score=%.2f ***",
                                  scores[COUGH_INFER_IDX_COUGH]);
                            cough_ui_push_cough_event();
                            cough_stat_record_event(scores[COUGH_INFER_IDX_COUGH]);

                            /* Flash LED on cough detection */
                            common_led_set_mode(LED_MODE_BLINK_ONCE);
                        }
                    }
                    else
                    {
                        LOG_W("Inference failed: %d", ret);
                    }
                }
                /* Reset for next window */
                buf_pos = 0;
                energy_triggered = RT_FALSE;
            }
        }
        else
        {
            /* Not calibrating or listening — reset accumulation */
            buf_pos = 0;
            energy_triggered = RT_FALSE;
        }
    }
}

/* ─────────────────────────────────────────────────────────────────── */
/*  Recording done callback                                            */
/* ─────────────────────────────────────────────────────────────────── */
static void record_done_callback(const char *wav_path, void *user_data)
{
    RT_UNUSED(user_data);
    LOG_I("Recording chunk completed: %s", wav_path);
    rt_event_send(s_event, CD_EVENT_CHUNK_DONE);
}

/* ─────────────────────────────────────────────────────────────────── */
/*  Periodic upload timer callback                                     */
/* ─────────────────────────────────────────────────────────────────── */
static rt_timer_t s_upload_timer = RT_NULL;

static void upload_timer_callback(void *parameter)
{
    RT_UNUSED(parameter);
    /* Only send event — heavy work done in control thread to avoid timer stack overflow */
    rt_event_send(s_event, CD_EVENT_UPLOAD_TICK);
}

/* ─────────────────────────────────────────────────────────────────── */
/*  Control thread — state machine                                     */
/* ─────────────────────────────────────────────────────────────────── */
static void control_thread_entry(void *param)
{
    rt_uint32_t recv = 0;

    while (1)
    {
        /* Wait for any event, no timeout */
        rt_event_recv(s_event,
                      CD_EVENT_KEY_DOWN | CD_EVENT_KEY_UP |
                      CD_EVENT_TRIGGERED | CD_EVENT_CHUNK_DONE |
                      CD_EVENT_UPLOAD_TICK | CD_EVENT_REMIND_FIRE |
                      CD_EVENT_STAT_FLUSH,
                      RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                      RT_WAITING_FOREVER,
                      &recv);

        /* ── Key pressed: start calibration ──────────────────────── */
        if (recv & CD_EVENT_KEY_DOWN)
        {
            LOG_I("[STATE] → CALIBRATE  (hold button to measure ambient noise)");
            s_state = CD_STATE_CALIBRATE;
            cough_ui_set_state_text("CALIBRATE");
        }

        /* ── Key released: begin listening ───────────────────────── */
        if (recv & CD_EVENT_KEY_UP)
        {
            if (s_baseline <= 0.0f)
            {
                LOG_W("No valid baseline yet. Press and hold the button longer.");
                s_state = CD_STATE_IDLE;
                cough_ui_set_state_text("IDLE");
            }
            else
            {
                LOG_I("[STATE] → LISTENING  (baseline=%.1f, threshold=%.1f)",
                      s_baseline, s_baseline * CD_SNR_FACTOR);
                s_state = CD_STATE_LISTENING;
                cough_ui_set_state_text("LISTEN");
            }
        }

        /* ── Sound detected → start recording ────────────────────── */
        if (recv & CD_EVENT_TRIGGERED)
        {
            if (s_state == CD_STATE_LISTENING)
            {
                LOG_I("[STATE] → RECORDING  (5-minute chunk started)");
                s_state = CD_STATE_RECORDING;
                cough_ui_set_state_text("RECORD");
                common_led_set_mode(LED_MODE_BLINK_SLOW);

                /* Start WAV recording to SD card */
                if (cough_record_start(record_done_callback, RT_NULL) != RT_EOK)
                {
                    LOG_W("Recording start failed, returning to LISTENING");
                    s_state = CD_STATE_LISTENING;
                    cough_ui_set_state_text("LISTEN");
                    common_led_set_mode(LED_MODE_ON);
                }
            }
        }

        /* ── Periodic stats upload (from timer) ─────────────────── */
        if (recv & CD_EVENT_UPLOAD_TICK)
        {
            if (common_network_is_ready())
            {
                char json_buf[512];
                cough_stat_to_json(json_buf, sizeof(json_buf));
                int result = common_network_upload_json("/api/cough/stats", json_buf);
                if (result == RT_EOK)
                    LOG_I("Stats uploaded to cloud");
            }
        }

        /* ── Reminder alert (from timer) ──────────────────────────── */
        if (recv & CD_EVENT_REMIND_FIRE)
        {
            cough_remind_do_alert();
        }

        /* ── Midnight stats flush (from timer) ────────────────────── */
        if (recv & CD_EVENT_STAT_FLUSH)
        {
            cough_stat_flush_to_storage();
        }

        /* ── Chunk complete → upload then resume listening ────────── */
        if (recv & CD_EVENT_CHUNK_DONE)
        {
            LOG_I("[STATE] → UPLOADING  (sending stats to server)");
            s_state = CD_STATE_UPLOADING;
            cough_ui_set_state_text("UPLOAD");

            /* Upload stats JSON */
            if (common_network_is_ready())
            {
                char json_buf[512];
                cough_stat_to_json(json_buf, sizeof(json_buf));
                common_network_upload_json("/api/cough/stats", json_buf);
            }
            else
            {
                /* Cache stats to SD card for later upload */
                cough_stat_flush_to_storage();
                LOG_I("Network offline, stats cached to SD card");
            }

            /* Resume listening */
            LOG_I("[STATE] → LISTENING  (chunk processed, resuming)");
            s_state = CD_STATE_LISTENING;
            cough_ui_set_state_text("LISTEN");
            common_led_set_mode(LED_MODE_ON);
        }
    }
}

/* ─────────────────────────────────────────────────────────────────── */
/*  Public API                                                         */
/* ─────────────────────────────────────────────────────────────────── */

int cough_detect_init(void)
{
    /* 1. Create shared synchronisation objects */
    s_event = rt_event_create("cd_evt", RT_IPC_FLAG_FIFO);
    RT_ASSERT(s_event != RT_NULL);

    s_ring_mutex = rt_mutex_create("cd_rbuf", RT_IPC_FLAG_FIFO);
    RT_ASSERT(s_ring_mutex != RT_NULL);

    /* 2. Open microphone device */
    s_mic_dev = rt_device_find(CD_MIC_DEVICE_NAME);
    if (s_mic_dev == RT_NULL)
    {
        LOG_E("Microphone device '%s' not found!", CD_MIC_DEVICE_NAME);
        return -1;
    }

    struct rt_audio_caps mic_cfg;
    mic_cfg.main_type      = AUDIO_TYPE_MIXER;
    mic_cfg.sub_type       = AUDIO_MIXER_VOLUME;
    mic_cfg.udata.value    = 30;
    rt_device_control(s_mic_dev, AUDIO_CTL_CONFIGURE, &mic_cfg);

    if (rt_device_open(s_mic_dev, RT_DEVICE_OFLAG_RDONLY) != RT_EOK)
    {
        LOG_E("Failed to open microphone device!");
        return -2;
    }
    LOG_I("Microphone '%s' opened OK", CD_MIC_DEVICE_NAME);

    /* 3. Configure button pin (interrupt on both edges) */
    rt_pin_mode(CD_BUTTON_PIN, PIN_MODE_INPUT_PULLUP);
    rt_pin_attach_irq(CD_BUTTON_PIN, PIN_IRQ_MODE_RISING_FALLING,
                      button_irq_handler, RT_NULL);
    rt_pin_irq_enable(CD_BUTTON_PIN, PIN_IRQ_ENABLE);
    LOG_I("Button pin configured (GET_PIN(8,3))");

    cough_ui_set_state_text("IDLE");

    /* 4. Initialise the TFLite Micro inference engine */
    if (cough_infer_init() != 0)
    {
        LOG_E("Inference engine init failed!");
        /* Continue anyway — mic + UI still work, inference just won't run */
    }

    /* 5. Initialize statistics, recording, and reminder subsystems */
    cough_stat_init();
    cough_record_init();
    cough_remind_init();

    /* 6. Start periodic stats upload timer */
    s_upload_timer = rt_timer_create("cd_upl", upload_timer_callback, RT_NULL,
                                     rt_tick_from_millisecond(CD_UPLOAD_INTERVAL_MS),
                                     RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);
    if (s_upload_timer != RT_NULL)
    {
        rt_timer_start(s_upload_timer);
    }

    /* LED: steady on means system active */
    common_led_set_mode(LED_MODE_ON);

    /* 7. Start the three worker threads */
    rt_thread_t t;

    t = rt_thread_create("cd_mic", mic_thread_entry, RT_NULL,
                         1024 * 2, 6, 10);
    RT_ASSERT(t != RT_NULL);
    rt_thread_startup(t);

    t = rt_thread_create("cd_inf", infer_thread_entry, RT_NULL,
                         1024 * 8, 8, 10);
    RT_ASSERT(t != RT_NULL);
    rt_thread_startup(t);

    t = rt_thread_create("cd_ctl", control_thread_entry, RT_NULL,
                         1024 * 2, 10, 10);
    RT_ASSERT(t != RT_NULL);
    rt_thread_startup(t);

    LOG_I("cough_detect_init complete. Press & hold button to calibrate.");
    return 0;
}

cd_state_t cough_detect_get_state(void)
{
    return s_state;
}

float cough_detect_get_baseline(void)
{
    return s_baseline;
}
