// verse_layout.h — place the date, verse, reference and footer on the panel.
//
// Works from the GFXfont tables directly (glyph advances and bounds), so the
// same code runs on the frame and in tests/render_preview.cpp, which draws the
// result to a PNG for checking without hardware.
//
// Picks the largest verse size that fits, re-wraps it so the lines come out
// even (no single word stranded on the last line), and centres the whole block
// in the space between the date and the footer.

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "verse_text.h"
#include "fonts.h"

enum Role : uint8_t { ROLE_VERSE, ROLE_REFERENCE, ROLE_DATE, ROLE_RULE, ROLE_FOOTER };

struct DrawText { const GFXfont* font; int x; int baseline; std::string text; Role role; };
struct DrawRect { int x, y, w, h; Role role; };

struct Layout {
  std::vector<DrawText> texts;
  std::vector<DrawRect> rects;
  int fontStep = -1;      // index into FONT_LADDER
  int lines = 0;
  bool truncated = false;
};

struct LayoutOptions {
  int width = 480, height = 800;   // after rotation
  int margin = 36;                 // left/right/top/bottom
  bool centred = true;             // centre each line; false = ragged right
  bool showDate = true;
  std::string footer;              // panel bytes; empty = none
};

// ---- Font metrics -----------------------------------------------------------

static inline const GFXglyph* glyphFor(const GFXfont* f, uint8_t c) {
  if (c < f->first || c > f->last) return nullptr;
  return &f->glyph[c - f->first];
}

static inline int textWidth(const GFXfont* f, const std::string& s) {
  int w = 0;
  for (unsigned char c : s) if (const GFXglyph* g = glyphFor(f, c)) w += g->xAdvance;
  return w;
}

// Pixels above / below the baseline, over letters, digits and punctuation.
static inline int fontAscent(const GFXfont* f) {
  int a = 0;
  for (int c = f->first; c <= f->last; c++) { const GFXglyph* g = glyphFor(f, (uint8_t)c); if (-g->yOffset > a) a = -g->yOffset; }
  return a;
}
static inline int fontDescent(const GFXfont* f) {
  int d = 0;
  for (int c = f->first; c <= f->last; c++) { const GFXglyph* g = glyphFor(f, (uint8_t)c); if (g->yOffset + g->height > d) d = g->yOffset + g->height; }
  return d;
}

// ---- Wrapping -----------------------------------------------------------------

struct Token { std::string text; bool spaceBefore; };

// Split on spaces, and also after an em dash joined to the next word
// ("heart—and"), so long clauses can break there.
static inline std::vector<Token> tokenize(const std::string& s) {
  std::vector<Token> out;
  std::string cur;
  bool space = false;
  for (size_t i = 0; i < s.size(); i++) {
    const unsigned char c = (unsigned char)s[i];
    if (c == ' ') {
      if (!cur.empty()) { out.push_back({cur, space}); cur.clear(); }
      space = true;
      continue;
    }
    cur += (char)c;
    if (c == GLYPH_MDASH && i + 1 < s.size() && s[i + 1] != ' ') {
      out.push_back({cur, space}); cur.clear(); space = false;
    }
  }
  if (!cur.empty()) out.push_back({cur, space});
  if (!out.empty()) out[0].spaceBefore = false;
  return out;
}

