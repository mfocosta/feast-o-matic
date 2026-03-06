idf_component_register(SRCS "main.cpp"
                            "core/init.c" "core/wifi.c" "core/feeder.cpp" "core/scheduler.c"
                            "hardware/dht_handler.cpp" "hardware/display.cpp" "hardware/motor.cpp" "hardware/scale.cpp" "hardware/tof.cpp"
                            "mqtt/mqtt.c"
                            "ota/ota.c"
                            "ui/bitmaps.cpp"
                    INCLUDE_DIRS "." "config/." "core/." "hardware/." "include/." "mqtt/." "ota/." "ui/."
                    PRIV_REQUIRES app_update esp_https_ota nvs_flash esp_wifi esp_http_server esp_event lwip esp_driver_gpio
                    REQUIRES HX711 Adafruit_GFX Adafruit_SSD1306 Stepper DHT dns_server Adafruit_VL53L1X
                    EMBED_FILES ui/captive.html)