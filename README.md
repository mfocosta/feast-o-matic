# 🥘 Feast-o-Matic

**Feast-o-Matic** é um projeto de firmware desenvolvido usando framework **ESP-IDF**, integrando o **núcleo Arduino-ESP32** como um componente.



## ⚙️ Overview

O Feast-o-Matic é um alimentador automático para animais de estimação, concebido para
gerir de forma inteligente as refeições, garantindo horários regulares, porções controladas e
monitorização do consumo.​



## 🧩 Estrutura do Projeto

root/ \
├── main/               # Main application (FreeRTOS tasks, initialization, etc.) \
│ └── idf_component.yml # Arduino core integrated as an ESP-IDF component \
├── components/         # Required third-party Arduino libraries \
└── sdkconfig           # ESP-IDF configuration file




## 🚀 Building the Project

### Requirements
- [ESP-IDF v5.5.1](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32/get-started/index.html)
- [Arduino-ESP32 (v3.3.2) as an ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
- [Espressif-IDE](https://docs.espressif.com/projects/espressif-ide/en/latest/downloads.html) or [ESP-IDF Extension for VS Code](https://github.com/espressif/vscode-esp-idf-extension/blob/master/README.md)

