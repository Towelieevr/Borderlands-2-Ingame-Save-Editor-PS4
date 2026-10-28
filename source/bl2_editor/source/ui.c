#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "bl2.h"
#include "ui.h"
#include "style.h"

#define PE_SLOT        (0x200 / 8)
#define PC_MYHUD       0x8C0
#define HUD_CANVAS     0x270
#define CV_FONT        0x60
#define CV_CLIPX       0x70
#define CV_CLIPY       0x74
#define CV_DEFAULT_TEX 0xC0

typedef void (*ProcessEvent_t)(void* self, void* fn, void* parms);

static ProcessEvent_t g_orig_pe;
static void** g_hud_vtable;
static uintptr_t g_fn_postrender, g_fn_setpos, g_fn_setcolor, g_fn_drawtext, g_fn_drawtile, g_fn_textsize;
static uintptr_t g_font;
static volatile uint32_t g_postrender_seen;
static UiDraw g_draw_cb;

typedef struct { uint16_t* data; int32_t count, max; } FString;
static uint16_t g_wbuf[256];

static FString fstr(const char* s) {
    int n = 0;
    while (s[n] && n < 255) { g_wbuf[n] = (uint8_t)s[n]; n++; }
    g_wbuf[n] = 0;
    FString f = {g_wbuf, n + 1, n + 1};
    return f;
}

static void pe(uintptr_t obj, uintptr_t fn, void* parms) {
    ProcessEvent_t f = (ProcessEvent_t)(*(void***)obj)[PE_SLOT];
    f((void*)obj, (void*)fn, parms);
}

static uintptr_t g_cv;
void ui_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (style_text_on()) { Rgb t = style_text(); r = (uint8_t)(t.r * 255); g = (uint8_t)(t.g * 255); b = (uint8_t)(t.b * 255); }
    uint8_t p[4] = {r, g, b, a}; pe(g_cv, g_fn_setcolor, p);
}
static void setpos(float x, float y) {
    struct { float x, y, z; } p = {x, y, 0}; pe(g_cv, g_fn_setpos, &p);
}
void ui_rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    uintptr_t tex = *(uintptr_t*)(g_cv + CV_DEFAULT_TEX);
    if (!tex) return;
    setpos(x, y);
    struct __attribute__((packed)) {
        uintptr_t tex; float xl, yl, u, v, ul, vl; float lc[4]; uint32_t clip; uint8_t blend, plane; uint8_t pad[2];
    } p;
    memset(&p, 0, sizeof(p));
    p.tex = tex; p.xl = w; p.yl = h; p.u = 0; p.v = 0; p.ul = 1; p.vl = 1;
    p.lc[0] = r; p.lc[1] = g; p.lc[2] = b; p.lc[3] = a;
    p.blend = 2;
    pe(g_cv, g_fn_drawtile, &p);
}
void ui_text(float x, float y, const char* s, float scale) {
    setpos(x, y);
    struct __attribute__((packed)) {
        FString text; uint32_t cr; float xs, ys; uint8_t render_info[40]; uint8_t plane; uint8_t pad[3];
    } p;
    memset(&p, 0, sizeof(p));
    p.text = fstr(s); p.cr = 0; p.xs = scale * style_scale(); p.ys = scale * style_scale();
    p.render_info[0] = 0x02;
    pe(g_cv, g_fn_drawtext, &p);
}
float ui_text_width(const char* s, float scale) {
    struct __attribute__((packed)) { FString str; float xl, yl; } p;
    memset(&p, 0, sizeof(p));
    p.str = fstr(s);
    pe(g_cv, g_fn_textsize, &p);
    return p.xl * scale * style_scale();
}
float ui_screen_w(void) { return *(float*)(g_cv + CV_CLIPX); }
float ui_screen_h(void) { return *(float*)(g_cv + CV_CLIPY); }

static int32_t g_canvas_param = 0;
static void __attribute__((force_align_arg_pointer)) vpc_pe_hook(void* self, void* fn, void* parms) {
    g_orig_pe(self, fn, parms);
    if ((uintptr_t)fn != g_fn_postrender || !parms || !g_draw_cb) return;
    g_postrender_seen++;
    uintptr_t cv = *(uintptr_t*)((uintptr_t)parms + g_canvas_param);
    if (!cv) return;
    g_cv = cv;
    uintptr_t old_font = *(uintptr_t*)(cv + CV_FONT);
    uintptr_t font = style_font() ? style_font() : g_font;
    if (font) *(uintptr_t*)(cv + CV_FONT) = font;
    g_draw_cb();
    *(uintptr_t*)(cv + CV_FONT) = old_font;
}

int ui_install(UiDraw cb, void (*patch_ptr)(uint64_t at, uint64_t value)) {
    g_draw_cb = cb;
    if (g_hud_vtable) return 0;
    uintptr_t vpc = bl2_find_instance("WillowGameViewportClient");
    if (!vpc) return -2;
    g_fn_postrender = bl2_find_function("WillowGameViewportClient", "PostRender");
    g_canvas_param  = bl2_param_offset(g_fn_postrender, "Canvas");
    if (g_canvas_param < 0) return -4;
    g_fn_setpos     = bl2_find_function("Canvas", "SetPos");
    g_fn_setcolor   = bl2_find_function("Canvas", "SetDrawColor");
    g_fn_drawtext   = bl2_find_function("Canvas", "DrawText");
    g_fn_drawtile   = bl2_find_function("Canvas", "DrawTile");
    g_fn_textsize   = bl2_find_function("Canvas", "TextSize");
    g_font          = bl2_find_object("Font", "Font_Willowbody_18pt");
    if (!g_font) g_font = bl2_find_object("Font", "MediumFont");
    if (!g_fn_postrender || !g_fn_setpos || !g_fn_setcolor || !g_fn_drawtext || !g_fn_drawtile || !g_fn_textsize) return -3;
    void** vt = *(void***)vpc;
    g_orig_pe = (ProcessEvent_t)vt[PE_SLOT];
    patch_ptr((uint64_t)&vt[PE_SLOT], (uint64_t)&vpc_pe_hook);
    g_hud_vtable = vt;
    return 0;
}

void ui_uninstall(void (*restore)(void)) { if (g_hud_vtable && restore) restore(); g_hud_vtable = NULL; }
