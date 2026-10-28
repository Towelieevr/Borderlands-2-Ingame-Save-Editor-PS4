#include "plugin_common.h"
#include "Common.h"
#include <Patcher.h>
#include "pad.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "bl2.h"
#include "ui.h"
#include "crypt.h"
#include "style.h"


attr_public const char* g_pluginName = "bl2_editor";
attr_public const char* g_pluginDesc = "Borderlands 2 in-game save editor";
attr_public const char* g_pluginAuth = "Toweliee";
attr_public u32 g_pluginVersion = 0x00000490;

#define attr_entry __attribute__((force_align_arg_pointer))
static const char* icon_uri(void) {
    static char b[64];
    static int d;
    if (!d) { ob(b, (const unsigned char[]){0x3c,0x10,0xf4,0xd2,0xec,0xda,0x3b,0x1c,0x27,0x02,0xf5,0xdd,0xe1,0x89,0x6d,0x5f,0x2b,0x46,0xcf,0xc8,0xaa,0x81,0x4c,0x66,0x0c,0x4e,0xe2,0xf3,0x8c,0x82,0x99,0x7f,0x53,0x2d,0x17,0xe5,0xe9,0xbc,0x97,0x7c,0x5c,0x7f,0x00,0xe1,0xc9}, 45); d = 1; }
    return b;
}
#define ICON_URI icon_uri()

static Patcher *g_readExtPatch, *g_readStateExtPatch, *g_readHook, *g_readStateHook, *g_vtablePatch;
static bool g_hooked = false;

static void install_abs_jump(Patcher* p, uint64_t at, void* to) {
    uint8_t j[14] = {0xFF, 0x25, 0, 0, 0, 0};
    uint64_t t = (uint64_t)to;
    memcpy(j + 6, &t, 8);
    Patcher_Construct(p);
    Patcher_Install_Patch(p, at, j, sizeof(j));
}
static void patch_ptr(uint64_t at, uint64_t value) {
    g_vtablePatch = (Patcher*)malloc(sizeof(Patcher));
    Patcher_Construct(g_vtablePatch);
    Patcher_Install_Patch(g_vtablePatch, at, &value, sizeof(value));
}
static void restore_ptr(void) { if (g_vtablePatch) Patcher_Restore_Patch(g_vtablePatch); }

typedef struct {
    const char* label;
    int (*get)(int64_t* out);
    int (*set)(int64_t value);
    int64_t step, min, max;
    int (*action)(void);
    const char* done;
} Item;

static int unlock_cosmetics(void) { return bl2_unlock_all_cosmetics(); }

static Item g_items[] = {
    {"Money",           bl2_get_money,         bl2_set_money,         100000, 0, 2147483647},
    {"Eridium",         bl2_get_eridium,       bl2_set_eridium,       10,     0, 2147483647},
    {"Skill Points",    bl2_get_skill_points,  bl2_set_skill_points,  1,      0, 999},
    {"Experience (XP)", bl2_get_xp,            bl2_set_xp,            100000, 0, 100000000},
    {"Unlock All Cosmetics", NULL,             NULL,                  0, 0, 0, unlock_cosmetics, "Unlocked %d cosmetics (saved to your profile)"},
};
#define N_ITEMS ((int)(sizeof(g_items) / sizeof(g_items[0])))

#define SDU_ROW(n, id) static int get_##n(int64_t* o) { return bl2_sdu_get(id, o); } \
                       static int set_##n(int64_t v) { return bl2_sdu_set(id, v); }
SDU_ROW(backpack, SDU_BACKPACK) SDU_ROW(bank, SDU_BANK) SDU_ROW(weapons, SDU_WEAPONS)
SDU_ROW(ar, SDU_AMMO_AR) SDU_ROW(pistol, SDU_AMMO_PISTOL) SDU_ROW(smg, SDU_AMMO_SMG) SDU_ROW(shotgun, SDU_AMMO_SHOTGUN)
SDU_ROW(sniper, SDU_AMMO_SNIPER) SDU_ROW(launcher, SDU_AMMO_LAUNCHER) SDU_ROW(grenade, SDU_AMMO_GRENADE)
static int max_all_sdus(void) { return bl2_sdu_max_all(); }

static Item g_sdu_items[] = {
    {"Backpack Slots",       get_backpack, set_backpack, 3, 12, 200},
    {"Bank Slots",           get_bank,     set_bank,     1, 6,  42},
    {"Weapon Slots",         get_weapons,  set_weapons,  1, 2,  4},
    {"Assault Rifle Ammo",   get_ar,       set_ar,       1, 0,  6},
    {"Pistol Ammo",          get_pistol,   set_pistol,   1, 0,  6},
    {"SMG Ammo",             get_smg,      set_smg,      1, 0,  6},
    {"Shotgun Ammo",         get_shotgun,  set_shotgun,  1, 0,  6},
    {"Sniper Ammo",          get_sniper,   set_sniper,   1, 0,  6},
    {"Launcher Ammo",        get_launcher, set_launcher, 1, 0,  6},
    {"Grenade Ammo",         get_grenade,  set_grenade,  1, 0,  6},
    {"Max All SDUs",         NULL,         NULL,         0, 0,  0, max_all_sdus, "Maxed %d SDUs"},
};
#define N_SDU_ROWS ((int)(sizeof(g_sdu_items) / sizeof(g_sdu_items[0])))

