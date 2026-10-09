// board_pins.h — ESP32-S3-PhotoPainter GPIO map.
// Same as the message board; taken from Waveshare's bsp_config.h.
#pragma once

// 7.3" Spectra-6 panel (SPI)
#define PIN_EPD_DC     8
#define PIN_EPD_CS     9
#define PIN_EPD_SCK    10
#define PIN_EPD_MOSI   11
#define PIN_EPD_RST    12
#define PIN_EPD_BUSY   13

// Buttons
#define PIN_BTN_BOOT   0    // BOOT — wake and check right now
#define PIN_BTN_KEY    4
#define PIN_BTN_PWR    5

// Status LEDs
#define PIN_LED_RED    45
#define PIN_LED_GREEN  42

// I2C bus — AXP2101 PMU at 0x34
#define PIN_I2C_SDA    47
#define PIN_I2C_SCL    48

// AXP2101 interrupt line, driven HIGH and held through deep sleep.
// A low pulse is the PMU wake signal; if it drifts low the rails can drop.
#define PIN_PMU_IRQ    21

// Panel geometry (native, rotation 0)
#define EPD_WIDTH      800
#define EPD_HEIGHT     480
