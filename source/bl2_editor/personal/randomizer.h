#pragma once
#include <stdint.h>

void        rando_boot(void);
void        rando_tick(void);
int         rando_rows(void);
const char* rando_label(int row);
void        rando_value(int row, char* out, int n);
const char* rando_change(int row, int dir);
const char* rando_press(int row);
void        rando_unhook(void);