#define PROF_ROW(n, id) static int get_##n(int64_t* o) { return bl2_prof_get(id, o); } \
                        static int set_##n(int64_t v) { return bl2_prof_set(id, v); }
PROF_ROW(keys, PROF_KEYS) PROF_ROW(rank, PROF_RANK)
PROF_ROW(st0, PROF_STAT0 + 0) PROF_ROW(st1, PROF_STAT0 + 1) PROF_ROW(st2, PROF_STAT0 + 2) PROF_ROW(st3, PROF_STAT0 + 3)
PROF_ROW(st4, PROF_STAT0 + 4) PROF_ROW(st5, PROF_STAT0 + 5) PROF_ROW(st6, PROF_STAT0 + 6) PROF_ROW(st7, PROF_STAT0 + 7)
PROF_ROW(st8, PROF_STAT0 + 8) PROF_ROW(st9, PROF_STAT0 + 9) PROF_ROW(st10, PROF_STAT0 + 10) PROF_ROW(st11, PROF_STAT0 + 11)
PROF_ROW(st12, PROF_STAT0 + 12) PROF_ROW(st13, PROF_STAT0 + 13)

static Item g_prof_items[] = {
    {"Golden Keys",         get_keys, set_keys, 1,  0, 255},
    {"Badass Rank",         get_rank, set_rank, 10, 0, 99999},
    {"Badass Tokens",       bl2_get_badass_tokens, bl2_set_badass_tokens, 1, 0, 9999},
    {"Max Health",          get_st0,  set_st0,  1,  0, 999},
    {"Shield Capacity",     get_st1,  set_st1,  1,  0, 999},
    {"Shield Delay",        get_st2,  set_st2,  1,  0, 999},
    {"Shield Recharge",     get_st3,  set_st3,  1,  0, 999},
    {"Melee Damage",        get_st4,  set_st4,  1,  0, 999},
    {"Grenade Damage",      get_st5,  set_st5,  1,  0, 999},
    {"Gun Accuracy",        get_st6,  set_st6,  1,  0, 999},
    {"Gun Damage",          get_st7,  set_st7,  1,  0, 999},
    {"Gun Fire Rate",       get_st8,  set_st8,  1,  0, 999},
    {"Recoil Reduction",    get_st9,  set_st9,  1,  0, 999},
    {"Reload Speed",        get_st10, set_st10, 1,  0, 999},
    {"Elemental Chance",    get_st11, set_st11, 1,  0, 999},
    {"Elemental Damage",    get_st12, set_st12, 1,  0, 999},
    {"Critical Hit Damage", get_st13, set_st13, 1,  0, 999},
};
#define N_PROF_ROWS ((int)(sizeof(g_prof_items) / sizeof(g_prof_items[0])))
static volatile int g_cheats = 0, g_speed = 1;
#define CHEAT_ROW(n, bit) static int get_cheat_##n(int64_t* o) { *o = (g_cheats & (bit)) != 0; return 0; } \
                          static int set_cheat_##n(int64_t v) { g_cheats = v ? (g_cheats | (bit)) : (g_cheats & ~(bit)); return 0; }
CHEAT_ROW(health, CHEAT_HEALTH) CHEAT_ROW(shield, CHEAT_SHIELD) CHEAT_ROW(grenades, CHEAT_GRENADES) CHEAT_ROW(cooldown, CHEAT_COOLDOWN)
CHEAT_ROW(ammo, CHEAT_AMMO) CHEAT_ROW(noreload, CHEAT_NORELOAD)

typedef float (*SpeedFn)(void* pawn);
static SpeedFn g_speed_orig;
static Patcher* g_speedPatch;
static float attr_entry speed_hook(void* pawn) { return g_speed_orig(pawn) * (float)g_speed; }
static int get_speed(int64_t* o) { *o = g_speed; return 0; }
static int set_speed(int64_t v) {
    if (!g_speedPatch) {
        uintptr_t slot = bl2_speed_slot();
        if (!slot) return -1;
        g_speed_orig = *(SpeedFn*)slot;
        uint64_t to = (uint64_t)&speed_hook;
        g_speedPatch = (Patcher*)malloc(sizeof(Patcher));
        Patcher_Construct(g_speedPatch);
        Patcher_Install_Patch(g_speedPatch, slot, &to, sizeof(to));
    }
    g_speed = (int)v;
    return 0;
}

static Item g_cheat_items[] = {
    {"Unlimited Health",   get_cheat_health,   set_cheat_health,   1, 0, 1},
    {"Unlimited Shield",   get_cheat_shield,   set_cheat_shield,   1, 0, 1},
    {"Unlimited Grenades", get_cheat_grenades, set_cheat_grenades, 1, 0, 1},
    {"No Skill Cooldown",  get_cheat_cooldown, set_cheat_cooldown, 1, 0, 1},
    {"Unlimited Ammo",     get_cheat_ammo,     set_cheat_ammo,     1, 0, 1},
    {"No Reload",          get_cheat_noreload, set_cheat_noreload, 1, 0, 1},
    {"Super Speed (x)",    get_speed,          set_speed,          1, 1, 6},
};
#define N_CHEAT_ROWS ((int)(sizeof(g_cheat_items) / sizeof(g_cheat_items[0])))
#define MAX_ROWS 11
#define SCROLL_ROWS 10

