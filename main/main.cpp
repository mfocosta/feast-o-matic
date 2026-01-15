#include <stdio.h>
#include "Arduino.h"
#include "HX711.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Stepper.h>
#include <WiFi.h>
#include "DHT.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* Project includes */
#include "ota.h"
#include "mqtt.h"
#include "system.h"

static const char *TAG = "main_app";


// Credenciais WiFi (configuradas via menuconfig)
char ssid[] = CONFIG_WIFI_SSID;
char pass[] = CONFIG_WIFI_PASSWORD;

extern "C" void app_main()
{
    /* Check if OTA update is pending verification */
    ota_verification_check();
    
    /* Initialize system components */
    initialize_system();


    ESP_ERROR_CHECK(esp_event_loop_create_default());


    initArduino();


    Serial.begin(115200);  // Inicia a comunicação serial
    dht.begin();
    pinMode(ledPin, OUTPUT);
    
    // Add small delay to allow esp_event_loop to initialize properly
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    // Initialize WiFi before OTA task
    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    
    int wifi_attempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifi_attempts < 20) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        wifi_attempts++;
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        ESP_LOGI(TAG, "WiFi connected. IP: %s", WiFi.localIP().toString().c_str());
    } else {
        ESP_LOGW(TAG, "WiFi connection failed after %d attempts", wifi_attempts);
    }
    
    // Inicializar o display OLED
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Endereço I2C 0x3C para o display OLED
        Serial.println(F("Falha ao inicializar o display OLED"));
        for(;;);
    }

    // Exibe o logotipo durante o carregamento
    showLogo();
    
    display.clearDisplay();
    display.setTextSize(1.5);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Sistema Iniciado");
    display.display();

    // Iniciar a célula de carga
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scale.set_scale(calibration_factor);
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

        if (displayOption == 0) {
            humidade = dht.readHumidity(); // Lê a umidade
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
        if (!manual_motor_control) {  // Verifica se o controle manual está desativado
            if (current_weight < target_weight) {
            if (previous_weight != current_weight) {  // Executa apenas se o peso mudou
                for(int dutyCycle = 0; dutyCycle <= 255; dutyCycle++){   
                // changing the LED brightness with PWM
                analogWrite(ledPin, dutyCycle);
                
                }

                // decrease the LED brightness
                for(int dutyCycle = 255; dutyCycle >= 0; dutyCycle--){
                // changing the LED brightness with PWM
                analogWrite(ledPin, dutyCycle);
                
                }

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
            desativarMotor();   // Desativa o motor após atingir o peso alvo
            }
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