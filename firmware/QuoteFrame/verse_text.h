// verse_text.h — pull the verse out of a verseoftheday.com page and turn it
// into bytes the panel fonts can draw.
//
// The page's <head> carries the verse in its share tags:
//   <meta property="og:title"       content="Verse of the Day: Philippians 1:9-10">
//   <meta property="og:description" content="This is my prayer: that your love ...">
//   <meta property="og:url"         content="https://www.verseoftheday.com/en/09302026/">
// Those are written for Facebook/X previews, so they change far less often than
// the page layout does. twitter:title / twitter:description are the fallback.
//
// No Arduino code in here, so tests/test_text.cpp can run it on a PC.

#pragma once

#include <string>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cctype>

// Font slots above '~' (fonts/ were generated with these, see fonts/README).
#define GLYPH_LSQUO  0x7F   // ‘
#define GLYPH_RSQUO  0x80   // ’
#define GLYPH_LDQUO  0x81   // “
#define GLYPH_RDQUO  0x82   // ”
#define GLYPH_MDASH  0x83   // —
#define GLYPH_NDASH  0x84   // –
#define GLYPH_HELLIP 0x85   // …
#define GLYPH_MIDDOT 0x86   // ·

struct Verse {
  std::string text;        // panel bytes (see toPanelText)
  std::string reference;   // panel bytes, e.g. "Philippians 1:9-10"
  int year = 0, month = 0, day = 0;   // the page's own date, from og:url; 0 if unknown
};

// ---- HTML -------------------------------------------------------------------

static inline std::string lowerAscii(std::string s) {
  for (auto& c : s) c = (char)tolower((unsigned char)c);
  return s;
}

// Value of attribute `name` inside one tag's text, or "" if absent.
// Handles "double", 'single' and unquoted values.
static inline bool tagAttribute(const std::string& tag, const char* name, std::string& out) {
  const std::string lt = lowerAscii(tag);
  const std::string key = name;
  size_t pos = 0;
  while ((pos = lt.find(key, pos)) != std::string::npos) {
    // must be a whole attribute name: preceded by whitespace, followed by '='
    const bool startOk = pos > 0 && isspace((unsigned char)lt[pos - 1]);
    size_t p = pos + key.size();
    while (p < lt.size() && isspace((unsigned char)lt[p])) p++;
    if (!startOk || p >= lt.size() || lt[p] != '=') { pos += key.size(); continue; }
    p++;
    while (p < lt.size() && isspace((unsigned char)lt[p])) p++;
    if (p >= lt.size()) return false;
    if (lt[p] == '"' || lt[p] == '\'') {
      const char q = lt[p];
      const size_t end = lt.find(q, p + 1);
      if (end == std::string::npos) return false;
      out = tag.substr(p + 1, end - p - 1);
    } else {
      size_t end = p;
      while (end < lt.size() && !isspace((unsigned char)lt[end]) && lt[end] != '>') end++;
      out = tag.substr(p, end - p);
    }
    return true;
  }
  return false;
}

// content="" of the first <meta property=key> or <meta name=key>.
static inline bool metaContent(const std::string& html, const char* key, std::string& out) {
  const std::string lh = lowerAscii(html);
  size_t pos = 0;
  while ((pos = lh.find("<meta", pos)) != std::string::npos) {
    const size_t end = lh.find('>', pos);
    if (end == std::string::npos) return false;
    const std::string tag = html.substr(pos, end - pos + 1);
    std::string which;
    if ((tagAttribute(tag, "property", which) || tagAttribute(tag, "name", which)) &&
        lowerAscii(which) == key &&
        tagAttribute(tag, "content", out)) {
      return true;
    }
    pos = end;
  }
  return false;
}

static inline void appendUtf8(std::string& s, uint32_t cp) {
  if (cp < 0x80) s += (char)cp;
  else if (cp < 0x800) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 0x3F)); }
  else if (cp < 0x10000) {
    s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 0x3F));
    s += (char)(0x80 | (cp & 0x3F));
  } else {
    s += (char)(0xF0 | (cp >> 18)); s += (char)(0x80 | ((cp >> 12) & 0x3F));
    s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F));
  }
}

