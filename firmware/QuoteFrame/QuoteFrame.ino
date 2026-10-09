/*
 * QuoteFrame — one passage a day from Pope Leo XIV's encyclical
 * Magnifica Humanitas on the Waveshare ESP32-S3-PhotoPainter, in portrait.
 *
 * A variation on VerseFrame. Once a day, just after midnight:
 *   1. Wi-Fi on, set the clock from NTP
 *   2. Work out today's entry in quotes.h (paragraph + sentences)
 *   3. Stream the encyclical from vatican.va until that paragraph has gone
 *      by, and cut the chosen sentences out of it
 *   4. Lay it out (largest size that fits) and draw it
 *   5. Deep sleep until the same time tomorrow
 *
 * Only paragraph and sentence numbers are stored in the firmware; the words
 * are read from the Vatican's page each time.
 *
 * Press BOOT to skip ahead to the next passage.
 *
 * Board: ESP32S3 Dev Module, 16MB flash, USB CDC On Boot enabled.
 * Libraries: GxEPD2, Adafruit GFX Library.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <Wire.h>
#include <time.h>
#include <esp_sleep.h>
#include <esp_sntp.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>

#include "board_pins.h"
#include "config.h"
#include "pmu.h"
#include "panel.h"
#include "verse_text.h"
#include "verse_layout.h"
#include "quote_source.h"
#include "quotes.h"

GxEPD2_7C<PhotoPainterPanel, PhotoPainterPanel::HEIGHT / 4> display(
    PhotoPainterPanel(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));

static const char* USER_AGENT = "Mozilla/5.0 (compatible; PhotoPainter-QuoteFrame/1.0)";
static const size_t MAX_PAGE_BYTES = 3 * 1024 * 1024;   // give up after this much
static const uint32_t MAX_FETCH_MS = 120000;

// ---- Survives deep sleep ----------------------------------------------------
RTC_DATA_ATTR uint32_t rtcShownDay   = 0;      // yyyymmdd the panel's quote is for
RTC_DATA_ATTR uint32_t rtcShownHash  = 0;      // quote + date on the panel
RTC_DATA_ATTR int32_t  rtcShownIndex = -1;     // entry in QUOTES on the panel
RTC_DATA_ATTR int32_t  rtcShownPos   = -1;     // its position in the day-by-day sequence
RTC_DATA_ATTR int32_t  rtcSkip       = 0;      // BOOT presses: how far ahead of the calendar
RTC_DATA_ATTR uint32_t rtcFailures   = 0;      // consecutive failed wakes
RTC_DATA_ATTR bool     rtcErrorShown = false;  // an error screen is up
RTC_DATA_ATTR time_t   rtcSleptAt    = 0;      // for measuring the sleep clock's drift
RTC_DATA_ATTR uint32_t rtcSleptFor   = 0;
RTC_DATA_ATTR float    rtcClockScale = 1.0f;   // real seconds per sleep-timer second

std::string gBatteryNote;    // appended to the footer, e.g. " · 82%"
bool        gBatteryLow = false;

// ---- Sleep --------------------------------------------------------------------

void holdPmuLineThroughSleep() {
  digitalWrite(PIN_PMU_IRQ, HIGH);
  gpio_hold_en((gpio_num_t)PIN_PMU_IRQ);
  gpio_deep_sleep_hold_en();
}

// The ESP32's sleep timer runs off an internal RC oscillator that can be a few
// percent off, which over 24 hours is most of an hour. Each timer wake compares
// how long the sleep really took (by NTP) with what was asked for, and the next
// sleep is scaled to match.
void sleepFor(uint32_t realSeconds) {
  if (realSeconds < 60) realSeconds = 60;
  uint32_t timerSeconds = (uint32_t)(realSeconds / rtcClockScale);
  if (timerSeconds < 60) timerSeconds = 60;
  Serial.printf("sleeping %lus (timer %lus)\n", (unsigned long)realSeconds, (unsigned long)timerSeconds);
  Serial.flush();

  rtcSleptAt  = time(nullptr);
  rtcSleptFor = timerSeconds;

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  esp_sleep_enable_timer_wakeup((uint64_t)timerSeconds * 1000000ULL);
  rtc_gpio_pullup_en((gpio_num_t)PIN_BTN_BOOT);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BTN_BOOT, 0);   // BOOT pressed = low

  if (POWER_DOWN_IN_SLEEP) pmuPowerDownForSleep();
  holdPmuLineThroughSleep();
  esp_deep_sleep_start();
  while (true) {}
}

// Seconds from now until the next UPDATE_HOUR:UPDATE_MINUTE (tomorrow's if
// today's has passed). mktime handles month ends and DST changes.
uint32_t secondsUntilUpdate(const struct tm& now) {
  struct tm t = now;
  t.tm_hour = UPDATE_HOUR;
  t.tm_min = UPDATE_MINUTE;
  t.tm_sec = 0;
  t.tm_isdst = -1;
  time_t target = mktime(&t);
  const time_t nowT = mktime(const_cast<struct tm*>(&now));
  if (target <= nowT + 30) {
    t = now;
    t.tm_mday += 1;
    t.tm_hour = UPDATE_HOUR;
    t.tm_min = UPDATE_MINUTE;
    t.tm_sec = 0;
    t.tm_isdst = -1;
    target = mktime(&t);
  }
  return (uint32_t)(target - nowT);
}

void sleepAfterFailure(bool haveClock, const struct tm& now) {
  rtcFailures++;
  uint32_t m = RETRY_MINUTES;
  for (uint32_t i = 1; i < rtcFailures && m < RETRY_MAX_MINUTES; i++) m *= 2;
  if (m > RETRY_MAX_MINUTES) m = RETRY_MAX_MINUTES;
  uint32_t s = m * 60;
  // Never sleep past the next daily update because of a retry.
  if (haveClock) { const uint32_t u = secondsUntilUpdate(now); if (u < s) s = u; }
  Serial.printf("failure %lu, retrying in %lus\n", (unsigned long)rtcFailures, (unsigned long)s);
  sleepFor(s);
}

// ---- Network ------------------------------------------------------------------

// The clock keeps running through deep sleep (not accurately); read it without NTP.
void readSleepClock(struct tm& now, bool& haveClock) {
  setenv("TZ", TZ_INFO, 1);
  tzset();
  const time_t t = time(nullptr);
  localtime_r(&t, &now);
  haveClock = now.tm_year + 1900 >= 2025;
}

bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  const uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(200);
  if (WiFi.status() != WL_CONNECTED) return false;
  Serial.printf("wifi ok, %s, rssi %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  return true;
}

// true if NTP answered. `now` is filled either way when the clock is plausible
// (it keeps running through deep sleep, just not accurately).
bool syncClock(struct tm& now, bool& haveClock) {
  sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
  configTzTime(TZ_INFO, "pool.ntp.org", "time.nist.gov", "time.google.com");
  const uint32_t t0 = millis();
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && millis() - t0 < 10000) delay(100);
  const bool synced = sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED;
  const time_t t = time(nullptr);
  localtime_r(&t, &now);
  haveClock = now.tm_year + 1900 >= 2025;
  return synced;
}

// Stream the page until paragraph `para` has gone by; its text (panel bytes,
// footnote markers removed) ends up in `text`. Stops downloading as soon as
// the paragraph is complete, so early paragraphs cost less.
bool fetchParagraph(int para, std::string& text, String& err) {
  WiFiClientSecure client;
  client.setInsecure();   // public, read-only page; nothing secret is sent
  HTTPClient http;
  http.useHTTP10(true);   // no chunked encoding, so the stream is the plain body
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent(USER_AGENT);
  http.setTimeout(15000);
  if (!http.begin(client, QUOTE_PAGE_URL)) { err = "bad url"; return false; }
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    err = code < 0 ? String("http ") + http.errorToString(code) : String("http ") + code;
    http.end();
    return false;
  }

  ParagraphScanner scanner({para});
  auto* s = http.getStreamPtr();
  static char buf[2048];
  size_t total = 0;
  const uint32_t t0 = millis();
  bool found = false;
  while (millis() - t0 < MAX_FETCH_MS && total < MAX_PAGE_BYTES) {
    const int avail = s->available();
    if (avail > 0) {
      const int n = s->readBytes(buf, avail < (int)sizeof(buf) ? avail : (int)sizeof(buf));
      total += n;
      if (scanner.feed(buf, n)) { found = true; break; }
    } else if (!s->connected()) {
      break;
    } else {
      delay(5);
    }
  }
  if (!found) found = scanner.finish();
  http.end();
  Serial.printf("read %u bytes in %lu ms%s\n", (unsigned)total, (unsigned long)(millis() - t0),
                found ? "" : " (whole page)");
  if (!found) {
    if (total == 0) err = "empty response";
    else if (scanner.highestSeen() == 0) err = "no numbered paragraphs on the page";
    else err = String("no paragraph ") + para + " (the page goes up to " + scanner.highestSeen() + ")";
    return false;
  }
  text = scanner.found().at(para);
  return true;
}

bool fetchQuote(int index, Verse& v, String& err) {
  const QuoteRef& q = QUOTES[index];
  if (q.count == 1)
    Serial.printf("quote %d of %d: paragraph %u, sentence %u\n", index + 1, QUOTE_COUNT, q.paragraph, q.first);
  else
    Serial.printf("quote %d of %d: paragraph %u, sentences %u-%u\n", index + 1, QUOTE_COUNT,
                  q.paragraph, q.first, q.first + q.count - 1);
  std::string para, why;
  if (!fetchParagraph(q.paragraph, para, err)) return false;
  if (!pickSentences(para, q, v.text, why)) { err = why.c_str(); return false; }
  v.reference = toPanelText(std::string(QUOTE_SOURCE) + ", " + std::to_string(q.paragraph));
  return true;
}

static int wrapIndex(int32_t i) {
  i %= QUOTE_COUNT;
  return i < 0 ? i + QUOTE_COUNT : i;
}

// Which entry in QUOTES to show at a position in the daily sequence.
// SPREAD_QUOTES steps through the list by a stride of about 3/8 of its length
// (adjusted to share no factor with it, so every entry still comes up exactly
// once per cycle), which puts consecutive days in different chapters.
static int quoteForPosition(int32_t pos) {
  if (!SPREAD_QUOTES || QUOTE_COUNT < 3) return wrapIndex(pos);
  int stride = QUOTE_COUNT * 3 / 8;
  auto gcd = [](int a, int b) { while (b) { int t = a % b; a = b; b = t; } return a; };
  while (gcd(stride, QUOTE_COUNT) != 1) stride++;
  return wrapIndex((int32_t)((int64_t)wrapIndex(pos) * stride % QUOTE_COUNT));
}

// ---- Drawing ------------------------------------------------------------------

uint16_t roleColour(Role r) {
  switch (r) {
    case ROLE_VERSE:     return COLOR_VERSE;
    case ROLE_REFERENCE: return COLOR_REFERENCE;
    case ROLE_DATE:      return COLOR_DATE;
    case ROLE_RULE:      return COLOR_RULE;
    case ROLE_FOOTER:    return gBatteryLow ? GxEPD_RED : COLOR_FOOTER;
  }
  return GxEPD_BLACK;
}

void panelBegin() {
  pmuInitPanelRails();
  display.init(115200, true, 2, false);
  SPI.end();
  SPI.begin(PIN_EPD_SCK, -1, PIN_EPD_MOSI, PIN_EPD_CS);
  display.setRotation(PANEL_ROTATION);
  display.setTextWrap(false);
}

void drawLayout(const Layout& L) {
  if (CLEAR_BEFORE_DRAW) {
    Serial.println("clearing panel");
    display.clearScreen();
  }
  const uint32_t t0 = millis();
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    for (const DrawRect& r : L.rects) display.fillRect(r.x, r.y, r.w, r.h, roleColour(r.role));
    for (const DrawText& t : L.texts) {
      display.setFont(t.font);
      display.setTextColor(roleColour(t.role));
      display.setCursor(t.x, t.baseline);
      display.print(t.text.c_str());
    }
  } while (display.nextPage());
  Serial.printf("refresh took %lu ms\n", (unsigned long)(millis() - t0));
  display.hibernate();
}

LayoutOptions layoutOptions() {
  LayoutOptions o;
  o.width = display.width();
  o.height = display.height();
  o.centred = CENTRE_TEXT;
  o.showDate = SHOW_DATE;
  o.footer = gBatteryLow ? toPanelText("Battery low \xE2\x80\x94 please charge" + gBatteryNote)
                        : toPanelText(FOOTER_TEXT + gBatteryNote);
  if (o.width > o.height) o.margin = 40;   // landscape
  return o;
}

void renderVerse(const Verse& v, const std::string& dateLine) {
  panelBegin();
  const Layout L = layoutVerse(v, dateLine, layoutOptions());
  Serial.printf("layout: size step %d, %d lines%s\n", L.fontStep, L.lines, L.truncated ? ", truncated" : "");
  drawLayout(L);
}

// Only used when nothing has been drawn yet, so a new frame doesn't sit blank.
void renderMessage(const char* title, const String& detail) {
  panelBegin();
  Verse m;
  m.text = toPanelText(title);
  m.reference = toPanelText(detail.c_str());
  LayoutOptions o = layoutOptions();
  o.showDate = false;
  drawLayout(layoutVerse(m, "", o));
}

// ---- Main ---------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(300);

  gpio_hold_dis((gpio_num_t)PIN_PMU_IRQ);
  pinMode(PIN_PMU_IRQ, OUTPUT);
  digitalWrite(PIN_PMU_IRQ, HIGH);

  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  const bool byButton = cause == ESP_SLEEP_WAKEUP_EXT0;
  const bool byTimer  = cause == ESP_SLEEP_WAKEUP_TIMER;
  Serial.printf("\nQuoteFrame, wake: %s\n", byButton ? "BOOT button" : byTimer ? "timer" : "power on");

  // Battery, read before Wi-Fi loads it down.
  const BatteryInfo bat = pmuReadBattery();
  if (bat.present) {
    Serial.printf("battery %d mV, %d%%%s\n", bat.millivolts, bat.percent,
                  bat.charging ? ", charging" : bat.usb ? ", on USB" : "");
    if (SHOW_BATTERY && bat.percent >= 0) {
      gBatteryNote = std::string(bat.usb ? " \xC2\xB7 charging " : " \xC2\xB7 ") + std::to_string(bat.percent) + "%";
      if (std::string(FOOTER_TEXT).empty()) gBatteryNote = gBatteryNote.substr(4);   // no leading " · "
      gBatteryLow = !bat.usb && bat.percent < LOW_BATTERY_PERCENT;
    }
  } else {
    Serial.println("no battery detected");
  }

  // BOOT = the next passage. Counted before anything can fail, so a press
  // during an outage still moves on once the network is back.
  if (byButton && rtcShownPos >= 0) rtcSkip++;

  struct tm now = {};
  bool haveClock = false;

  if (!connectWifi()) {
    Serial.println("wifi failed");
    readSleepClock(now, haveClock);
    if (rtcShownHash == 0 && !rtcErrorShown) {
      renderMessage("Can\xE2\x80\x99t connect to Wi-Fi.", String("Check ") + WIFI_SSID);
      rtcErrorShown = true;
    }
    sleepAfterFailure(haveClock, now);
  }

  const bool synced = syncClock(now, haveClock);
  if (!synced) Serial.println(haveClock ? "ntp failed, using sleep clock" : "ntp failed, no clock");

  // Measure the sleep timer against real time (only across a full timer sleep).
  if (synced && byTimer && rtcSleptAt > 0 && rtcSleptFor >= 3600) {
    const double actual = difftime(time(nullptr), rtcSleptAt);
    const double ratio = actual / rtcSleptFor;
    if (ratio > 0.85 && ratio < 1.15) {
      rtcClockScale = (float)(0.5 * rtcClockScale + 0.5 * ratio);
      Serial.printf("sleep clock: slept %.0fs for %lus asked, scale now %.4f\n",
                    actual, (unsigned long)rtcSleptFor, rtcClockScale);
    }
  }

  const int y = now.tm_year + 1900, mo = now.tm_mon + 1, d = now.tm_mday;
  const uint32_t today = haveClock ? (uint32_t)(y * 10000 + mo * 100 + d) : 0;

  // Woke a little early (the timer runs fast), and today's quote is already up.
  if (!byButton && haveClock && rtcShownDay == today && !rtcErrorShown) {
    Serial.println("today's quote is already showing");
    sleepFor(secondsUntilUpdate(now));
  }

  // One passage per calendar day, in list order; BOOT presses shift it along.
  // Without a clock, just move to the next one.
  const int32_t pos = haveClock ? daysFromCivil(y, mo, d) + rtcSkip : rtcShownPos + 1;
  const int index = quoteForPosition(pos);

  Verse v;
  String err;
  if (!fetchQuote(index, v, err)) {
    Serial.printf("no quote: %s\n", err.c_str());
    if (rtcShownHash == 0 && !rtcErrorShown) {
      renderMessage("Couldn\xE2\x80\x99t get today\xE2\x80\x99s quote.", err);
      rtcErrorShown = true;
    }
    sleepAfterFailure(haveClock, now);
  }

  const std::string dateLine = haveClock ? formatDate(y, mo, d) : std::string();
  Serial.printf("%s: %s\n", v.reference.c_str(), v.text.c_str());

  const uint32_t hash = verseHash(v) ^ (today * 2654435761u);
  if (hash != rtcShownHash || byButton || rtcErrorShown) {
    Serial.println("rendering");
    renderVerse(v, toPanelText(dateLine));
    rtcShownHash = hash;
    rtcErrorShown = false;
  } else {
    Serial.println("unchanged");
  }
  rtcShownIndex = index;
  rtcShownPos = pos;
  rtcShownDay = today;
  rtcFailures = 0;

  if (!haveClock) sleepFor(24 * 3600);
  sleepFor(secondsUntilUpdate(now));
}

void loop() {}
