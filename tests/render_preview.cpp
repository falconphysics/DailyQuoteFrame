// Draw a layout the way Adafruit GFX would, to a PPM, for checking by eye.
//   g++ -std=c++17 -I. -I../firmware/QuoteFrame render_preview.cpp -o render_preview
//   ./render_preview out.ppm "reference" "verse text" ["date line"]
// Colours approximate what the Spectra 6 inks look like, not pure RGB.
#define PROGMEM
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "gfxfont.h"
#include "verse_layout.h"

static const int W = 480, H = 800;
static uint8_t img[H][W][3];

static void px(int x, int y, const uint8_t* c) {
  if (x >= 0 && x < W && y >= 0 && y < H) memcpy(img[y][x], c, 3);
}

// Same bit walk as Adafruit_GFX::drawChar with a custom font.
static void drawText(const GFXfont* f, int x, int y, const std::string& s, const uint8_t* col) {
  for (unsigned char ch : s) {
    const GFXglyph* g = glyphFor(f, ch);
    if (!g) continue;
    const uint8_t* bm = f->bitmap + g->bitmapOffset;
    uint8_t bits = 0, bit = 0;
    for (int yy = 0; yy < g->height; yy++)
      for (int xx = 0; xx < g->width; xx++) {
        if (!(bit++ & 7)) bits = *bm++;
        if (bits & 0x80) px(x + g->xOffset + xx, y + g->yOffset + yy, col);
        bits <<= 1;
      }
    x += g->xAdvance;
  }
}

int main(int argc, char** argv) {
  if (argc < 4) { fprintf(stderr, "usage: out.ppm reference text [date]\n"); return 2; }
  static const uint8_t white[3] = {0xE6, 0xE6, 0xDC}, black[3] = {0x1E, 0x1E, 0x22},
                       red[3] = {0xA6, 0x2C, 0x26}, blue[3] = {0x2C, 0x4C, 0x9A};
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) memcpy(img[y][x], white, 3);

  Verse v;
  v.reference = toPanelText(argv[2]);
  v.text = toPanelText(argv[3]);
  LayoutOptions o;
  o.footer = std::string("Pope Leo XIV ") + (char)GLYPH_MIDDOT + " vatican.va";
  Layout L = layoutVerse(v, argc > 4 ? toPanelText(argv[4]) : "", o);
  auto colour = [&](Role r) -> const uint8_t* {
    switch (r) { case ROLE_REFERENCE: case ROLE_RULE: return red; case ROLE_DATE: return blue; default: return black; }
  };
  for (auto& r : L.rects) for (int y = r.y; y < r.y + r.h; y++) for (int x = r.x; x < r.x + r.w; x++) px(x, y, colour(r.role));
  for (auto& t : L.texts) drawText(t.font, t.x, t.baseline, t.text, colour(t.role));

  FILE* fp = fopen(argv[1], "wb");
  fprintf(fp, "P6\n%d %d\n255\n", W, H);
  fwrite(img, 1, sizeof(img), fp);
  fclose(fp);
  printf("%s: step %d, %d lines%s\n", argv[1], L.fontStep, L.lines, L.truncated ? ", truncated" : "");
  return 0;
}
