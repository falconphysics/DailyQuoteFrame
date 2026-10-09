// quote_source.h — find one numbered paragraph in a long HTML page and cut
// chosen sentences out of it.
//
// The page is fed in as it downloads (any chunk size, even a byte at a time),
// so the frame never holds more than the paragraph it is building. Tags are
// dropped, block tags end a paragraph, <script>/<style>/comments are skipped.
// A finished paragraph is kept if its text starts "N. " for a wanted N.
//
// Pure C++ (no Arduino), so the same code runs in the host tests and in
// tests/preview_quotes.cpp.

#pragma once

#include <string>
#include <vector>
#include <map>
#include <cctype>
#include <cstdint>
#include <cstring>
#include "verse_text.h"   // decodeEntities, toPanelText, GLYPH_*

// ---- Paragraph scanner --------------------------------------------------------

class ParagraphScanner {
 public:
  explicit ParagraphScanner(std::vector<int> wanted) : wanted_(std::move(wanted)) {}

  // Feed the next piece of the page. Returns true once every wanted paragraph
  // has been found (the caller can stop downloading).
  bool feed(const char* p, size_t n) {
    for (size_t i = 0; i < n; i++) step(p[i]);
    return done();
  }
  bool finish() { endParagraph(); return done(); }   // call at end of page
  bool done() const { return found_.size() == wanted_.size(); }

  // Panel text of paragraph n, without its number or footnote markers.
  const std::map<int, std::string>& found() const { return found_; }
  int highestSeen() const { return highest_; }

 private:
  enum State { TEXT, TAG, COMMENT } state_ = TEXT;
  std::vector<int> wanted_;
  std::map<int, std::string> found_;
  std::string para_;        // raw text of the paragraph being built
  std::string tag_;         // current tag, lowercased, first 24 chars
  std::string skipUntil_;   // "script"/"style" while inside one
  std::string win_;         // last few bytes, while looking for its end tag
  char last2_[2] = {0, 0};  // for spotting "-->"
  int highest_ = 0;
  static const size_t MAX_PARA = 12 * 1024;

  void step(char c) {
    switch (state_) {
      case TEXT:
        if (!skipUntil_.empty()) {
          // Inside <script>/<style>: the contents aren't HTML ("i<n" is not a
          // tag), so just watch for the literal closing tag.
          win_ += (char)tolower((unsigned char)c);
          const std::string end = "</" + skipUntil_;
          if (win_.size() > end.size()) win_.erase(0, win_.size() - end.size());
          if (win_ == end) { skipUntil_.clear(); win_.clear(); state_ = TAG; tag_ = end.substr(1); }
          return;
        }
        if (c == '<') { state_ = TAG; tag_.clear(); return; }
        if (para_.size() < MAX_PARA) para_ += c;
        return;
      case TAG:
        if (c == '>') { endTag(); state_ = TEXT; return; }
        if (tag_.size() < 24) tag_ += (char)tolower((unsigned char)c);
        if (tag_ == "!--") { state_ = COMMENT; last2_[0] = last2_[1] = 0; }
        return;
      case COMMENT:
        if (c == '>' && last2_[0] == '-' && last2_[1] == '-') { state_ = TEXT; return; }
        last2_[0] = last2_[1];
        last2_[1] = c;
        return;
    }
  }

  void endTag() {
    const bool closing = !tag_.empty() && tag_[0] == '/';
    size_t a = closing ? 1 : 0, b = a;
    while (b < tag_.size() && (isalnum((unsigned char)tag_[b]))) b++;
    const std::string name = tag_.substr(a, b - a);

    if (!closing && (name == "script" || name == "style")) {
      if (tag_.back() != '/') { skipUntil_ = name; win_.clear(); }
      return;
    }
    static const char* blocks[] = {"p", "div", "br", "li", "ul", "ol", "h1", "h2", "h3",
                                   "h4", "h5", "h6", "blockquote", "td", "tr", "table",
                                   "section", "article", "hr", "body", "center"};
    for (const char* bl : blocks) {
      if (name == bl) { endParagraph(); return; }
    }
    // Inline tags (<i>, <a>, <span>, <b>...) simply vanish; the page's own
    // spaces are kept, so words around them stay as written.
  }

  void endParagraph() {
    if (para_.empty()) return;
    std::string raw;
    raw.swap(para_);
    if (done()) return;
    int n = 0;
    std::string text;
    if (!numberedParagraph(raw, n, text)) return;
    if (n > highest_) highest_ = n;
    for (int w : wanted_) {
      if (w == n && !found_.count(n)) found_[n] = text;
    }
  }

 public:
  // "12. Some text [3] more" (raw HTML text, entities undecoded)
  //   -> n = 12, text = "Some text more" in panel bytes.
  static bool numberedParagraph(const std::string& raw, int& n, std::string& text) {
    std::string t = toPanelText(decodeEntities(raw));
    size_t i = 0;
    while (i < t.size() && isdigit((unsigned char)t[i]) && i < 4) i++;
    if (i == 0 || i >= t.size() || t[i] != '.') return false;
    if (i + 1 >= t.size() || t[i + 1] != ' ') return false;
    n = atoi(t.substr(0, i).c_str());
    text = stripFootnotes(t.substr(i + 2));
    return n > 0 && !text.empty();
  }

