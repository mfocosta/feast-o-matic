#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

/* System-wide event group bits */
#define WIFI_CONNECTED_BIT    BIT0
#define MQTT_CONNECTED_BIT    BIT1
#define OTA_IN_PROGRESS_BIT   BIT2
#define DISPLAY_AVAILABLE_BIT BIT3
#define RESERVOIR_UPDATE_BIT  BIT4

/* Shared RTOS handles – defined in core/init.c */
extern QueueHandle_t      logic_queue;
extern QueueHandle_t      display_queue;
extern EventGroupHandle_t system_event_group;
extern SemaphoreHandle_t  i2c_mutex;

/* ── Display message queue types ─────────────────────────────────────── */
typedef enum {
    DISPLAY_MSG_STATUS,           /* idle screen: weight + temp + humidity   */
    DISPLAY_MSG_DISPENSING,       /* feeding in progress: target grams       */
    DISPLAY_MSG_DISPENSING_DONE,  /* feeding finished – restore previous page*/
    DISPLAY_MSG_ERROR,            /* error string                            */
    DISPLAY_MSG_STARTUP,          /* "Sistema Iniciado" splash               */
    DISPLAY_MSG_LOGO,             /* feast logo bitmap                       */
    DISPLAY_MSG_NAV,              /* button navigation event                 */
    DISPLAY_MSG_OTA_PROGRESS,     /* OTA download progress (0-100 %)         */
    DISPLAY_MSG_SCHEDULE_HINT,    /* next scheduled feeding info             */
} display_msg_type_t;

typedef struct {
    display_msg_type_t type;
    union {
        struct { float weight; float temp; float hum; int fill_pct;  } status;
        struct { int   grams;                                        } dispensing;
        struct { int8_t dir; bool confirm;                           } nav;
        struct { int   pct;                                          } ota;
        struct { int   hour; int minute; int grams;                  } schedule_hint;
        char error[64];
    } data;
} display_msg_t;

/* Command types */
typedef enum {
    CMD_FEED_NOW,
    CMD_UPDATE_SCHEDULE,
    CMD_WRITE_BOWL_TAG,
} cmd_type_t;


typedef struct {
    int slot;   /* 0 … CONFIG_SCHED_MAX-1 */
    int hour;   /* -1 = disable this slot */
    int minute;
    int grams;
} data_feed_schedule_t;

/* Queue item – union avoids wasting space */
typedef struct {
    cmd_type_t type;
    union {
        int                  grams;        /* CMD_FEED_NOW        */
        data_feed_schedule_t schedule;     /* CMD_UPDATE_SCHEDULE */
        float                bowl_weight;  /* CMD_WRITE_BOWL_TAG  */
    } data;
} logic_queue_item_t;