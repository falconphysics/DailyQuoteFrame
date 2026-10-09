// preview_quotes — show what the frame will display, from a saved copy of the page.
//
//   curl -L -o page.html "https://www.vatican.va/content/leo-xiv/en/encyclicals/documents/20260515-magnifica-humanitas.html"
//   g++ -std=c++17 -I. -I../firmware/QuoteFrame preview_quotes.cpp -o preview_quotes
//
//   ./preview_quotes page.html            every entry in quotes.h, in order
//   ./preview_quotes page.html 42         paragraph 42 split into numbered
//                                         sentences, for picking new entries
//
// Uses the same scanner and sentence splitter as the firmware, so what it
// prints is what the frame will draw (curly quotes shown as plain ones).

#define PROGMEM
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "gfxfont.h"
#include "verse_layout.h"
#include "quote_source.h"
#include "quotes.h"

static std::string plain(const std::string& s) {
  std::string o;
  for (unsigned char c : s) {
    switch (c) {
      case GLYPH_LSQUO: case GLYPH_RSQUO: o += '\''; break;
      case GLYPH_LDQUO: case GLYPH_RDQUO: o += '"'; break;
      case GLYPH_MDASH: o += "--"; break;
      case GLYPH_NDASH: o += '-'; break;
      case GLYPH_HELLIP: o += "..."; break;
      case GLYPH_MIDDOT: o += '*'; break;
      default: o += (char)c;
    }
  }
  return o;
}

static size_t words(const std::string& s) {
  size_t n = 0; bool in = false;
  for (char c : s) { if (c == ' ') in = false; else if (!in) { in = true; n++; } }
  return n;
}

int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: %s page.html [paragraph]\n", argv[0]); return 2; }
  std::ifstream f(argv[1], std::ios::binary);
  if (!f) { fprintf(stderr, "can't read %s\n", argv[1]); return 2; }
  std::stringstream ss; ss << f.rdbuf();
  const std::string page = ss.str();

  if (argc >= 3) {
    const int n = atoi(argv[2]);
    ParagraphScanner sc({n});
    sc.feed(page.data(), page.size());
    sc.finish();
    if (!sc.done()) { printf("no paragraph %d (page goes up to %d)\n", n, sc.highestSeen()); return 1; }
    const auto sents = splitSentences(sc.found().at(n));
    printf("paragraph %d, %zu sentences:\n\n", n, sents.size());
    for (size_t i = 0; i < sents.size(); i++)
      printf("[%zu] (%zu words) %s\n\n", i, words(sents[i]), plain(sents[i]).c_str());
    return 0;
  }

  std::vector<int> want;
  for (int i = 0; i < QUOTE_COUNT; i++) want.push_back(QUOTES[i].paragraph);
  std::sort(want.begin(), want.end());
  want.erase(std::unique(want.begin(), want.end()), want.end());
  ParagraphScanner sc(want);
  sc.feed(page.data(), page.size());
  sc.finish();
  printf("page: %zu bytes, numbered paragraphs up to %d\n\n", page.size(), sc.highestSeen());
  if (sc.highestSeen() == 0) {
    printf("No numbered paragraphs found. To see how the page marks them up, run:\n"
           "  grep -o '.\\{300\\}Humanity, created' %s | head -1\n"
           "and paste the result.\n", argv[1]);
    return 1;
  }

  LayoutOptions o;
  o.footer = toPanelText("Pope Leo XIV \xC2\xB7 vatican.va");
  int bad = 0;
  for (int i = 0; i < QUOTE_COUNT; i++) {
    const QuoteRef& q = QUOTES[i];
    printf("%2d. {%u, %u, %u}  ", i + 1, q.paragraph, q.first, q.count);
    auto it = sc.found().find(q.paragraph);
    if (it == sc.found().end()) { printf("MISSING: paragraph not found\n\n"); bad++; continue; }
    std::string text, why;
    if (!pickSentences(it->second, q, text, why)) { printf("ERROR: %s\n\n", why.c_str()); bad++; continue; }
    Verse v; v.text = text; v.reference = toPanelText("Magnifica Humanitas, " + std::to_string(q.paragraph));
    const Layout L = layoutVerse(v, "Wednesday, September 30", o);
    printf("%zu words, size step %d, %d lines%s\n    %s\n\n", words(text), L.fontStep, L.lines,
           L.truncated ? ", TRUNCATED" : "", plain(text).c_str());
    if (L.truncated) bad++;
  }
  printf(bad ? "%d problem(s)\n" : "all %d entries OK\n", bad ? bad : QUOTE_COUNT);
  return bad ? 1 : 0;
}