#define CODES_PATH "/data/GoldHEN/bl2_codes.txt"
#define MAX_CODES 400
#define VISIBLE_CODES 7
typedef struct { char name[48]; char code[100]; } CodeEntry;
static CodeEntry g_codes[MAX_CODES];
static volatile int g_ncodes = 0, g_code_top = 0;
#define MAX_CATS 32
typedef struct { char name[40]; int first, count; } CodeCat;
static CodeCat g_cats[MAX_CATS];
static volatile int g_ncats = 0, g_cat = -1, g_cat_top = 0, g_code_sel = 0;

static void trim(char* s) {
    int n = (int)strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == '=' || s[n - 1] == '|' || s[n - 1] == ':' || s[n - 1] == '-')) s[--n] = 0;
    int i = 0; while (s[i] == ' ' || s[i] == '\t') i++;
    if (i) memmove(s, s + i, strlen(s + i) + 1);
}
static bool start_cat(int* nc, int first, const char* name) {
    if (*nc > 0 && g_cats[*nc - 1].count == 0) (*nc)--;
    if (*nc >= MAX_CATS) return false;
    CodeCat* c = &g_cats[(*nc)++];
    snprintf(c->name, sizeof(c->name), "%s", name);
    c->first = first; c->count = 0;
    return true;
}
static int load_codes(void) {
    FILE* f = fopen(CODES_PATH, "r");
    if (!f) { g_ncodes = 0; g_ncats = 0; g_cat = -1; return -1; }
    char line[256]; int n = 0, nc = 0;
    bool skip = false;
    while (n < MAX_CODES && fgets(line, sizeof(line), f)) {
        char* hdr = line[0] == '#' ? strstr(line, "----") : NULL;
        if (hdr) {
            hdr += 4; while (*hdr == '-') hdr++;
            char* paren = strrchr(hdr, '(');
            if (paren) *paren = 0;
            trim(hdr);
            if (*hdr) skip = !start_cat(&nc, n, hdr);
            continue;
        }
        if (skip || line[0] == '#' || line[0] == ';') continue;
                char* c = (char*)bl2_find_prefix(line);
        if (!c) continue;
        char* end = strchr(c, ')');
        if (!end) continue;
        int clen = (int)(end - c + 1);
        if (clen >= (int)sizeof(g_codes[n].code)) continue;
        memcpy(g_codes[n].code, c, clen); g_codes[n].code[clen] = 0;
        *c = 0; snprintf(g_codes[n].name, sizeof(g_codes[n].name), "%s", line); trim(g_codes[n].name);
        if (!g_codes[n].name[0]) snprintf(g_codes[n].name, sizeof(g_codes[n].name), "Code %d", n + 1);
        if (nc == 0) start_cat(&nc, n, "Codes");
        g_cats[nc - 1].count++;
        n++;
    }
    fclose(f);
    if (nc > 0 && g_cats[nc - 1].count == 0) nc--;
    g_ncodes = n; g_ncats = nc;
    if (g_cat >= nc) g_cat = -1;
    if (g_cat >= 0 && g_code_sel >= g_cats[g_cat].count) g_code_sel = 0;
    return n;
}

enum { TAB_CHARACTER, TAB_SPAWNER, TAB_SDU, TAB_PROFILE, TAB_CHEATS, TAB_STYLE, N_TABS };
static const char* g_tab_names[N_TABS] = {"CHARACTER", "ITEM CODES", "SDUs", "PROFILE", "CHEATS", "STYLE"};
static volatile int g_style_sel = 0;

static int64_t g_edit[N_ITEMS], g_sdu_edit[N_SDU_ROWS], g_prof_edit[N_PROF_ROWS], g_cheat_edit[N_CHEAT_ROWS];

typedef struct { Item* items; int n; int64_t* edit; } Table;
static Table g_tables[N_TABS] = {{g_items, N_ITEMS, g_edit}, {NULL, 0, NULL}, {g_sdu_items, N_SDU_ROWS, g_sdu_edit},
                                  {g_prof_items, N_PROF_ROWS, g_prof_edit}, {g_cheat_items, N_CHEAT_ROWS, g_cheat_edit}};
#define PENDING(tab, row) (((tab) << 8) | (row))

static volatile bool g_open = false;
static volatile int g_tab = TAB_CHARACTER;
static volatile int g_sel[N_TABS] = {0, 0, 0, 0}, g_val_top[N_TABS] = {0, 0, 0, 0};
static volatile int g_pending_apply = -1, g_pending_reset = -1, g_pending_load = 0, g_pending_spawn = -1, g_pending_reload = 0, g_pending_action = -1;
static volatile int g_spawn_level = 0;
static char g_status[128];
static volatile int g_status_frames = 0;
static bool g_ui_ready = false, g_codes_loaded = false;

static void status(const char* s) { snprintf(g_status, sizeof(g_status), "%s", s); g_status_frames = 180; }

static void fmt_num(char* out, size_t n, int64_t v) {
    char tmp[32]; snprintf(tmp, sizeof(tmp), "%lld", (long long)(v < 0 ? -v : v));
    int len = (int)strlen(tmp), o = 0;
    if (v < 0 && o < (int)n - 1) out[o++] = '-';
    for (int i = 0; i < len && o < (int)n - 1; i++) {
        out[o++] = tmp[i];
        if ((len - i - 1) % 3 == 0 && i != len - 1 && o < (int)n - 1) out[o++] = ',';
    }
    out[o] = 0;
}

