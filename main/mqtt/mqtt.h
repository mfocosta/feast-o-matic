#ifndef MQTT_H
#define MQTT_H

#include "mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

extern esp_mqtt_client_handle_t mqtt_client;

void mqtt_app_start(void);

#ifdef __cplusplus
}
#endif

#endif // MQTT_H