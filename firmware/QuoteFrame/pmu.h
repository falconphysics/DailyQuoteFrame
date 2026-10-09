// pmu.h — the AXP2101 power chip: panel rails, battery level, and switching
// everything off during deep sleep.
//
// Rails on this board:
//   DCDC1        3.3 V for the ESP32 itself. Never touched.
//   ALDO3/ALDO4  the e-paper panel.
//   the rest     audio codec and amplifier, SD card, sensors. Unused here.
//
// Switching rails off can't simply be undone over I2C on the next wake:
// chips that share the I2C bus lose power too, hold the bus lines low, and
// the AXP2101 stops answering. So, as in Waveshare's power-consumption test
// (04_PowerConsumptionTest in their repo), the AXP2101 is put into its own
// sleep mode first, with "IRQ pin pulled low" as its wake-up signal and
// "restore the rails" as what waking does. On the next ESP32 wake,
// pmuWakeIfAsleep() pulses GPIO21 low; the AXP2101 turns the rails back on
// by itself and the bus works again.
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <driver/gpio.h>
#include "board_pins.h"

#define AXP2101_ADDR            0x34
#define AXP2101_STATUS1         0x00   // bit3 = battery present, bit5 = USB power good
#define AXP2101_STATUS2         0x01   // bits 6:5 = 01 when charging
#define AXP2101_SLEEP_WAKE      0x26   // bit0 sleep, bit2 rails on wake, bit3 PWROK, bit4 IRQ-pin wake
#define AXP2101_IRQ_EN0         0x40   // 0x40-0x42 interrupt enables
#define AXP2101_IRQ_STS0        0x48   // 0x48-0x4A interrupt status, write 1 to clear
#define AXP2101_ADC_CTRL        0x30   // bit0 = battery voltage measurement
#define AXP2101_ADC_VBAT_H      0x34   // battery mV, 14 bits across 0x34/0x35
#define AXP2101_ADC_VBAT_L      0x35
#define AXP2101_BAT_DET_CTRL    0x68   // bit0 = battery detection
#define AXP2101_DC_ONOFF        0x80   // bit0 = DCDC1 (ESP32) ... bit4 = DCDC5
#define AXP2101_LDO_ONOFF_CTRL0 0x90   // ALDO1-4, BLDO1-2, CPUSLDO, DLDO1
#define AXP2101_LDO_ONOFF_CTRL1 0x91   // bit0 = DLDO2
#define AXP2101_LDO_VOL2_CTRL   0x94   // ALDO3 voltage
#define AXP2101_LDO_VOL3_CTRL   0x95   // ALDO4 voltage
#define AXP2101_BAT_PERCENT     0xA4   // fuel gauge, 0-100
#define AXP2101_VOL_3V3         0x1C   // 0.5 V + 28 × 0.1 V

inline bool pmuReadRegister(uint8_t reg, uint8_t& value) {
  Wire.beginTransmission(AXP2101_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)AXP2101_ADDR, (uint8_t)1) != 1) return false;
  value = Wire.read();
  return true;
}

inline bool pmuWriteRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(AXP2101_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

inline bool pmuSetBits(uint8_t reg, uint8_t mask) {
  uint8_t v;
  if (!pmuReadRegister(reg, v)) return false;
  return (v & mask) == mask || pmuWriteRegister(reg, v | mask);
}

inline bool pmuProbe(uint32_t hz) {
  Wire.end();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, hz);
  delay(10);
  uint8_t v;
  return pmuReadRegister(AXP2101_STATUS1, v);
}

// Pull the AXP2101's IRQ line low for `ms`, then back high. While the chip is
// in sleep mode this wakes it, and it switches its rails back on.
inline void pmuPulseIrq(uint32_t ms) {
  gpio_hold_dis((gpio_num_t)PIN_PMU_IRQ);
  pinMode(PIN_PMU_IRQ, OUTPUT);
  digitalWrite(PIN_PMU_IRQ, LOW);
  delay(ms);
  digitalWrite(PIN_PMU_IRQ, HIGH);
  delay(150);   // rails coming up
}

// Start I2C and make sure the chip answers, waking it if it was left asleep.
inline bool pmuBegin() {
  static bool ok = false;
  if (ok) return true;
  if (pmuProbe(400000)) return ok = true;
  static const uint32_t pulses[] = {50, 300, 1000};
  for (uint32_t ms : pulses) {
    Serial.printf("PMU: not answering, waking it (%lu ms pulse)\n", (unsigned long)ms);
    pmuPulseIrq(ms);
    if (pmuProbe(400000) || pmuProbe(100000)) return ok = true;
  }
  Serial.println("PMU: no response at 0x34");
  return false;
}

