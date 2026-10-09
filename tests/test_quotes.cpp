// Host tests for quote_source.h (and the layout with quote-shaped text).
//   g++ -std=c++17 -I. -I../firmware/QuoteFrame test_quotes.cpp -o test_quotes && ./test_quotes
//
// The sample page below is made up. It copies the structure of the Vatican's
// encyclical pages (numbered <p> paragraphs, footnote links like
// <a href="#_ftn8">[8]</a>, <i> titles, "(cf. Gen 1:1)" references, curly
// quotes as entities) without any of the encyclical's text.

#define PROGMEM
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <random>
#include "gfxfont.h"
#include "verse_layout.h"
#include "quote_source.h"
#include "quotes.h"

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("FAIL %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static std::string show(const std::string& s) {
  std::string o;
  for (unsigned char c : s) {
    switch (c) {
      case GLYPH_LSQUO: o += "'"; break;   case GLYPH_RSQUO: o += "'"; break;
      case GLYPH_LDQUO: o += "\""; break;  case GLYPH_RDQUO: o += "\""; break;
      case GLYPH_MDASH: o += "--"; break;  case GLYPH_NDASH: o += "-"; break;
      case GLYPH_HELLIP: o += "..."; break; case GLYPH_MIDDOT: o += "*"; break;
      default: o += (char)c;
    }
  }
  return o;
}

static const char* PAGE =
  "<!DOCTYPE html><html><head><title>Letter</title>\n"
  "<script>var x = '<p>99. Not a paragraph.</p>';</script>\n"
  "<SCRIPT type=\"text/javascript\">for (var i=0;i<n;i++) { if (a<b && c>d) document.write('<p>97. No.</p>'); }</SCRIPT>\n"
  "<script src=\"x.js\"></script><script>var y = 1 </ 2;</script>\n"
  "<style>p { margin: 0 }</style></head><body>\n"
  "<div class=\"nav\"><a href=\"/\">Home</a> <a href=\"/en\">English</a></div>\n"
  "<!-- <p>98. Commented out.</p> -->\n"
  "<p align=\"center\"><b>INTRODUCTION</b></p>\n"
  "<p align=\"justify\">1. The garden needs tending. Every season brings its own work: "
  "pruning, planting and waiting. <i>Hortus conclusus</i> is an old image.<a href=\"#_ftn1\" name=\"_ftnref1\">[1]</a> "
  "Who will do it?</p>\n"
  "<p align=\"justify\">2.&nbsp;A lantern was lit (cf. <i>Ps</i> 119:105). "
  "St. Anne&#8217;s bell rang twice. It said &#8220;come home.&#8221; <a href=\"#_ftn2\">[2]</a> "
  "Everyone came &mdash; even the neighbours &hellip; and the dog. "
  "What more could we ask? Nothing!</p>\n"
  "<p align=\"justify\"><b>A heading that is not numbered</b></p>\n"
  "<p>3. Short one.</p>\n"
  "<p>4. Values like 3.5 or v2.0 do not split, nor does e.g. this. "
  "Walls rise slowly (<i>Neh</i> 2&#8211;6). J. Smith agreed.<a href=\"#_ftn3\">[3]</a> "
  "The end.</p>\n"
  "<p>12. Twelve comes after four here, which is fine.<br>A line break ends the paragraph.</p>\n"
  "<p>____________________</p>\n"
  "<p><a href=\"#_ftnref1\" name=\"_ftn1\">[1]</a> A footnote, not paragraph 1.</p>\n"
  "</body></html>";

static std::map<int, std::string> scanAll(const std::string& page, const std::vector<int>& want,
                                          std::mt19937* rng = nullptr) {
  ParagraphScanner sc(want);
  size_t i = 0;
  while (i < page.size()) {
    size_t n = rng ? 1 + (*rng)() % 37 : page.size();
    if (i + n > page.size()) n = page.size() - i;
    if (sc.feed(page.data() + i, n)) break;
    i += n;
  }
  sc.finish();
  return sc.found();
}

