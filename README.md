# Gooldoon Node

## Introduction

Up and Running example with ESP32 and use it as a simple smart flower pot.

## Up and Running

First of all, set your WiFi credentials in `src/main.c`:

```c
/* Wifi Credentials */
#define DEMO_WIFI_SSID "ssid"
#define DEMO_WIFI_PASSWORD "secret"
```

The use [PlatformIO](https://platformio.org/) run program the board and have fun:

```sh
pio lib -e esp32dev install
pio run
pio run --target upload
pio device monitor
```
