#pragma once
#include <stdint.h>

typedef void (*UiDraw)(void);

int   ui_install(UiDraw draw, void (*patch_ptr)(uint64_t at, uint64_t value));
void  ui_uninstall(void (*restore)(void));

void  ui_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void  ui_rect(float x, float y, float w, float h, float r, float g, float b, float a);
void  ui_text(float x, float y, const char* s, float scale);
float ui_text_width(const char* s, float scale);
float ui_screen_w(void);
float ui_screen_h(void);
