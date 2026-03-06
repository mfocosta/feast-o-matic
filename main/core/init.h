#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Persisted settings (loaded from NVS on boot, defaults come from Kconfig) */
extern int  g_sched_hour;    /* -1 = no schedule configured */
extern int  g_sched_minute;
extern int  g_sched_grams;
extern int  g_cal_factor;
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