# ESP32-Demo

## Introduction
Up and Running example with ESP32. We have plan to use it on smart flower pots.

## Up and Running
First of all, set your WiFi credentials in `src/main.c`:

```c
/* Wifi Credentials */
#define DEMO_WIFI_SSID "ssid"
#define DEMO_WIFI_PASSWORD "secret"
```

The use [PlatformIO](https://platformio.org/) run program the board and have fun:

```sh
pio run
pio run --target upload
pio device monitor
```