int main() {
  const std::string page = PAGE;

  // 1. Finding paragraphs, whatever the chunking.
  {
    auto f = scanAll(page, {1, 2, 3, 4, 12});
    CHECK(f.size() == 5, "found %zu of 5", f.size());
    CHECK(show(f[1]) == "The garden needs tending. Every season brings its own work: pruning, planting and "
                        "waiting. Hortus conclusus is an old image. Who will do it?", "p1 = %s", show(f[1]).c_str());
    CHECK(show(f[2]).rfind("A lantern was lit (cf. Ps 119:105). St. Anne's", 0) == 0, "p2 = %s", show(f[2]).c_str());
    CHECK(show(f[12]) == "Twelve comes after four here, which is fine.", "p12 = %s", show(f[12]).c_str());
    std::mt19937 rng(7);
    for (int trial = 0; trial < 200; trial++) {
      auto g = scanAll(page, {1, 2, 3, 4, 12}, &rng);
      if (g != f) { CHECK(false, "chunked scan differs on trial %d", trial); break; }
    }
    // A byte at a time.
    ParagraphScanner one({4});
    bool done = false;
    for (char c : page) if ((done = one.feed(&c, 1))) break;
    CHECK(done && one.found().at(4) == f[4], "byte-at-a-time scan");
  }

  // 2. Script, style, comments and footnotes aren't paragraphs.
  {
    ParagraphScanner sc({97, 98, 99});
    sc.feed(page.data(), page.size());
    sc.finish();
    CHECK(sc.found().empty(), "found a paragraph inside script/comment");
    CHECK(sc.highestSeen() == 12, "highest = %d", sc.highestSeen());
  }

  // 3. Stops as soon as the paragraph ends.
  {
    ParagraphScanner sc({2});
    size_t used = 0;
    for (; used < page.size(); used++) if (sc.feed(&page[used], 1)) break;
    CHECK(used < page.find("<p>3."), "read past paragraph 3 before stopping");
  }

  // 4. Sentences.
  {
    auto f = scanAll(page, {1, 2, 4});
    auto s1 = splitSentences(f[1]);
    CHECK(s1.size() == 4, "p1 sentences %zu", s1.size());
    auto s2 = splitSentences(f[2]);
    // "(cf. Ps 119:105)." | "St. Anne's bell rang twice." | "It said "come home."" |
    // "Everyone came -- even ... and the dog." | "What more could we ask?" | "Nothing!"
    CHECK(s2.size() == 6, "p2 sentences %zu", s2.size());
    for (size_t i = 0; i < s2.size(); i++) printf("    p2[%zu] %s\n", i, show(s2[i]).c_str());
    if (s2.size() == 6) {
      CHECK(show(s2[0]) == "A lantern was lit (cf. Ps 119:105).", "s2[0] %s", show(s2[0]).c_str());
      CHECK(show(s2[2]) == "It said \"come home.\"", "s2[2] %s", show(s2[2]).c_str());
      CHECK(show(s2[3]) == "Everyone came -- even the neighbours ... and the dog.", "s2[3] %s", show(s2[3]).c_str());
      CHECK(show(s2[5]) == "Nothing!", "s2[5] %s", show(s2[5]).c_str());
    }
    auto s4 = splitSentences(f[4]);
    for (size_t i = 0; i < s4.size(); i++) printf("    p4[%zu] %s\n", i, show(s4[i]).c_str());
    CHECK(s4.size() == 4, "p4 sentences %zu", s4.size());
    if (s4.size() == 4) CHECK(show(s4[1]) == "Walls rise slowly (Neh 2-6).", "s4[1] %s", show(s4[1]).c_str());

    std::string out, why;
    CHECK(pickSentences(f[2], {2, 4, 2}, out, why) && show(out) == "What more could we ask? Nothing!", "pick %s", show(out).c_str());
    CHECK(!pickSentences(f[2], {2, 5, 2}, out, why) && why.find("has 6 sentences") != std::string::npos, "range: %s", why.c_str());
    CHECK(!pickSentences(f[2], {2, 0, 0}, out, why), "zero count accepted");
  }

  // 4b. Dotted initials don't end a sentence; a lone capital letter still is one.
  {
    auto t = splitSentences("The author J.R.R. Tolkien wrote it. Then we read it.");
    CHECK(t.size() == 2, "initials split into %zu", t.size());
    auto u = splitSentences("Plan B. Then plan C.");
    CHECK(u.size() == 1 || u.size() == 2, "plan B");   // ambiguous by design; just mustn't crash
  }

  // 5. Footnote stripping.
  CHECK(ParagraphScanner::stripFootnotes("clear.\x82 [8] Indeed [[110]] [10] so") == "clear.\x82 Indeed so",
        "%s", ParagraphScanner::stripFootnotes("clear.\x82 [8] Indeed [[110]] [10] so").c_str());
  CHECK(ParagraphScanner::stripFootnotes("a paradigm [119] in our world") == "a paradigm in our world", "mid");
  CHECK(ParagraphScanner::stripFootnotes("end [5], then") == "end, then", "comma");
  CHECK(ParagraphScanner::stripFootnotes("keep [a] and [12b]") == "keep [a] and [12b]", "non-numeric kept");

  // 6. Numbered-paragraph detection.
  {
    int n; std::string t;
    CHECK(ParagraphScanner::numberedParagraph("  17.&nbsp;Text", n, t) && n == 17 && t == "Text", "nbsp");
    CHECK(!ParagraphScanner::numberedParagraph("2026 was a year.", n, t), "year taken as number");
    CHECK(!ParagraphScanner::numberedParagraph("3.5 percent", n, t), "decimal taken as number");
    CHECK(!ParagraphScanner::numberedParagraph("[1] Footnote.", n, t), "footnote taken as paragraph");
  }

  // 7. Calendar rotation.
  CHECK(daysFromCivil(1970, 1, 1) == 0, "epoch");
  CHECK(daysFromCivil(2026, 9, 30) == 20726, "2026-09-30 = %d", daysFromCivil(2026, 9, 30));
  CHECK(daysFromCivil(2028, 3, 1) - daysFromCivil(2028, 2, 28) == 2, "leap day");
  CHECK(daysFromCivil(2027, 1, 1) - daysFromCivil(2026, 12, 31) == 1, "new year");

  // 8. The quote list is sane.
  {
    bool ok = true;
    for (int i = 0; i < QUOTE_COUNT; i++) {
      if (QUOTES[i].paragraph == 0 || QUOTES[i].count == 0 || QUOTES[i].count > 4) ok = false;
      for (int j = 0; j < i; j++)
        if (QUOTES[i].paragraph == QUOTES[j].paragraph && QUOTES[i].first < QUOTES[j].first + QUOTES[j].count &&
            QUOTES[j].first < QUOTES[i].first + QUOTES[i].count) ok = false;   // overlap
    }
    CHECK(ok, "bad or overlapping entry in quotes.h");
    printf("  %d quotes\n", QUOTE_COUNT);
  }

  // 9. Quote-length text lays out on the portrait panel without truncation,
  //    from one line up to ~60 words.
  {
    LayoutOptions o;
    o.footer = toPanelText("Pope Leo XIV \xC2\xB7 vatican.va");
    const char* samples[] = {
      "Short and plain.",
      "A sentence of about twenty words, which is the kind of length most of the chosen passages come to, give or take.",
      "A longer passage of two sentences, the sort the list pairs together when the second completes the first and neither "
      "stands well alone. It runs to roughly sixty words, which is about the longest entry, and should still be drawn at "
      "a comfortable size with the date, reference and footer all on the panel.",
    };
    for (const char* t : samples) {
      Verse v; v.text = toPanelText(t); v.reference = toPanelText("Magnifica Humanitas, 94");
      Layout L = layoutVerse(v, "Wednesday, September 30", o);
      CHECK(!L.truncated, "truncated");
      for (auto& d : L.texts) {
        const int w = textWidth(d.font, d.text);
        CHECK(d.x >= 0 && d.x + w <= o.width && d.baseline + fontDescent(d.font) <= o.height, "off panel");
      }
      printf("  %3zu chars -> step %d, %d lines\n", strlen(t), L.fontStep, L.lines);
    }
  }

  printf(fails ? "%d FAILED\n" : "all passed\n", fails);
  return fails ? 1 : 0;
}
