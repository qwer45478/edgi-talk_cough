/*
 * common_config.h — Persistent configuration storage (FlashDB KVDB on NOR Flash)
 *
 * All settings survive power cycles. Backed by a 64 KB FAL partition "config"
 * using FlashDB's key-value database with automatic wear leveling.
 */

#ifndef COMMON_CONFIG_H
#define COMMON_CONFIG_H

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Key names (string literals reused across modules) ──────────── */
#define CFG_KEY_THRESHOLD       "threshold"      /* float 0.20–0.80, default 0.35 */
#define CFG_KEY_BRIGHTNESS      "brightness"     /* int   10–100,    default 80   */
#define CFG_KEY_WIFI_SSID       "wifi_ssid"      /* string, max 31 chars */
#define CFG_KEY_WIFI_PASS       "wifi_pass"      /* string, max 63 chars */
#define CFG_KEY_SERVER_URL      "server_url"     /* string, max 127 chars */
#define CFG_KEY_UPLOAD_EN       "upload_en"      /* int 0/1, default 1 */
#define CFG_KEY_DEVICE_ID       "device_id"      /* string, 12-char hex */
#define CFG_KEY_REMIND_FMT      "remind_%d"      /* remind_0 … remind_7 */
/* Remind value format: "HH:MM,enabled,label"  e.g. "08:00,1,Morning Med" */

/* ── Default values ─────────────────────────────────────────────── */
#define CFG_DEFAULT_THRESHOLD   0.35f
#define CFG_DEFAULT_BRIGHTNESS  80
#define CFG_DEFAULT_UPLOAD_EN   1

/**
 * Initialize FlashDB KVDB on the "config" FAL partition.
 * Must be called after fal_init() and before any other common_config API.
 * Returns RT_EOK on success.
 */
int common_config_init(void);

/* ── Generic get/set (string values) ────────────────────────────── */

/**
 * Read a string value by key. Returns length copied (excl. NUL), or 0 if not found.
 */
int common_config_get_str(const char *key, char *buf, int buf_size);

/**
 * Write a string value by key. Returns RT_EOK on success.
 */
int common_config_set_str(const char *key, const char *value);

/* ── Typed convenience helpers ──────────────────────────────────── */

int   common_config_get_int(const char *key, int default_val);
void  common_config_set_int(const char *key, int value);

float common_config_get_float(const char *key, float default_val);
void  common_config_set_float(const char *key, float value);

/* ── High-level load/save for specific subsystems ───────────────── */

/**
 * Load ALL persisted settings and apply to subsystems
 * (remind slots, threshold, WiFi, brightness, etc.).
 * Called once from main() after all modules are initialized.
 */
void common_config_load_all(void);

/**
 * Save a single remind slot to flash.
 * @param index  0 .. 7
 */
void common_config_save_remind(int index);

/**
 * Get the unique device ID (generates on first boot).
 * Returns pointer to a static 13-char string "xxxxxxxxxxxx\0".
 */
const char *common_config_get_device_id(void);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_CONFIG_H */