// &amp; &#8217; &#x2019; &rsquo; ... -> UTF-8. Unknown entities are left as-is.
static inline std::string decodeEntities(const std::string& in) {
  static const struct { const char* name; uint32_t cp; } named[] = {
    {"amp", '&'}, {"lt", '<'}, {"gt", '>'}, {"quot", '"'}, {"apos", '\''},
    {"nbsp", 0xA0}, {"lsquo", 0x2018}, {"rsquo", 0x2019}, {"ldquo", 0x201C},
    {"rdquo", 0x201D}, {"mdash", 0x2014}, {"ndash", 0x2013}, {"hellip", 0x2026},
    {"middot", 0xB7}, {"sbquo", 0x201A}, {"bdquo", 0x201E},
  };
  std::string out;
  out.reserve(in.size());
  for (size_t i = 0; i < in.size(); i++) {
    if (in[i] != '&') { out += in[i]; continue; }
    const size_t semi = in.find(';', i);
    if (semi == std::string::npos || semi - i > 10) { out += '&'; continue; }
    const std::string ent = in.substr(i + 1, semi - i - 1);
    uint32_t cp = 0;
    bool ok = false;
    if (!ent.empty() && ent[0] == '#') {
      char* e = nullptr;
      const bool hex = ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X');
      cp = (uint32_t)strtoul(ent.c_str() + (hex ? 2 : 1), &e, hex ? 16 : 10);
      ok = e && *e == 0 && ent.size() > (size_t)(hex ? 2 : 1) && cp > 0 && cp < 0x110000;
    } else {
      for (auto& n : named) if (ent == n.name) { cp = n.cp; ok = true; break; }
    }
    if (!ok) { out += '&'; continue; }
    appendUtf8(out, cp);
    i = semi;
  }
  return out;
}

// ---- UTF-8 -> panel bytes ---------------------------------------------------

// Accented Latin-1 letters fall back to the plain letter; the fonts are ASCII
// plus the punctuation slots above.
static inline char foldLatin1(uint32_t cp) {
  static const char* map =
    "AAAAAAACEEEEIIII"   // C0-CF
    "DNOOOOO*OUUUUYTs"   // D0-DF
    "aaaaaaaceeeeiiii"   // E0-EF
    "dnooooo/ouuuuyty";  // F0-FF
  if (cp >= 0xC0 && cp <= 0xFF) return map[cp - 0xC0];
  return 0;
}

// Decode UTF-8, map to font bytes, collapse runs of whitespace, trim.
static inline std::string toPanelText(const std::string& utf8) {
  std::string out;
  bool pendingSpace = false;
  auto put = [&](char c) {
    if (pendingSpace && !out.empty()) out += ' ';
    pendingSpace = false;
    out += c;
  };
  for (size_t i = 0; i < utf8.size();) {
    const uint8_t b = (uint8_t)utf8[i];
    uint32_t cp;
    int len;
    if (b < 0x80)      { cp = b; len = 1; }
    else if ((b & 0xE0) == 0xC0) { cp = b & 0x1F; len = 2; }
    else if ((b & 0xF0) == 0xE0) { cp = b & 0x0F; len = 3; }
    else if ((b & 0xF8) == 0xF0) { cp = b & 0x07; len = 4; }
    else { i++; continue; }                         // stray continuation byte
    if (i + len > utf8.size()) break;
    bool bad = false;
    for (int k = 1; k < len; k++) {
      const uint8_t c = (uint8_t)utf8[i + k];
      if ((c & 0xC0) != 0x80) { bad = true; break; }
      cp = (cp << 6) | (c & 0x3F);
    }
    if (bad) { i++; continue; }
    i += len;

    if (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == 0xA0 ||
        (cp >= 0x2000 && cp <= 0x200A)) { pendingSpace = true; continue; }
    if (cp >= 0x21 && cp <= 0x7E) { put((char)cp); continue; }
    switch (cp) {
      case 0x2018: case 0x201B: put((char)GLYPH_LSQUO); continue;
      case 0x2019: case 0x02BC: put((char)GLYPH_RSQUO); continue;
      case 0x201C: case 0x201F: put((char)GLYPH_LDQUO); continue;
      case 0x201D:              put((char)GLYPH_RDQUO); continue;
      case 0x201A:              put(','); continue;
      case 0x201E:              put((char)GLYPH_LDQUO); continue;
      case 0x2014: case 0x2015: put((char)GLYPH_MDASH); continue;
      case 0x2013: case 0x2012: put((char)GLYPH_NDASH); continue;
      case 0x2026:              put((char)GLYPH_HELLIP); continue;
      case 0xB7:   case 0x2022: put((char)GLYPH_MIDDOT); continue;
      case 0x200B: case 0xFEFF: case 0xAD: continue;   // invisible
    }
    if (const char f = foldLatin1(cp)) { put(f); continue; }
    // anything else (emoji, other scripts) is dropped
  }
  return out;
}