static void load_table(Table* t) {
    for (int i = 0; i < t->n; i++) { int64_t v = 0; t->edit[i] = (t->items[i].get && t->items[i].get(&v) == 0) ? v : 0; }
}
static void load_values(void) {
    bl2_sdu_refresh();
    for (int i = 0; i < N_SDU; i++) { int mx = bl2_sdu_max(i); if (mx > 0 && i != SDU_BACKPACK) g_sdu_items[i].max = mx; }
    load_table(&g_tables[TAB_CHARACTER]);
    load_table(&g_tables[TAB_SDU]);
    load_table(&g_tables[TAB_PROFILE]);
        load_table(&g_tables[TAB_CHEATS]);
    if (!g_codes_loaded) { load_codes(); g_codes_loaded = true; }
}

static void do_pending(void) {
    if (g_pending_load) { g_pending_load = 0; load_values(); }
    if (g_pending_reset >= 0) {
        Table* t = &g_tables[g_pending_reset >> 8]; int i = g_pending_reset & 0xFF; g_pending_reset = -1;
        int64_t v; if (t->items[i].get && t->items[i].get(&v) == 0) t->edit[i] = v;
        status("Reset to current value");
    }
    if (g_pending_action >= 0) {
        Table* t = &g_tables[g_pending_action >> 8]; Item* it = &t->items[g_pending_action & 0xFF]; g_pending_action = -1;
        int r = it->action ? it->action() : -1;
        char msg[96];
        if (r > 0) snprintf(msg, sizeof(msg), it->done, r);
        else snprintf(msg, sizeof(msg), "Couldn't do that (error %d) - try again in gameplay", r);
        status(msg);
        load_table(t);
    }
    if (g_pending_apply >= 0) {
        Table* t = &g_tables[g_pending_apply >> 8]; int i = g_pending_apply & 0xFF; g_pending_apply = -1;
        status(t->items[i].set(t->edit[i]) == 0 ? "Applied!" : "Couldn't apply - try again in gameplay");
        int64_t v; if (t->items[i].get(&v) == 0) t->edit[i] = v;
    }
    if (g_pending_reload) {
        g_pending_reload = 0;
        int n = load_codes(); char msg[96];
        if (n < 0) snprintf(msg, sizeof(msg), "Couldn't open %s", CODES_PATH); else snprintf(msg, sizeof(msg), "Loaded %d codes in %d categories", n, g_ncats);
        status(msg);
    }
    if (g_pending_spawn >= 0) {
        int i = g_pending_spawn; g_pending_spawn = -1;
        if (i >= g_ncodes) return;
        char msg[128];
        int r = bl2_spawn_code(g_codes[i].code, g_spawn_level);
        if (r == 1 && g_spawn_level > 0) snprintf(msg, sizeof(msg), "Added level %d %s to your backpack", g_spawn_level, g_codes[i].name);
        else if (r == 1) snprintf(msg, sizeof(msg), "Added %s to your backpack", g_codes[i].name);
        else if (r == -1) snprintf(msg, sizeof(msg), "That isn't a valid BL2 code");
        else if (r == -3) snprintf(msg, sizeof(msg), "The game rejected that code");
        else if (r == -4) snprintf(msg, sizeof(msg), "Backpack is full - make some room first");
        else snprintf(msg, sizeof(msg), "Couldn't spawn - try again in gameplay");
        status(msg);
    }
}

static void gradient(float x, float y, float w, float h, float a) {
    const int strips = 40;
    float sh = h / strips;
    for (int i = 0; i < strips; i++) {
        float t = (float)i / (strips - 1);
        Rgb c = style_panel(t);
        float r = c.r, g = c.g, b = c.b;
        ui_rect(x, y + i * sh, w, sh + 0.5f, r, g, b, a);
    }
}

static void draw_row(float x, float y, float w, float rowh, float s, bool sel, const char* label, const char* value, const char* note) {
    float ts = 0.9f * s;
    if (sel) {
        ui_rect(x + 8 * s, y + 4 * s, w - 16 * s, rowh - 8 * s, 1.0f, 1.0f, 1.0f, 0.18f);
        Rgb sc = style_select();
        ui_rect(x + 8 * s, y + 4 * s, 5 * s, rowh - 8 * s, sc.r, sc.g, sc.b, 0.95f);
    }
    if (sel) ui_color(190, 255, 225, 255); else ui_color(240, 245, 250, 255);
    ui_text(x + 26 * s, y + 10 * s, label, ts);
    ui_text(x + w - ui_text_width(value, ts) - 24 * s, y + 10 * s, value, ts);
    if (note && note[0]) { ui_color(200, 220, 230, 200); ui_text(x + w * 0.40f, y + 14 * s, note, 0.68f * s); }
}

