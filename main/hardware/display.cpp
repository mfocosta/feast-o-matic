#include <Adafruit_SSD1306.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "display.h"
#include "events.h"
#include <bitmaps.h>

static const char *TAG = "display_handler";

#define SCREEN_WIDTH  128  
#define SCREEN_HEIGHT 64  
#define OLED_RESET    -1  /* Not used */

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool lastReadNext = HIGH;
bool lastReadPrev = HIGH;
bool stateNext = HIGH;
bool statePrev = HIGH;

// Variáveis dinâmicas
bool wifiConectado = true;

// Estado do Menu (0 = Dashboard Dinâmico, 1 = Bitmap1, 2 = Bitmap2...)
int menuPage = 0;
const int totalPages = 3; // Dashboard + 2 Bitmaps de exemplo

// ================= DESENHO DA INTERFACE (PÁGINA 0) =================
void desenharInterfaceFixa() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Barra de Topo
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 2);
  display.print("Feast-O-Matic");
  
  if(wifiConectado) {
    display.setCursor(85, 2);
    display.print("WiFi ON"); 
  }

  // Divisória e Rótulos
  display.drawFastVLine(64, 13, 40, SSD1306_WHITE);
  display.setCursor(5, 18); display.print("RESERV.");
  display.setCursor(75, 18); display.print("NA TACA");

  // Rodapé
  display.drawFastHLine(0, 52, 128, SSD1306_WHITE);
  display.display();
}

void display_show_status(int bowl_weight, float temp, int hum, int reservoir_level) {
  display.fillRect(5, 30, 55, 20, SSD1306_BLACK);   
  display.fillRect(75, 30, 50, 20, SSD1306_BLACK);  
  display.fillRect(0, 55, 128, 9, SSD1306_BLACK);   

  display.setTextSize(2);
  display.setCursor(5, 32);
  display.print(reservoir_level); display.setTextSize(1); display.print("%");

  display.setTextSize(2);
  display.setCursor(75, 32);
  display.print(bowl_weight); display.setTextSize(1); display.print("g");

  display.setTextSize(1);
  display.setCursor(1, 56);
  display.print("Temp:"); display.print(temp, 1); display.print(" C");
  display.setCursor(75, 56);
  display.print("Hum: "); display.print(hum); display.print("%");

  display.display();
}

/*
// ================= LOOP =================
void loop() {
  // 1. Simulação de Sensores
  temp += 0.01;
  pesoTaca = random(115, 125);

  // 2. Lógica dos Botões
  verificarBotoes();

  // 3. Renderização Condicional
  if (menuPage == 0) {
    display_show_status();
  }
}*/

/*void verificarBotoes() {
  bool readingNext = digitalRead(BTN_NEXT);
  bool readingPrev = digitalRead(BTN_PREV);

  // Botão NEXT (Avançar)
  if (readingNext != lastReadNext) lastDebounceNext = millis();
  if ((millis() - lastDebounceNext) > debounceDelay) {
    if (readingNext != stateNext) {
      stateNext = readingNext;
      if (stateNext == LOW) {
        menuPage = (menuPage + 1) % totalPages;
        mudarPagina();
      }
    }
  }
  lastReadNext = readingNext;

  // Botão PREV (Voltar / Home)
  if (readingPrev != lastReadPrev) {
    lastDebouncePrev = millis();
    if (readingPrev == LOW) timePressedPrev = millis();
  }
  if ((millis() - lastDebouncePrev) > debounceDelay) {
    if (readingPrev != statePrev) {
      statePrev = readingPrev;
      if (statePrev == HIGH && (millis() - timePressedPrev < longPressTime)) {
        menuPage--;
        if (menuPage < 0) menuPage = totalPages - 1;
        mudarPagina();
      }
    }
    if (statePrev == LOW && (millis() - timePressedPrev >= longPressTime)) {
      if (menuPage != 0) { menuPage = 0; mudarPagina(); delay(200); }
    }
  }
  lastReadPrev = readingPrev;
}*/

void mudarPagina() {
  display.clearDisplay();
  if (menuPage == 0) {
    desenharInterfaceFixa();
  } else if (menuPage == 1) {
    // Exemplo: Desenhar o teu bitmap2
    display.drawBitmap(0, 0, bitmap2, 128, 64, WHITE);
  } else if (menuPage == 2) {
    // Exemplo: Desenhar o teu bitmap3
     display.drawBitmap(0, 0, bitmap3, 128, 64, WHITE);
  }
  display.display();
}

void display_show_feast_logo(void) {
    display.clearDisplay(); 
    display.drawBitmap(0, 0, feastLogoUpgraded, 128, 64, WHITE);
    display.display();
}

void display_init(void) {
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        ESP_LOGW(TAG, "OLED display initialization failed - continuing without display");
        return;
    }

    /* ToDo: EventGroupBit */
    ESP_LOGI(TAG, "OLED display initialized successfully");
}

/* ── Display task ─────────────────────────────────────────────────────── */

void display_task(void *pvParameter)
{
    display_msg_t msg;

    display_init();

    display_show_feast_logo();
    vTaskDelay(pdMS_TO_TICKS(5000));

    desenharInterfaceFixa();  

    for (;;) {
        if (xQueueReceive(display_queue, &msg, portMAX_DELAY) != pdTRUE) continue;

        switch (msg.type) {
        case DISPLAY_MSG_STATUS:
            display_show_status((int)msg.data.status.weight,
                                msg.data.status.temp,
                                (int)msg.data.status.hum,
                                80); /* ToDo: reservoir level hardcoded for now */
            break;
        case DISPLAY_MSG_DISPENSING:
            //display_show_dispensing(msg.data.dispensing.grams);
            break;
        case DISPLAY_MSG_ERROR:
            //display_show_error(msg.data.error);
            break;
        default:
            break;
        }
    }
}

/* ── Post helpers (non-blocking, safe to call from any task) ─────────── */

void display_post_status(float weight, float temp, float hum)
{
    display_msg_t msg = { .type = DISPLAY_MSG_STATUS };
    msg.data.status.weight = weight;
    msg.data.status.temp   = temp;
    msg.data.status.hum    = hum;
    xQueueSend(display_queue, &msg, 0); /* non-blocking; drop if queue full */
}

void display_post_dispensing(int target_grams)
{
    display_msg_t msg = { .type = DISPLAY_MSG_DISPENSING };
    msg.data.dispensing.grams = target_grams;
    xQueueSendToFront(display_queue, &msg, 0); /* high-priority: jump the queue */
}

void display_post_error(const char *message)
{
    display_msg_t msg = { .type = DISPLAY_MSG_ERROR };
    strncpy(msg.data.error, message, sizeof(msg.data.error) - 1);
    msg.data.error[sizeof(msg.data.error) - 1] = '\0';
    xQueueSendToFront(display_queue, &msg, 0);
}

void display_post_startup(void)
{
    display_msg_t msg = { .type = DISPLAY_MSG_STARTUP };
    xQueueSend(display_queue, &msg, 0);
}

void display_post_logo(void)
{
    display_msg_t msg = { .type = DISPLAY_MSG_LOGO };
    xQueueSend(display_queue, &msg, 0);
}

