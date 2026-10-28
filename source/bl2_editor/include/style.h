#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct { float r, g, b; } Rgb;

enum { STYLE_PANEL, STYLE_ACCENT, STYLE_TEXT, STYLE_OPACITY, STYLE_FONT, STYLE_SIZE, STYLE_RESET, STYLE_ROWS };

typedef int (*StyleFontScan)(uintptr_t* objs, int max);
typedef const char* (*StyleFontName)(uintptr_t obj);

void        style_init(const char* path, StyleFontScan scan, StyleFontName name);
void        style_request_fonts(void);
void        style_tick(void);
void        style_change(int row, int dir);
const char* style_label(int row);
void        style_value(int row, char* out, int n);

Rgb       style_panel(float t);
Rgb       style_border(void);
Rgb       style_select(void);
Rgb       style_tab(void);
Rgb       style_text(void);
bool      style_text_on(void);
float     style_alpha(void);
float     style_scale(void);
uintptr_t style_font(void);