  // Remove "[12]" footnote markers along with the space before them.
  static std::string stripFootnotes(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
      if (s[i] == '[') {
        size_t j = i + 1;
        while (j < s.size() && isdigit((unsigned char)s[j])) j++;
        if (j > i + 1 && j < s.size() && s[j] == ']') {
          while (!out.empty() && out.back() == ' ') out.pop_back();
          i = j;
          continue;
        }
      }
      if (s[i] == ' ' && !out.empty() && out.back() == ' ') continue;
      if (s[i] == ' ' && out.empty()) continue;
      out += s[i];
    }
    // "[[8]]" leaves "[]"; a marker right before punctuation leaves "word ,".
    std::string noEmpty;
    for (size_t i = 0; i < out.size(); i++) {
      if (out[i] == '[' && i + 1 < out.size() && out[i + 1] == ']') { i++; continue; }
      if (out[i] == ' ' && !noEmpty.empty() && noEmpty.back() == ' ') continue;
      noEmpty += out[i];
    }
    out.swap(noEmpty);
    std::string tidy;
    for (size_t i = 0; i < out.size(); i++) {
      if (out[i] == ' ' && i + 1 < out.size() &&
          (out[i + 1] == ',' || out[i + 1] == '.' || out[i + 1] == ';' || out[i + 1] == ':')) continue;
      tidy += out[i];
    }
    while (!tidy.empty() && tidy.back() == ' ') tidy.pop_back();
    return tidy;
  }
};

// ---- Sentences ----------------------------------------------------------------

static inline bool isCloser(uint8_t c) {
  return c == GLYPH_RDQUO || c == GLYPH_RSQUO || c == '"' || c == '\'' || c == ')' || c == ']';
}
static inline bool isOpener(uint8_t c) {
  return c == GLYPH_LDQUO || c == GLYPH_LSQUO || c == '"' || c == '\'' || c == '(' || c == '[';
}

// Is the '.' at s[dot] the end of an abbreviation ("cf.", "St.", "e.g.", "J.")?
static inline bool endsAbbreviation(const std::string& s, size_t dot) {
  size_t a = dot;
  while (a > 0 && (isalpha((unsigned char)s[a - 1]) || s[a - 1] == '.')) a--;
  std::string w = s.substr(a, dot - a);
  for (auto& ch : w) ch = (char)tolower((unsigned char)ch);
  if (w.size() == 1 && isupper((unsigned char)s[a])) return true;   // an initial
  {
    // Dotted initials: "J.R.R." -> "j.r.r", every part one letter.
    bool initials = w.find('.') != std::string::npos;
    size_t start = 0;
    while (initials && start <= w.size()) {
      size_t dot = w.find('.', start);
      if (dot == std::string::npos) dot = w.size();
      if (dot - start != 1) initials = false;
      start = dot + 1;
    }
    if (initials && isupper((unsigned char)s[a])) return true;
  }
  // Only abbreviations that really take a full stop. Bible books are left out:
  // this document writes them without one ("cf. Gen 11:4"), and "is." or
  // "ex." would otherwise swallow real sentence ends.
  static const char* abbrevs[] = {"cf", "e.g", "i.e", "st", "sts", "vs", "nos", "nn", "pp",
                                  "ibid", "cit", "eds", "vol", "fr", "dr", "mr", "mrs", "ms"};
  for (const char* ab : abbrevs) if (w == ab) return true;
  return false;
}

// Split panel text into sentences. A sentence ends at . ? or ! (plus any
// closing quotes or brackets) when a space and a capital letter follow,
// optionally behind an opening quote or bracket.
static inline std::vector<std::string> splitSentences(const std::string& s) {
  std::vector<std::string> out;
  size_t start = 0;
  for (size_t i = 0; i < s.size(); i++) {
    const char c = s[i];
    if (c != '.' && c != '?' && c != '!') continue;
    size_t j = i + 1;
    while (j < s.size() && isCloser((uint8_t)s[j])) j++;
    if (j >= s.size() || s[j] != ' ') continue;
    size_t k = j + 1;
    while (k < s.size() && isOpener((uint8_t)s[k])) k++;
    if (k >= s.size() || !isupper((unsigned char)s[k])) continue;
    if (c == '.' && endsAbbreviation(s, i)) continue;
    out.push_back(s.substr(start, j - start));
    start = j + 1;
    i = j;
  }
  if (start < s.size()) out.push_back(s.substr(start));
  return out;
}

// ---- Which passages -----------------------------------------------------------

struct QuoteRef {
  uint16_t paragraph;   // the document's own paragraph number
  uint8_t  first;       // first sentence, counting from 0
  uint8_t  count;       // how many sentences
};

// Sentences first..first+count-1 of an already-extracted paragraph.
static inline bool pickSentences(const std::string& paragraph, const QuoteRef& q,
                                 std::string& out, std::string& why) {
  const std::vector<std::string> ss = splitSentences(paragraph);
  if (q.count == 0 || (size_t)q.first + q.count > ss.size()) {
    why = "paragraph " + std::to_string(q.paragraph) + " has " + std::to_string(ss.size()) +
          " sentences; wanted " + std::to_string(q.first) + "+" + std::to_string(q.count);
    return false;
  }
  out.clear();
  for (int i = 0; i < q.count; i++) {
    if (i) out += ' ';
    out += ss[q.first + i];
  }
  return true;
}

// Days since 1970-01-01 for a calendar date (proleptic Gregorian).
static inline int32_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const uint32_t yoe = (uint32_t)(y - era * 400);
  const uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}
