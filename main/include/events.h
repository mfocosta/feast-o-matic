#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

/* System-wide event group bits */
#define WIFI_CONNECTED_BIT  BIT0
#define MQTT_CONNECTED_BIT  BIT1
#define OTA_IN_PROGRESS_BIT BIT2

/* Shared RTOS handles – defined in core/init.c */
extern QueueHandle_t      logic_queue;
extern QueueHandle_t      display_queue;
extern EventGroupHandle_t system_event_group;

/* ── Display message queue types ─────────────────────────────────────── */
typedef enum {
    DISPLAY_MSG_STATUS,     /* idle screen: weight + temp + humidity   */
    DISPLAY_MSG_DISPENSING, /* feeding in progress: target grams       */
    DISPLAY_MSG_ERROR,      /* error string                            */
    DISPLAY_MSG_STARTUP,    /* "Sistema Iniciado" splash               */
    DISPLAY_MSG_LOGO,       /* feast logo bitmap                       */
} display_msg_type_t;

typedef struct {
    display_msg_type_t type;
    union {
        struct { float weight; float temp; float hum; int distance; } status;
        struct { int   grams;                         } dispensing;
        char error[64];
    } data;
} display_msg_t;

/* Command types */
typedef enum {
    CMD_FEED_NOW,
    CMD_UPDATE_SCHEDULE,
} cmd_type_t;

typedef struct {
    int grams;
    int hour;
    int minute;
} data_feed_schedule_t;

/* Queue item – union avoids wasting space */
typedef struct {
    cmd_type_t type;
    union {
        int                  grams;    /* CMD_FEED_NOW      */
        data_feed_schedule_t schedule; /* CMD_UPDATE_SCHEDULE */
    } data;
} logic_queue_item_t;