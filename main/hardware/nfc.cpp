#include <Adafruit_PN532.h>
#include <string.h>
#include "esp_log.h"
#include "nfc.h"

static const char *TAG = "nfc";

/* PN532 on I2C, no IRQ / Reset pins connected */
static Adafruit_PN532 pn532(-1, -1);
static bool nfc_available = false;

/* Bowl-weight data lives on NTAG2xx page 6 (4 bytes = 1 float) */
#define BOWL_WEIGHT_PAGE 6

bool nfc_init(void)
{
    if (!pn532.begin()) {
        ESP_LOGE(TAG, "PN532 begin failed");
        return false;
    }

    uint32_t fw = pn532.getFirmwareVersion();
    if (!fw) {
        ESP_LOGE(TAG, "PN532 not found");
        return false;
    }

    ESP_LOGI(TAG, "Found PN5%02X  FW %d.%d",
             (unsigned)((fw >> 24) & 0xFF),
             (int)((fw >> 16) & 0xFF),
             (int)((fw >> 8)  & 0xFF));

    /* One HW retry so readPassiveTargetID fails fast (~150 ms) */
    pn532.setPassiveActivationRetries(0x01);

    nfc_available = true;
    return true;
}

bool nfc_poll_uid(uint8_t *uid, uint8_t *uidLen)
{
    if (!nfc_available) return false;
    return pn532.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, uidLen, 150);
}

bool nfc_read_bowl_weight(float *weight_grams)
{
    uint8_t buf[4];
    if (!pn532.ntag2xx_ReadPage(BOWL_WEIGHT_PAGE, buf))
        return false;

    float w;
    memcpy(&w, buf, sizeof(w));
    *weight_grams = w;
    return true;
}

bool nfc_write_bowl_weight(float weight_grams)
{
    /* Detect tag first */
    uint8_t uid[7], uidLen;
    if (!pn532.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 500))
        return false;

    uint8_t buf[4];
    memcpy(buf, &weight_grams, sizeof(weight_grams));
    return pn532.ntag2xx_WritePage(BOWL_WEIGHT_PAGE, buf);
}