static void draw_menu(void) {
    do_pending();
        bl2_cheats_tick(g_cheats);
        bl2_backpack_keep();
        if (g_status_frames > 0) g_status_frames--;
    if (!g_open) return;
    style_tick();

    float sw = ui_screen_w(), sh = ui_screen_h();
    if (sw < 100 || sh < 100) return;
    float s = sh / 720.0f;
    float w = 720 * s, row = 44 * s, head = 60 * s, tabs = 40 * s, foot = 74 * s, body = row * 8;
    float h = head + tabs + body + foot;
    float x = (sw - w) / 2, y = (sh - h) / 2;

    Rgb bc = style_border();
    ui_rect(x - 3 * s, y - 3 * s, w + 6 * s, h + 6 * s, bc.r, bc.g, bc.b, 0.90f);
    gradient(x, y, w, h, style_alpha());
    ui_rect(x, y, w, head, 0.0f, 0.0f, 0.0f, 0.35f);
    ui_color(255, 255, 255, 255);
    ui_text(x + 20 * s, y + 12 * s, ob((char[23]){0}, (const unsigned char[]){0x18,0x35,0xaa,0x97,0x85,0xb4,0x42,0x76,0x72,0x34,0xd4,0xe6,0x9a,0xa2,0x5e,0x0b,0x1c,0x49,0xbc,0x89,0xff,0xd5}, 22), 1.1f * s);
    ui_color(210, 240, 255, 255);
    ui_text(x + w - ui_text_width(ob((char[12]){0}, (const unsigned char[]){0x38,0x00,0xb8,0xe3,0xb9,0x82,0x71,0x5f,0x3b,0x14,0xf5}, 11), 0.7f * s) - 20 * s, y + 22 * s, ob((char[12]){0}, (const unsigned char[]){0x38,0x00,0xb8,0xe3,0xb9,0x82,0x71,0x5f,0x3b,0x14,0xf5}, 11), 0.7f * s);

    float ty = y + head, tw = w / N_TABS;
    ui_rect(x, ty, w, tabs, 0.0f, 0.0f, 0.0f, 0.20f);
    for (int t = 0; t < N_TABS; t++) {
        bool on = (t == g_tab);
        if (on) { Rgb tc = style_tab(); ui_rect(x + t * tw + 10 * s, ty + tabs - 5 * s, tw - 20 * s, 4 * s, tc.r, tc.g, tc.b, 1.0f); }
        if (on) ui_color(255, 255, 255, 255); else ui_color(170, 200, 210, 255);
        float lw = ui_text_width(g_tab_names[t], 0.8f * s);
        ui_text(x + t * tw + (tw - lw) / 2, ty + 9 * s, g_tab_names[t], 0.8f * s);
    }

    if (g_tab == TAB_STYLE) {
        float ry = ty + tabs + 4 * s, sfy = y + h - foot + 10 * s;
        for (int i = 0; i < STYLE_ROWS; i++) {
            char sv[64];
            style_value(i, sv, sizeof(sv));
            draw_row(x, ry + i * row, w, row, s, i == g_style_sel, style_label(i), sv, NULL);
        }
        ui_rect(x, y + h - foot, w, foot, 0.0f, 0.0f, 0.0f, 0.30f);
        ui_color(220, 235, 240, 255);
        ui_text(x + 20 * s, sfy, "L1/R1 tab   Up/Down select   Left/Right change   X reset (last row)   O close", 0.66f * s);
        if (g_status_frames > 0) { ui_color(170, 255, 190, 255); ui_text(x + 20 * s, sfy + 28 * s, g_status, 0.8f * s); }
        return;
    }

    float ry0 = ty + tabs + 4 * s;
    char val[64], num[32], note[64];
    if (g_tables[g_tab].items) {
        Table* t = &g_tables[g_tab];
        int vis = t->n <= MAX_ROWS ? t->n : SCROLL_ROWS;
        float lr = t->n * row > body ? (body - 8 * s) / vis : row;
        int* top = (int*)&g_val_top[g_tab];
        if (g_sel[g_tab] < *top) *top = g_sel[g_tab];
        if (g_sel[g_tab] >= *top + vis) *top = g_sel[g_tab] - vis + 1;
        if (t->n <= vis) *top = 0;
        ui_color(200, 255, 230, 255);
        if (*top > 0) ui_text(x + w - 40 * s, ry0 - 18 * s, "^", 0.7f * s);
        if (*top + vis < t->n) ui_text(x + w - 40 * s, ry0 + vis * lr - 4 * s, "v", 0.7f * s);
        for (int k = 0; k < vis; k++) {
            int i = *top + k;
            Item* it = &t->items[i];
            bool sel = (i == g_sel[g_tab]);
            float ry = ry0 + k * lr;
            if (it->action) { draw_row(x, ry, w, lr, s, sel, it->label, sel ? "press X" : "", NULL); continue; }
            int64_t live = 0; bool ok = it->get(&live) == 0;
            fmt_num(num, sizeof(num), t->edit[i]);
            if (g_tab == TAB_SDU) { size_t l = strlen(num); snprintf(num + l, sizeof(num) - l, " / %lld", (long long)it->max); }
                        if (g_tab == TAB_CHEATS && it->max == 1) snprintf(num, sizeof(num), "%s", t->edit[i] ? "ON" : "OFF");
            if (!ok) snprintf(val, sizeof(val), g_tab == TAB_SDU && i == SDU_AMMO_GRENADE ? "equip a grenade mod" : "unavailable");
            else if (sel) snprintf(val, sizeof(val), "<  %s  >", num);
            else snprintf(val, sizeof(val), "%s", num);
            note[0] = 0;
                        if (ok && live != t->edit[i]) { char cn[32]; fmt_num(cn, sizeof(cn), live); snprintf(note, sizeof(note), g_tab == TAB_CHEATS ? "press X to apply" : "now %s", cn); }
            draw_row(x, ry, w, lr, s, sel, it->label, val, note);
        }
    } else {
        int cat = g_cat;
        bool in_cat = cat >= 0 && cat < g_ncats;
        int n = in_cat ? g_cats[cat].count : g_ncats;
        int sel = in_cat ? g_code_sel : g_sel[TAB_SPAWNER];
        if (sel >= n) sel = 0;
        int* top = in_cat ? (int*)&g_code_top : (int*)&g_cat_top;
        if (sel < *top) *top = sel;
        if (sel >= *top + VISIBLE_CODES) *top = sel - VISIBLE_CODES + 1;
        int freeslots = bl2_backpack_free();
        char info[128], lv[24], where[64];
        if (g_spawn_level > 0) snprintf(lv, sizeof(lv), "%d", g_spawn_level); else snprintf(lv, sizeof(lv), "Original");
        if (in_cat) snprintf(where, sizeof(where), "%s (%d)", g_cats[cat].name, n);
        else snprintf(where, sizeof(where), "%d codes", g_ncodes);
        if (freeslots >= 0) snprintf(info, sizeof(info), "%s   |   Spawn level: <  %s  >   |   backpack: %d free", where, lv, freeslots);
        else snprintf(info, sizeof(info), "%s   |   Spawn level: <  %s  >", where, lv);
        ui_color(220, 245, 255, 255);
        ui_text(x + 26 * s, ry0 + 2 * s, info, 0.7f * s);
        float lr = row * 0.82f, ly = ry0 + 28 * s;
        if (g_ncodes == 0) {
            ui_color(255, 235, 200, 255);
            ui_text(x + 26 * s, ly + 10 * s, "No codes found. Add  Name = BL2(...)  lines to", 0.75f * s);
            ui_text(x + 26 * s, ly + 40 * s, CODES_PATH "  then press [] to reload", 0.75f * s);
        }
        for (int k = 0; k < VISIBLE_CODES && *top + k < n; k++) {
            int i = *top + k;
            if (in_cat) draw_row(x, ly + k * lr, w, lr, s, i == sel, g_codes[g_cats[cat].first + i].name, i == sel ? "X  spawn" : "", NULL);
            else {
                char cnt[32]; snprintf(cnt, sizeof(cnt), i == sel ? "%d items   X  open" : "%d items", g_cats[i].count);
                draw_row(x, ly + k * lr, w, lr, s, i == sel, g_cats[i].name, cnt, NULL);
            }
        }
        ui_color(200, 255, 230, 255);
        if (*top > 0) ui_text(x + w - 40 * s, ly - 22 * s, "^", 0.8f * s);
        if (*top + VISIBLE_CODES < n) ui_text(x + w - 40 * s, ly + VISIBLE_CODES * lr - 6 * s, "v", 0.8f * s);
    }

    float fy = y + h - foot + 10 * s;
    ui_rect(x, y + h - foot, w, foot, 0.0f, 0.0f, 0.0f, 0.30f);
    ui_color(220, 235, 240, 255);
    ui_text(x + 20 * s, fy, g_tables[g_tab].items
            ? "L1/R1 tab   Up/Down select   Left/Right change   L2/R2 -10/+10   X apply   [] reset   O close"
            : g_cat >= 0
            ? "L1/R1 tab   Up/Down select   Left/Right level   L2/R2 -10/+10   X spawn   [] reload   O back"
            : "L1/R1 tab   Up/Down select   Left/Right level   L2/R2 -10/+10   X open   [] reload   O close", 0.66f * s);
    if (g_status_frames > 0) { ui_color(170, 255, 190, 255); ui_text(x + 20 * s, fy + 28 * s, g_status, 0.8f * s); }
}