static inline std::vector<std::string> wrapTokens(const GFXfont* f, const std::vector<Token>& toks, int maxWidth) {
  std::vector<std::string> lines;
  std::string line;
  const int spaceW = textWidth(f, " ");
  int w = 0;
  for (const Token& t : toks) {
    const int tw = textWidth(f, t.text);
    const int add = (line.empty() ? 0 : (t.spaceBefore ? spaceW : 0)) + tw;
    if (!line.empty() && w + add > maxWidth) {
      lines.push_back(line);
      line = t.text;
      w = tw;
    } else {
      if (!line.empty() && t.spaceBefore) line += ' ';
      line += t.text;
      w += add;
    }
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

// Narrowest width that still needs no more lines than maxWidth does.
static inline std::vector<std::string> wrapBalanced(const GFXfont* f, const std::vector<Token>& toks, int maxWidth) {
  std::vector<std::string> best = wrapTokens(f, toks, maxWidth);
  if (best.size() < 2) return best;
  int lo = maxWidth / 2, hi = maxWidth;
  while (lo < hi) {
    const int mid = (lo + hi) / 2;
    if (wrapTokens(f, toks, mid).size() <= best.size()) hi = mid; else lo = mid + 1;
  }
  return wrapTokens(f, toks, hi);
}

// ---- Layout -------------------------------------------------------------------

static inline void centreText(Layout& L, const GFXfont* f, const std::string& s, int width, int baseline, Role r) {
  L.texts.push_back({f, (width - textWidth(f, s)) / 2, baseline, s, r});
}

static inline Layout layoutVerse(const Verse& v, const std::string& dateLine, const LayoutOptions& o) {
  Layout L;
  const int textW = o.width - 2 * o.margin;

  // Header: date, then a short rule.
  int top = o.margin;
  if (o.showDate && !dateLine.empty()) {
    const int base = top + fontAscent(FONT_DATE);
    centreText(L, FONT_DATE, dateLine, o.width, base, ROLE_DATE);
    top = base + fontDescent(FONT_DATE) + 14;
    L.rects.push_back({(o.width - 64) / 2, top, 64, 3, ROLE_RULE});
    top += 3;
  }
  top += 24;

  // Footer.
  int bottom = o.height - o.margin;
  if (!o.footer.empty()) {
    const int base = bottom - fontDescent(FONT_FOOTER);
    centreText(L, FONT_FOOTER, o.footer, o.width, base, ROLE_FOOTER);
    bottom = base - fontAscent(FONT_FOOTER) - 24;
  }
  const int areaH = bottom - top;

  const std::vector<Token> toks = tokenize(v.text);
  std::string ref;
  ref += (char)GLYPH_MDASH;
  ref += ' ';
  ref += v.reference;

  // The reference steps down on its own when it is too wide for the text's
  // size ("— Magnifica Humanitas, 15" is longer than most Bible references),
  // so a long reference doesn't make a short quote small.
  auto refFont = [&](int i) {
    for (int k = i; k < FONT_LADDER_COUNT; k++)
      if (textWidth(FONT_LADDER[k].reference, ref) <= textW) return FONT_LADDER[k].reference;
    return FONT_LADDER[FONT_LADDER_COUNT - 1].reference;
  };

  // Largest size whose block fits.
  std::vector<std::string> lines;
  int step = FONT_LADDER_COUNT - 1;
  for (int i = 0; i < FONT_LADDER_COUNT; i++) {
    const GFXfont* vf = FONT_LADDER[i].verse;
    const GFXfont* rf = refFont(i);
    std::vector<std::string> ls = o.centred ? wrapBalanced(vf, toks, textW) : wrapTokens(vf, toks, textW);
    bool fitsWide = true;
    for (auto& l : ls) if (textWidth(vf, l) > textW) fitsWide = false;
    const int h = fontAscent(vf) + (int)(ls.size() - 1) * vf->yAdvance + fontDescent(vf) +
                  rf->yAdvance * 3 / 4 + fontAscent(rf) + fontDescent(rf);
    lines = ls;
    step = i;
    if (fitsWide && h <= areaH) break;
  }
  L.fontStep = step;
  const GFXfont* vf = FONT_LADDER[step].verse;
  const GFXfont* rf = refFont(step);

  // Still too tall at the smallest size: keep what fits and end with "…".
  const int refH = rf->yAdvance * 3 / 4 + fontAscent(rf) + fontDescent(rf);
  auto blockH = [&](size_t n) {
    return fontAscent(vf) + (int)(n - 1) * vf->yAdvance + fontDescent(vf) + refH;
  };
  if (blockH(lines.size()) > areaH) {
    while (lines.size() > 1 && blockH(lines.size()) > areaH) lines.pop_back();
    std::string& last = lines.back();
    const std::string ell(1, (char)GLYPH_HELLIP);
    while (!last.empty() && textWidth(vf, last + ell) > textW) {
      const size_t sp = last.rfind(' ');
      last = sp == std::string::npos ? std::string() : last.substr(0, sp);
    }
    last += ell;
    L.truncated = true;
  }
  L.lines = (int)lines.size();

  // Centre the block vertically; a touch above centre reads better.
  const int h = blockH(lines.size());
  int y = top + (areaH - h) * 9 / 20;
  if (y < top) y = top;

  int base = y + fontAscent(vf);
  int widest = 0;
  for (auto& l : lines) if (textWidth(vf, l) > widest) widest = textWidth(vf, l);
  for (auto& l : lines) {
    const int x = o.centred ? (o.width - textWidth(vf, l)) / 2 : o.margin;
    L.texts.push_back({vf, x, base, l, ROLE_VERSE});
    base += vf->yAdvance;
  }
  base = base - vf->yAdvance + fontDescent(vf) + rf->yAdvance * 3 / 4 + fontAscent(rf);
  if (o.centred) centreText(L, rf, ref, o.width, base, ROLE_REFERENCE);
  else {
    int rx = o.margin + widest - textWidth(rf, ref);   // flush with the longest line
    if (rx < o.margin) rx = o.margin;
    L.texts.push_back({rf, rx, base, ref, ROLE_REFERENCE});
  }
  return L;
}
