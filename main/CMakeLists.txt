idf_component_register(SRCS "ota/ota.c" "main.cpp"
                    INCLUDE_DIRS "." "ota/."
                    PRIV_REQUIRES app_update esp_https_ota nvs_flash

                    REQUIRES HX711 Adafruit_GFX Adafruit_SSD1306 Stepper Blynk DHT)