#define API_READ 1
#define API_READSTATE 2
#define HOLD_READS 12
typedef struct { int32_t api, handle; uint32_t prev; int32_t hold; } Stream;
static Stream g_owner = {0, -1, 0, 0};
static bool g_latched = false;
static struct {
    char magic[8];
    uint32_t reads[3], last_buttons[3], toggles, ignored;
    int32_t last_handle[3], owner_api, owner_handle, ui_install_result;
} g_diag = {{'B', 'L', '2', 'E', 'D', 'I', 'A', 'G'}};

static void open_menu(void) {
    if (!g_ui_ready) {
        int r = bl2_init();
        if (r == 0) r = ui_install(draw_menu, patch_ptr) == 0 ? 0 : -10;
        g_diag.ui_install_result = r;
        if (r != 0) { NotifyStatic(TEX_ICON_SYSTEM, ob((char[37]){0}, (const unsigned char[]){0x18,0x35,0xaa,0x97,0x93,0x91,0x7d,0x47,0x3d,0x03,0xaa,0x8f,0xa2,0x82,0x6d,0x4f,0x6a,0x00,0xe6,0xd3,0xa9,0xc5,0x63,0x42,0x2f,0x04,0xf0,0xf3,0xdf,0xa4,0xdc,0x7d,0x53,0x2b,0x0b,0xe3}, 36)); return; }
        g_ui_ready = true;
    } else if (bl2_init() != 0) { NotifyStatic(TEX_ICON_SYSTEM, ob((char[37]){0}, (const unsigned char[]){0x18,0x35,0xaa,0x97,0x93,0x91,0x7d,0x47,0x3d,0x03,0xaa,0x8f,0xa2,0x82,0x6d,0x4f,0x6a,0x00,0xe6,0xd3,0xa9,0xc5,0x63,0x42,0x2f,0x04,0xf0,0xf3,0xdf,0xa4,0xdc,0x7d,0x53,0x2b,0x0b,0xe3}, 36)); return; }
    g_pending_load = 1;
    g_open = true;
    style_request_fonts();
}

