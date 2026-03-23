#ifndef NFC_H
#define NFC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool    nfc_init(void);
bool    nfc_poll_uid(uint8_t *uid, uint8_t *uidLen);
bool    nfc_read_bowl_weight(float *weight_grams);
bool    nfc_write_bowl_weight(float weight_grams);

#ifdef __cplusplus
}
#endif

#endif // NFC_H