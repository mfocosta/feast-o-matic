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
extern EventGroupHandle_t system_event_group;
extern SemaphoreHandle_t  display_mutex;

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