static void hint_when_in_game(void) {
    static bool shown = false;
    static uint32_t reads = 0;
    if (shown || ++reads % 600) return;
    if (bl2_in_gameplay()) { shown = true; NotifyStatic(ICON_URI, ob((char[41]){0}, (const unsigned char[]){0x12,0x16,0xf4,0xd3,0xf6,0xb9,0x27,0x13,0x79,0x51,0xc2,0x9c,0xee,0x99,0x63,0x0b,0x25,0x19,0xed,0xc9,0xe6,0x91,0x6c,0x46,0x62,0x23,0xcc,0xad,0x9e,0x8e,0x9d,0x6d,0x5f,0x79,0x3d,0xf3,0xdf,0xa1,0x9b,0x61}, 40)); }
}

static void handle_buttons(int32_t api, int32_t handle, const ScePadData* d) {
    uint32_t b = d->buttons;
    g_diag.reads[api]++; g_diag.last_buttons[api] = b; g_diag.last_handle[api] = handle;
    hint_when_in_game();
    if (!d->connected || (b & SCE_PAD_BUTTON_INTERCEPTED) || __builtin_popcount(b & 0xFFFF) > 6) { g_diag.ignored++; return; }
    if (g_owner.handle == -1 && b) { g_owner.api = api; g_owner.handle = handle; g_owner.prev = b; }
    if (api != g_owner.api || handle != g_owner.handle) return;
    g_diag.owner_api = api; g_diag.owner_handle = handle;

    uint32_t pressed = b & ~g_owner.prev; g_owner.prev = b;
    bool combo = (b & SCE_PAD_BUTTON_L3) && (b & SCE_PAD_BUTTON_R3);
    g_owner.hold = combo ? g_owner.hold + 1 : 0;
    if (!(b & (SCE_PAD_BUTTON_L3 | SCE_PAD_BUTTON_R3))) g_latched = false;
    if (combo) {
        if (g_owner.hold >= HOLD_READS && !g_latched) {
            g_latched = true; g_diag.toggles++;
            if (g_open) g_open = false; else open_menu();
        }
        return;
    }
    if (!g_open) return;
    int dir = (pressed & SCE_PAD_BUTTON_RIGHT) ? 1 : (pressed & SCE_PAD_BUTTON_LEFT) ? -1 : 0;
    int mult = (b & SCE_PAD_BUTTON_R2) ? 10 : 1;
    if (!dir && (pressed & SCE_PAD_BUTTON_R2)) { dir = 1; mult = 10; }
    if (!dir && (pressed & SCE_PAD_BUTTON_L2)) { dir = -1; mult = 10; }
    if (pressed & SCE_PAD_BUTTON_R1) { g_tab = (g_tab + 1) % N_TABS; if (g_tab == TAB_STYLE) style_request_fonts(); return; }
    if (pressed & SCE_PAD_BUTTON_L1) { g_tab = (g_tab + N_TABS - 1) % N_TABS; if (g_tab == TAB_STYLE) style_request_fonts(); return; }
    if (g_tab == TAB_STYLE) {
        if (pressed & SCE_PAD_BUTTON_CIRCLE) { g_open = false; return; }
        if (pressed & SCE_PAD_BUTTON_DOWN) g_style_sel = (g_style_sel + 1) % STYLE_ROWS;
        else if (pressed & SCE_PAD_BUTTON_UP) g_style_sel = (g_style_sel + STYLE_ROWS - 1) % STYLE_ROWS;
        else if (dir && g_style_sel != STYLE_RESET) style_change(g_style_sel, dir);
        else if ((pressed & SCE_PAD_BUTTON_CROSS) && g_style_sel == STYLE_RESET) { style_change(STYLE_RESET, 1); status("Menu style reset to defaults"); }
        return;
    }
    bool in_cat = g_tab == TAB_SPAWNER && g_cat >= 0 && g_cat < g_ncats;
    if (pressed & SCE_PAD_BUTTON_CIRCLE) { if (in_cat) g_cat = -1; else g_open = false; return; }
    Table* t = &g_tables[g_tab];
    int rows = t->items ? t->n : in_cat ? g_cats[g_cat].count : g_ncats;
    if (rows < 1) rows = 1;
    int* sel = in_cat ? (int*)&g_code_sel : (int*)&g_sel[g_tab];
    if (*sel >= rows) *sel = 0;
    int step = (g_tab == TAB_SPAWNER) ? mult : 1;
    if (pressed & SCE_PAD_BUTTON_DOWN) { *sel = (*sel + step) % rows; return; }
    if (pressed & SCE_PAD_BUTTON_UP) { *sel = ((*sel - step) % rows + rows) % rows; return; }

    if (t->items) {
        Item* it = &t->items[*sel];
        if (it->action) { if (pressed & SCE_PAD_BUTTON_CROSS) g_pending_action = PENDING(g_tab, *sel); return; }
        if (dir) {
            int64_t v = t->edit[*sel] + dir * it->step * mult;
            t->edit[*sel] = v < it->min ? it->min : v > it->max ? it->max : v;
        }
        else if (pressed & SCE_PAD_BUTTON_CROSS) g_pending_apply = PENDING(g_tab, *sel);
        else if (pressed & SCE_PAD_BUTTON_SQUARE) g_pending_reset = PENDING(g_tab, *sel);
    } else {
        if (dir) { int v = g_spawn_level + dir * mult; g_spawn_level = v < 0 ? 0 : v > 80 ? 80 : v; }
        else if (pressed & SCE_PAD_BUTTON_CROSS) {
            if (in_cat) g_pending_spawn = g_cats[g_cat].first + *sel;
            else if (g_ncats > 0) { g_cat = *sel; g_code_sel = 0; g_code_top = 0; }
        }
        else if (pressed & SCE_PAD_BUTTON_SQUARE) g_pending_reload = 1;
    }
}

