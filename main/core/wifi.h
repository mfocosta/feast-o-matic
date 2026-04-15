#ifndef WIFI_H
#define WIFI_H

#ifdef __cplusplus
extern "C" {
#endif

void initialize_wifi();
void reset_wifi_config_and_start_portal(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_H