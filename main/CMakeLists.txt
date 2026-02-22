idf_component_register(SRCS "main.cpp"
                            "core/init.c" "core/wifi.c" "core/feeder.c" "core/scheduler.c"
                            "hardware/dht_handler.cpp" "hardware/display.cpp" "hardware/motor.cpp" "hardware/scale.cpp"
                            "mqtt/mqtt.c"
                            "ota/ota.c"
                            "ui/logos.c"
                    INCLUDE_DIRS "." "config/." "core/." "hardware/." "include/." "mqtt/." "ota/." "ui/."
                    PRIV_REQUIRES app_update esp_https_ota nvs_flash esp_wifi esp_http_server esp_event lwip
                    REQUIRES HX711 Adafruit_GFX Adafruit_SSD1306 Stepper DHT dns_server
                    EMBED_FILES ui/captive.html)