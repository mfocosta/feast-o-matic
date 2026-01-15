#include <stdio.h>
#include "Arduino.h"
#include "HX711.h"
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Stepper.h>
#include "DHT.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"

/* Project includes */
#include "ota.h"
#include "mqtt.h"
#include "init.h"
#include "wifi.h"
#include "pins.h"
#include "dht_handler.h"
#include "display.h"
#include "scale.h"
#include "motor.h"

static const char *TAG = "main_app";


bool verifyOta() {
    return true; /* Placeholder for OTA verification logic */
}

void configurePins() {
    /* OUTPUT */
    gpio_config_t io_conf = {};
    io_conf.intr_type     = GPIO_INTR_DISABLE;
    io_conf.mode          = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask  = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en    = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

}

extern "C" void app_main()
{

    /* Initialize system components */
    initialize_system();

    /** 
     * Initializes Arduino framework
     * Calls nvs_flash_init()
     * Performs OTA verification if needed
     */
    initArduino(); 

    initialize_wifi();

    configurePins();

    dht.begin();

    // Inicializar o display OLED
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Endereço I2C 0x3C para o display OLED
        ESP_LOGI(TAG, "Failed to initialize OLED display");
    }

    // Exibe o logotipo durante o carregamento
    display_show_feast_logo();
    
    display.clearDisplay();
    display.setTextSize(1.5);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Sistema Iniciado");
    display.display();

    // Iniciar a célula de carga
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scale.set_scale(CONFIG_HX711_CALIBRATION_FACTOR);
    scale.tare();  // Zera a balança com a tigela vazia

    // Configurar a velocidade do motor de passo
    stepper.setSpeed(10);  // Velocidade em rotações por minuto (RPM)
    
    Serial.println("Sistema pronto. Coloque a tigela vazia.");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Pronto. Coloque a tigela.");
    display.display();

    /**
     * Start FreeRTOS tasks
     */
    xTaskCreate(&ota_task, "ota_task", 12288, NULL, tskIDLE_PRIORITY + 1, NULL);

    mqtt_app_start();

    ESP_LOGI(TAG, "Setup done, entering loop");

    for(;;) {  // Loop principal
        // Only execute main loop if OTA is not running
        if (xSemaphoreTake(ota_semaphore, 0) == pdTRUE) {
            xSemaphoreGive(ota_semaphore);  // Immediately release
            
        // Ler o peso atual da célula de carga
        current_weight = scale.get_units();  
        
        // Exibir o peso atual no monitor serial e no display OLED
        Serial.print("Peso lido: ");
        Serial.print(current_weight, 2);
        Serial.println(" g");

        if (true) {
            humidade = dht.readHumidity(); // Lê a humidade
            temperatura = dht.readTemperature(); // Lê a temperatura em Celsius
            if (mqtt_client != NULL) {
                char temp_buffer[50];
                snprintf(temp_buffer, sizeof(temp_buffer), "%.1f", temperatura);
                esp_mqtt_client_publish(mqtt_client, "test", temp_buffer, 0, 0, 0);
            }

            // Mostra os dados no OLED
        if (isnan(temperatura) || isnan(humidade)) {
            display.println("Erro ao ler DHT11");
            } else {
            display.clearDisplay();
            display.setCursor(0, 0);
            display.println("Sensor DHT11:");
            display.print("Temperatura: ");
            display.print(temperatura);
            display.println(" C");
            display.print("Humidade: ");
            display.print(humidade);
            display.println(" %");
            }
        } else {

        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Peso atual:");
        display.setCursor(0, 20);
        display.print(current_weight, 2);
        display.println(" g");

        }

        // Controle automático baseado no peso
        if (current_weight < target_weight) {
            if (previous_weight != current_weight) {  // Executa apenas se o peso mudou
                gpio_set_level(LEDPIN, 1);
                
                stepper.step(-2048);  // Gira o motor para dispensar comida
                stepper.step(256);    // Ajusta o motor de volta
                previous_weight = current_weight;  // Atualiza o peso anterior
                //display.setCursor(0, 40);
                //display.println("Dispensando comida...");
                //display.display();  // Atualiza o conteúdo do display OLED
            }
            } else {
            display.setCursor(0, 40);
            //display.println("Peso alvo atingido.");
            Serial.println("Peso alvo atingido! Motor parado.");
            disableMotor();   // Desativa o motor após atingir o peso alvo
            gpio_set_level(LEDPIN, 0);
            }

        display.display();  // Atualiza o conteúdo do display OLED
} else {
            // OTA is running, skip main operations and sleep
            ESP_LOGD(TAG, "OTA in progress, main loop sleeping");
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);  // Aguardar meio segundo antes da próxima leitura
    }


    // WARNING: if program reaches end of function app_main() the MCU will restart ????????
}