inline bool pmuInitPanelRails() {
  if (!pmuBegin()) return false;

  uint8_t onoff = 0;
  if (!pmuReadRegister(AXP2101_LDO_ONOFF_CTRL0, onoff)) return false;

  uint8_t vol = 0;
  if (pmuReadRegister(AXP2101_LDO_VOL2_CTRL, vol) && (vol & 0x1F) != AXP2101_VOL_3V3)
    pmuWriteRegister(AXP2101_LDO_VOL2_CTRL, (vol & 0xE0) | AXP2101_VOL_3V3);
  if (pmuReadRegister(AXP2101_LDO_VOL3_CTRL, vol) && (vol & 0x1F) != AXP2101_VOL_3V3)
    pmuWriteRegister(AXP2101_LDO_VOL3_CTRL, (vol & 0xE0) | AXP2101_VOL_3V3);

  const uint8_t wanted = onoff | (1 << 2) | (1 << 3);
  if (wanted != onoff) {
    if (!pmuWriteRegister(AXP2101_LDO_ONOFF_CTRL0, wanted)) {
      Serial.println("PMU: failed to enable ALDO3/ALDO4");
      return false;
    }
    Serial.println("PMU: enabled panel rails");
    delay(100);
  }
  return true;
}

struct BatteryInfo {
  bool present = false;
  bool usb = false;        // running from USB power
  bool charging = false;
  int  millivolts = 0;
  int  percent = -1;       // fuel gauge; -1 if unknown
};

inline BatteryInfo pmuReadBattery() {
  BatteryInfo b;
  if (!pmuBegin()) return b;
  pmuSetBits(AXP2101_ADC_CTRL, 0x01);       // battery voltage measurement on
  pmuSetBits(AXP2101_BAT_DET_CTRL, 0x01);   // battery detection on
  uint8_t s1 = 0, s2 = 0, h = 0, l = 0, pct = 0;
  if (!pmuReadRegister(AXP2101_STATUS1, s1)) return b;
  pmuReadRegister(AXP2101_STATUS2, s2);
  b.present = s1 & (1 << 3);
  b.usb = s1 & (1 << 5);
  b.charging = ((s2 >> 5) & 0x03) == 0x01;
  if (b.present) {
    if (pmuReadRegister(AXP2101_ADC_VBAT_H, h) && pmuReadRegister(AXP2101_ADC_VBAT_L, l))
      b.millivolts = ((h & 0x1F) << 8) | l;
    if (pmuReadRegister(AXP2101_BAT_PERCENT, pct) && pct <= 100) b.percent = pct;
  }
  return b;
}

// Put the AXP2101 to sleep with everything except DCDC1 (the ESP32's own
// supply) switched off. Same order as Waveshare's axp_basic_sleep_start().
inline bool pmuPowerDownForSleep() {
  if (!pmuBegin()) return false;
  for (uint8_t i = 0; i < 3; i++) pmuWriteRegister(AXP2101_IRQ_EN0 + i, 0x00);   // no interrupts:
  for (uint8_t i = 0; i < 3; i++) pmuWriteRegister(AXP2101_IRQ_STS0 + i, 0xFF);  // the chip mustn't pull IRQ itself
  uint8_t w = 0;
  if (!pmuReadRegister(AXP2101_SLEEP_WAKE, w)) return false;
  w |= (1 << 2);              // on wake, rails as they were before sleep
  w &= ~(1 << 3);             // don't pull PWROK low on wake (that would reset the ESP32)
  w |= (1 << 4);              // wake when the IRQ pin is pulled low
  if (!pmuWriteRegister(AXP2101_SLEEP_WAKE, w)) return false;
  if (!pmuWriteRegister(AXP2101_SLEEP_WAKE, w | 0x01)) return false;   // enter sleep mode

  uint8_t dc = 0;
  if (pmuReadRegister(AXP2101_DC_ONOFF, dc)) pmuWriteRegister(AXP2101_DC_ONOFF, dc & 0x01);
  pmuWriteRegister(AXP2101_LDO_ONOFF_CTRL0, 0x00);
  uint8_t l1 = 0;
  if (pmuReadRegister(AXP2101_LDO_ONOFF_CTRL1, l1)) pmuWriteRegister(AXP2101_LDO_ONOFF_CTRL1, l1 & ~0x01);
  Serial.println("PMU: asleep, rails off");
  return true;
}
