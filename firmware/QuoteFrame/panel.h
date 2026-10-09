// panel.h — GxEPD2's GDEP073E01 driver with a longer busy timeout.
// The stock timeout is shorter than this panel's ~25 s refresh.
#pragma once

#include <GxEPD2_7C.h>

class PhotoPainterPanel : public GxEPD2_730c_GDEP073E01 {
  public:
    PhotoPainterPanel(int16_t cs, int16_t dc, int16_t rst, int16_t busy)
      : GxEPD2_730c_GDEP073E01(cs, dc, rst, busy) {
      _busy_timeout = 45000000UL;   // 45 s, in microseconds
    }
};
