// config.h — QuoteFrame settings. Edit the Wi-Fi details; the rest is optional.
#pragma once

// ---- Wi-Fi ------------------------------------------------------------------
#define WIFI_SSID      "your-network"
#define WIFI_PASSWORD  "your-password"

// ---- Time -------------------------------------------------------------------
// POSIX time zone. Michigan / US Eastern:
#define TZ_INFO        "EST5EDT,M3.2.0,M11.1.0"

// When the next quote goes up each day, local time.
#define UPDATE_HOUR    0
#define UPDATE_MINUTE  15

// ---- Source -----------------------------------------------------------------
// The document the quotes come from, and how the attribution line reads:
// "— Magnifica Humanitas, 15". The list of passages is in quotes.h.
#define QUOTE_PAGE_URL "https://www.vatican.va/content/leo-xiv/en/encyclicals/documents/20260515-magnifica-humanitas.html"
#define QUOTE_SOURCE   "Magnifica Humanitas"

// true: consecutive days come from different parts of the letter.
// false: work straight down quotes.h in reading order.
#define SPREAD_QUOTES  true

// ---- Screen -----------------------------------------------------------------
// 1 or 3 = portrait (which one is "up" depends on how the frame stands;
// try 1, and if the text is upside down use 3). 0 or 2 = landscape.
#define PANEL_ROTATION 1

#define CENTRE_TEXT    true    // false = left-aligned, reference flush right
#define SHOW_DATE      true    // "Wednesday, September 30" across the top

// Small line at the bottom. "" for none.
#define FOOTER_TEXT    "Pope Leo XIV · vatican.va"

// GxEPD_BLACK, GxEPD_WHITE, GxEPD_RED, GxEPD_BLUE, GxEPD_GREEN, GxEPD_YELLOW
#define COLOR_VERSE      GxEPD_BLACK   // the quote
#define COLOR_REFERENCE  GxEPD_RED     // "— Magnifica Humanitas, 15"
#define COLOR_DATE       GxEPD_BLUE
#define COLOR_RULE       GxEPD_RED
#define COLOR_FOOTER     GxEPD_BLACK

// Blank the panel to white before drawing. Only worth turning on if the
// previous quote shows faintly behind the new one.
#define CLEAR_BEFORE_DRAW false

// ---- Battery ----------------------------------------------------------------
// Switch off the board's unused power rails (audio, SD card, sensors and the
// panel's own) during deep sleep. This is what makes the battery last; only
// set it to false if the frame stops drawing after flashing this version.
#define POWER_DOWN_IN_SLEEP  true

// Add the battery level to the footer: "Pope Leo XIV · vatican.va · 82%".
// Below LOW_BATTERY_PERCENT the footer turns red and asks to be charged.
#define SHOW_BATTERY         true
#define LOW_BATTERY_PERCENT  15

// ---- When things go wrong ---------------------------------------------------
// A failed fetch keeps the current quote up and tries again after this many
// minutes, doubling each time up to RETRY_MAX_MINUTES.
#define RETRY_MINUTES      30
#define RETRY_MAX_MINUTES  240