// ---- The page ---------------------------------------------------------------

// ".../en/09302026/" -> 2026, 9, 30
static inline bool dateFromUrl(const std::string& url, int& y, int& m, int& d) {
  for (size_t i = 0; i + 8 <= url.size(); i++) {
    if (i > 0 && url[i - 1] != '/') continue;
    bool digits = true;
    for (int k = 0; k < 8; k++) if (!isdigit((unsigned char)url[i + k])) { digits = false; break; }
    if (!digits || (i + 8 < url.size() && url[i + 8] != '/')) continue;
    m = atoi(url.substr(i, 2).c_str());
    d = atoi(url.substr(i + 2, 2).c_str());
    y = atoi(url.substr(i + 4, 4).c_str());
    if (m >= 1 && m <= 12 && d >= 1 && d <= 31 && y >= 2000) return true;
  }
  return false;
}

// "Verse of the Day: Philippians 1:9-10" -> "Philippians 1:9-10"
static inline std::string stripTitlePrefix(const std::string& title) {
  const std::string lt = lowerAscii(title);
  const std::string prefix = "verse of the day";
  size_t p = lt.find(prefix);
  if (p == std::string::npos) return title;
  p += prefix.size();
  while (p < title.size() && (title[p] == ':' || title[p] == ' ' || title[p] == '-' ||
                              (uint8_t)title[p] == (uint8_t)GLYPH_MDASH ||
                              (uint8_t)title[p] == (uint8_t)GLYPH_NDASH)) p++;
  return p < title.size() ? title.substr(p) : std::string();
}

// Everything the frame needs, or false with a reason.
static inline bool parseVersePage(const std::string& html, Verse& v, std::string& why) {
  std::string title, desc, url;
  if (!metaContent(html, "og:description", desc)) metaContent(html, "twitter:description", desc);
  if (!metaContent(html, "og:title", title))      metaContent(html, "twitter:title", title);
  if (!metaContent(html, "og:url", url))          metaContent(html, "twitter:url", url);

  v.text = toPanelText(decodeEntities(desc));
  v.reference = stripTitlePrefix(toPanelText(decodeEntities(title)));
  v.year = v.month = v.day = 0;
  if (!url.empty()) dateFromUrl(url, v.year, v.month, v.day);

  if (v.text.empty())      { why = "no og:description on the page"; return false; }
  if (v.reference.empty()) { why = "no og:title on the page"; return false; }
  if (v.text.size() < 10)  { why = "verse text too short: " + v.text; return false; }
  return true;
}

// FNV-1a, to tell whether the verse on the panel changed.
static inline uint32_t verseHash(const Verse& v) {
  uint32_t h = 2166136261u;
  for (unsigned char c : v.reference) { h ^= c; h *= 16777619u; }
  h ^= 0xFF; h *= 16777619u;
  for (unsigned char c : v.text)      { h ^= c; h *= 16777619u; }
  return h;
}

// ---- Dates ------------------------------------------------------------------

static inline int dayOfWeek(int y, int m, int d) {   // 0 = Sunday
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

// "Wednesday, September 30"
static inline std::string formatDate(int y, int m, int d) {
  static const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                               "Thursday", "Friday", "Saturday"};
  static const char* months[] = {"January", "February", "March", "April", "May", "June", "July",
                                 "August", "September", "October", "November", "December"};
  if (m < 1 || m > 12 || d < 1 || d > 31) return "";
  return std::string(days[dayOfWeek(y, m, d)]) + ", " + months[m - 1] + " " + std::to_string(d);
}
