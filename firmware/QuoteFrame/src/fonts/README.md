# Fonts

GNU FreeSerif (GPLv3 with the font exception, so embedding them in firmware is
fine), converted with Adafruit GFX's `fontconvert`, range 0x20–0x86.

The stock `fontconvert` only reads the character codes it writes, so to get
typographic punctuation into the slots above `~` it was patched to look up a
different Unicode character for those slots:

| Slot | Character |
|---|---|
| 0x7F | ‘ U+2018 |
| 0x80 | ’ U+2019 |
| 0x81 | “ U+201C |
| 0x82 | ” U+201D |
| 0x83 | — U+2014 |
| 0x84 | – U+2013 |
| 0x85 | … U+2026 |
| 0x86 | · U+00B7 |

The patch, in `fontconvert.c` just above `main()`:

```c
static int remap(int c) {
  static const int extra[] = {0x2018, 0x2019, 0x201C, 0x201D, 0x2014, 0x2013, 0x2026, 0x00B7};
  if (c >= 0x7F && c < 0x7F + (int)(sizeof(extra) / sizeof(extra[0]))) return extra[c - 0x7F];
  return c;
}
```

and `FT_Load_Char(face, i, ...)` becomes `FT_Load_Char(face, remap(i), ...)`.

To add a size: `./fontconvert FreeSerif.ttf 30 32 134 > FreeSerif30pt8b.h`,
then include it and add a row to `FONT_LADDER` in `fonts.h`.
`verse_text.h` (`toPanelText`) maps incoming UTF-8 to these slots.
