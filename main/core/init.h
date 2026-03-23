#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Persisted settings (loaded from NVS on boot, defaults come from Kconfig) */
typedef struct {
    int16_t hour;    /* -1 = slot disabled */
    int16_t minute;
    int16_t grams;
} sched_entry_t;

extern sched_entry_t g_sched[CONFIG_SCHED_MAX];
extern int16_t  g_cal_factor;
extern int32_t  g_raw_offset;    /* HX711 raw reading with empty scale, no bowl */
extern int16_t  g_bowl_g;        /* bowl weight in grams */
extern char g_mqtt_broker[128];

/* Create all RTOS primitives (call before initArduino) */
void initialize_system(void);

/* Load / save settings from NVS (call after initArduino/nvs_flash_init) */
void nvs_load_settings(void);
void nvs_save_settings(void);
void nvs_save_broker(const char *broker_url);

#ifdef __cplusplus
}
#endif