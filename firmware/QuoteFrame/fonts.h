// fonts.h — the typefaces the verse frame draws with.
//
// GNU FreeSerif, converted with Adafruit GFX's fontconvert at 141 dpi. The
// range is 0x20-0x86: printable ASCII plus curly quotes, dashes, ellipsis and a
// middle dot in the slots above '~' (see GLYPH_* in verse_text.h and
// src/fonts/README.md for how they were generated).
#pragma once

#include "src/fonts/FreeSerif12pt8b.h"
#include "src/fonts/FreeSerif14pt8b.h"
#include "src/fonts/FreeSerif16pt8b.h"
#include "src/fonts/FreeSerif18pt8b.h"
#include "src/fonts/FreeSerif20pt8b.h"
#include "src/fonts/FreeSerif22pt8b.h"
#include "src/fonts/FreeSerif25pt8b.h"
#include "src/fonts/FreeSerif28pt8b.h"
#include "src/fonts/FreeSerif34pt8b.h"
#include "src/fonts/FreeSerifBoldItalic12pt8b.h"
#include "src/fonts/FreeSerifBoldItalic14pt8b.h"
#include "src/fonts/FreeSerifBoldItalic16pt8b.h"
#include "src/fonts/FreeSerifBoldItalic18pt8b.h"
#include "src/fonts/FreeSerifBoldItalic21pt8b.h"
#include "src/fonts/FreeSerifItalic13pt8b.h"
#include "src/fonts/FreeSerif10pt8b.h"

// Largest first. The layout uses the biggest verse size that fits, paired with
// the reference size on the same row.
struct FontStep { const GFXfont* verse; const GFXfont* reference; };

static const FontStep FONT_LADDER[] = {
  { &FreeSerif34pt8b, &FreeSerifBoldItalic21pt8b },
  { &FreeSerif28pt8b, &FreeSerifBoldItalic18pt8b },
  { &FreeSerif25pt8b, &FreeSerifBoldItalic18pt8b },
  { &FreeSerif22pt8b, &FreeSerifBoldItalic16pt8b },
  { &FreeSerif20pt8b, &FreeSerifBoldItalic16pt8b },
  { &FreeSerif18pt8b, &FreeSerifBoldItalic14pt8b },
  { &FreeSerif16pt8b, &FreeSerifBoldItalic14pt8b },
  { &FreeSerif14pt8b, &FreeSerifBoldItalic12pt8b },
  { &FreeSerif12pt8b, &FreeSerifBoldItalic12pt8b },
};
static const int FONT_LADDER_COUNT = sizeof(FONT_LADDER) / sizeof(FONT_LADDER[0]);

static const GFXfont* const FONT_DATE   = &FreeSerifItalic13pt8b;
static const GFXfont* const FONT_FOOTER = &FreeSerif10pt8b;