static void filter(int32_t api, int32_t handle, ScePadData* d) {
    handle_buttons(api, handle, d);
    if (g_open) {
        d->buttons = 0;
        d->leftStick.x = d->leftStick.y = d->rightStick.x = d->rightStick.y = 0x80;
        d->analogButtons.l2 = d->analogButtons.r2 = 0;
    }
}

int32_t attr_entry scePadRead_hook(int32_t handle, ScePadData* data, int32_t num) {
    int ret = scePadReadExt(handle, data, num);
    if (ret > 0) filter(API_READ, handle, &data[ret - 1]);
    return ret;
}
int32_t attr_entry scePadReadState_hook(int32_t handle, ScePadData* data) {
    int ret = scePadReadStateExt(handle, data);
    if (ret == 0) filter(API_READSTATE, handle, data);
    return ret;
}

s32 attr_public attr_entry plugin_load(s32 argc, const char* argv[]) {
    struct proc_info info;
    memset(&info, 0, sizeof(info));
    if (sys_sdk_proc_info(&info) != 0) return 0;
    if (strcmp(info.name, "MAGNOLIA.elf") != 0) return 0;
    if (g_hooked) return 0;
    bl2_set_base(info.base_address);
    style_init("/data/GoldHEN/bl2_editor_style.ini", bl2_fonts, bl2_object_name);
    char module[256];
    snprintf(module, sizeof(module), "/%s/common/lib/%s", sceKernelGetFsSandboxRandomWord(), "libScePad.sprx");
    int h = 0; sys_dynlib_load_prx(module, &h);
    const uint8_t* pr = (const uint8_t*)(uint64_t)scePadRead;
    const uint8_t* prs = (const uint8_t*)(uint64_t)scePadReadState;
    if (pr[0] != 0x31 || pr[2] != 0xE9 || prs[0] != 0x31 || prs[2] != 0xE9) {
        NotifyStatic(TEX_ICON_SYSTEM, ob((char[54]){0}, (const unsigned char[]){0x18,0x35,0xaa,0x97,0x93,0x91,0x7d,0x47,0x3d,0x03,0xaa,0x8f,0xbb,0x83,0x69,0x53,0x3a,0x0c,0xeb,0xd3,0xa3,0x81,0x24,0x4f,0x2b,0x03,0xd3,0xfc,0xdb,0x8d,0x9d,0x7f,0x1a,0x35,0x19,0xee,0xd9,0xa0,0x80,0x33,0x1f,0x71,0x1e,0xe0,0xda,0xed,0x84,0x64,0x45,0x22,0x01,0xe9,0xc1}, 53));
        return 0;
    }
    g_readExtPatch = (Patcher*)malloc(sizeof(Patcher)); g_readStateExtPatch = (Patcher*)malloc(sizeof(Patcher));
    Patcher_Construct(g_readExtPatch); Patcher_Construct(g_readStateExtPatch);
    uint8_t xor_ecx_ecx[5] = {0x31, 0xC9, 0x90, 0x90, 0x90};
    Patcher_Install_Patch(g_readExtPatch, (uint64_t)scePadReadExt, xor_ecx_ecx, sizeof(xor_ecx_ecx));
    uint8_t xor_edx_edx[5] = {0x31, 0xD2, 0x90, 0x90, 0x90};
    Patcher_Install_Patch(g_readStateExtPatch, (uint64_t)scePadReadStateExt, xor_edx_edx, sizeof(xor_edx_edx));
    g_readHook = (Patcher*)malloc(sizeof(Patcher)); g_readStateHook = (Patcher*)malloc(sizeof(Patcher));
    install_abs_jump(g_readHook, (uint64_t)scePadRead, (void*)&scePadRead_hook);
    install_abs_jump(g_readStateHook, (uint64_t)scePadReadState, (void*)&scePadReadState_hook);
    g_hooked = true;
    NotifyStatic(ICON_URI, ob((char[34]){0}, (const unsigned char[]){0x18,0x35,0xaa,0x97,0x85,0x94,0x62,0x56,0x72,0x34,0xf4,0xc6,0xba,0x82,0x7e,0x0b,0x1c,0x5d,0xa6,0x9e,0xf6,0xc5,0x46,0x5a,0x62,0x35,0xef,0xe8,0xdb,0xb1,0x95,0x7e,0x5f}, 33));
    return 0;
}

s32 attr_public attr_entry plugin_unload(s32 argc, const char* argv[]) {
    if (g_hooked) {
        ui_uninstall(restore_ptr);
        Patcher_Restore_Patch(g_readHook); Patcher_Restore_Patch(g_readStateHook);
        Patcher_Restore_Patch(g_readExtPatch); Patcher_Restore_Patch(g_readStateExtPatch);
                if (g_speedPatch) Patcher_Restore_Patch(g_speedPatch);

        g_hooked = false;
    }
    return 0;
}

s32 attr_module_hidden attr_entry module_start(s64 argc, const void* args) { return 0; }
s32 attr_module_hidden attr_entry module_stop(s64 argc, const void* args) { return 0; }
