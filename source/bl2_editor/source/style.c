#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "style.h"

#define MAX_FONTS 32
#define CONFIRM_FRAMES 90

typedef struct { const char* name; float r, g, b; } Swatch;
static const Swatch k_swatch[] = {
    {"Classic", 0.00f, 0.00f, 0.00f},
    {"White",   1.00f, 1.00f, 1.00f}, {"Black",  0.00f, 0.00f, 0.00f}, {"Grey",   0.55f, 0.56f, 0.60f},
    {"Red",     0.85f, 0.12f, 0.15f}, {"Orange", 1.00f, 0.50f, 0.10f}, {"Gold",   1.00f, 0.78f, 0.15f},
    {"Yellow",  1.00f, 0.95f, 0.30f}, {"Lime",   0.55f, 0.95f, 0.20f}, {"Green",  0.15f, 0.75f, 0.30f},
    {"Teal",    0.10f, 0.65f, 0.60f}, {"Cyan",   0.30f, 0.95f, 0.95f}, {"Sky",    0.35f, 0.70f, 1.00f},
    {"Blue",    0.15f, 0.30f, 0.85f}, {"Purple", 0.55f, 0.25f, 0.85f}, {"Pink",   1.00f, 0.40f, 0.75f},
};
#define N_SWATCH ((int)(sizeof(k_swatch) / sizeof(k_swatch[0])))

static const char* k_labels[STYLE_ROWS] = {"Panel colour", "Accent colour", "Text colour", "Panel opacity", "Font", "Text size", "Reset to defaults"};

static char g_path[96];
static StyleFontScan g_scan;
static StyleFontName g_fname;
static volatile int g_panel, g_accent, g_text, g_opacity = 90, g_size = 100, g_font_idx;
static char g_font_name[48];
static uintptr_t g_fonts[MAX_FONTS];
static char g_font_names[MAX_FONTS][48];
static volatile int g_nfonts;
static volatile uintptr_t g_font;
static volatile bool g_want_fonts, g_dirty;
static volatile int g_confirm;

