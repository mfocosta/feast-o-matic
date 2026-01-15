#include <Adafruit_SSD1306.h>
#include "display.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* Function to draw the full screen with a given bitmap */
void drawFullScreen(const uint8_t* logoBitmap) {
    display.clearDisplay(); 
    display.drawBitmap(0, 0, logoBitmap, 128, 64, WHITE);
    display.display();
}