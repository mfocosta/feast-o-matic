#include <DHT.h>
#include "config/pins.h"

#define DHTTYPE DHT11

// Inicializa o sensor DHT
DHT dht(DHTPIN, DHTTYPE);

float temperatura;
float humidade;