static int wrap(int v, int n) { return ((v % n) + n) % n; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void save(void) {
    FILE* f = fopen(g_path, "w");
    if (!f) return;
    fprintf(f, "panel=%d\naccent=%d\ntext=%d\nopacity=%d\nsize=%d\nfont=%s\n", g_panel, g_accent, g_text, g_opacity, g_size, g_font_name);
    fclose(f);
}

static void load(void) {
    FILE* f = fopen(g_path, "r");
    if (!f) return;
    char line[96];
    while (fgets(line, sizeof(line), f)) {
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char* v = eq + 1;
        size_t n = strlen(v);
        while (n && (v[n - 1] == '\n' || v[n - 1] == '\r' || v[n - 1] == ' ')) v[--n] = 0;
        if (!strcmp(line, "panel")) g_panel = wrap(atoi(v), N_SWATCH);
        else if (!strcmp(line, "accent")) g_accent = wrap(atoi(v), N_SWATCH);
        else if (!strcmp(line, "text")) g_text = wrap(atoi(v), N_SWATCH);
        else if (!strcmp(line, "opacity")) g_opacity = clampi(atoi(v), 40, 100);
        else if (!strcmp(line, "size")) g_size = clampi(atoi(v), 70, 140);
        else if (!strcmp(line, "font")) snprintf(g_font_name, sizeof(g_font_name), "%s", v);
    }
    fclose(f);
}

void style_init(const char* path, StyleFontScan scan, StyleFontName name) {
    snprintf(g_path, sizeof(g_path), "%s", path);
    g_scan = scan;
    g_fname = name;
    load();
    if (g_font_name[0]) g_want_fonts = true;
}

void style_request_fonts(void) { g_want_fonts = true; }

static void scan_fonts(void) {
    uintptr_t objs[MAX_FONTS * 2];
    int n = g_scan ? g_scan(objs, MAX_FONTS * 2) : 0, k = 0;
    for (int i = 0; i < n && k < MAX_FONTS; i++) {
        const char* nm = g_fname(objs[i]);
        if (!nm || !nm[0] || !strncmp(nm, "Default__", 9)) continue;
        bool dup = false;
        for (int j = 0; j < k; j++) if (!strcmp(g_font_names[j], nm)) dup = true;
        if (dup) continue;
        g_fonts[k] = objs[i];
        snprintf(g_font_names[k], sizeof(g_font_names[k]), "%s", nm);
        k++;
    }
    g_nfonts = k;
    g_font_idx = 0;
    g_font = 0;
    for (int i = 0; i < k; i++)
        if (g_font_name[0] && !strcmp(g_font_names[i], g_font_name)) { g_font_idx = i + 1; g_font = g_fonts[i]; }
}

void style_tick(void) {
    if (g_want_fonts) { g_want_fonts = false; scan_fonts(); }
    if (g_confirm > 0) { g_confirm--; return; }
    if (g_dirty) { g_dirty = false; save(); }
}

void style_change(int row, int dir) {
    switch (row) {
    case STYLE_PANEL:   g_panel = wrap(g_panel + dir, N_SWATCH); break;
    case STYLE_ACCENT:  g_accent = wrap(g_accent + dir, N_SWATCH); break;
    case STYLE_TEXT:    g_text = wrap(g_text + dir, N_SWATCH); break;
    case STYLE_OPACITY: g_opacity = clampi(g_opacity + dir * 5, 40, 100); break;
    case STYLE_SIZE:    g_size = clampi(g_size + dir * 5, 70, 140); break;
    case STYLE_FONT: {
        int n = g_nfonts + 1;
        int i = wrap(g_font_idx + dir, n);
        g_font_idx = i;
        g_font = i ? g_fonts[i - 1] : 0;
        snprintf(g_font_name, sizeof(g_font_name), "%s", i ? g_font_names[i - 1] : "");
        g_confirm = CONFIRM_FRAMES;
        if (!g_nfonts) g_want_fonts = true;
        break;
    }
    case STYLE_RESET:
        g_panel = g_accent = g_text = 0;
        g_opacity = 90; g_size = 100;
        g_font_idx = 0; g_font = 0; g_font_name[0] = 0;
        g_confirm = 0;
        break;
    default: return;
    }
    g_dirty = true;
}

const char* style_label(int row) { return row >= 0 && row < STYLE_ROWS ? k_labels[row] : ""; }

void style_value(int row, char* out, int n) {
    switch (row) {
    case STYLE_PANEL:   snprintf(out, n, "<  %s  >", g_panel ? k_swatch[g_panel].name : "Classic (blue/green)"); break;
    case STYLE_ACCENT:  snprintf(out, n, "<  %s  >", g_accent ? k_swatch[g_accent].name : "Classic (cyan)"); break;
    case STYLE_TEXT:    snprintf(out, n, "<  %s  >", g_text ? k_swatch[g_text].name : "Classic (white)"); break;
    case STYLE_OPACITY: snprintf(out, n, "<  %d%%  >", g_opacity); break;
    case STYLE_SIZE:    snprintf(out, n, "<  %d%%  >", g_size); break;
    case STYLE_FONT:    snprintf(out, n, "<  %s  >", g_font_idx ? g_font_names[g_font_idx - 1] : "Game default"); break;
    case STYLE_RESET:   snprintf(out, n, "press X"); break;
    default:            out[0] = 0;
    }
}

static Rgb sw(int i) { Rgb c = {k_swatch[i].r, k_swatch[i].g, k_swatch[i].b}; return c; }
static Rgb rgb(float r, float g, float b) { Rgb c = {r, g, b}; return c; }

Rgb style_panel(float t) {
    if (!g_panel) return rgb(0.05f + 0.02f * t, 0.22f + 0.43f * t, 0.75f - 0.45f * t);
    Rgb c = sw(g_panel);
    float k = 0.55f + 0.45f * t;
    return rgb(c.r * k, c.g * k, c.b * k);
}
Rgb style_border(void) { return g_accent ? sw(g_accent) : rgb(0.30f, 0.95f, 0.95f); }
Rgb style_select(void) { return g_accent ? sw(g_accent) : rgb(0.35f, 1.00f, 0.95f); }
Rgb style_tab(void)    { return g_accent ? sw(g_accent) : rgb(0.40f, 1.00f, 0.60f); }
Rgb style_text(void)   { return g_text ? sw(g_text) : rgb(1.0f, 1.0f, 1.0f); }
bool style_text_on(void) { return g_text != 0; }
float style_alpha(void) { return g_opacity / 100.0f; }
float style_scale(void) { return g_size / 100.0f; }
uintptr_t style_font(void) { return g_font; }
