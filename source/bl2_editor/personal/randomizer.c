#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "bl2.h"
#include "randomizer.h"
#include "Detour.h"
#include "pyrand.h"
#include "rando_data.h"

#define UFUNC_FUNC 0xD8
#define FRAME_NODE 0x18
#define FRAME_LOCALS 0x30
#define MAX_STRIPPED 256
#define POOLINFO_SIZE 0x28
#define INI_PATH "/data/GoldHEN/bl2_rando.ini"
#define SEED_VERSION 7
#define TAG_EVOLVED (1ULL << 24)
#define FIXED_ROWS 7

uint64_t sceKernelGetProcessTime(void);

typedef void (*ScriptFn)(void* self, void* stack, void* result);
typedef struct { uintptr_t data; int32_t count, max; } RArray;

static Detour r_detour;
static uintptr_t r_fn_died, r_fn_spawn, r_fn_drop, r_pool, r_last;
static int32_t r_off_bal = -1, r_off_loc = -1, r_off_pools = -1, r_off_tt = -1, r_off_class = -1, r_p_def = -1, r_p_qty = -1,
               r_p_stage = -1, r_p_ctx = -1, r_p_out = -1, r_p_loc = -1, r_p_vel = -1;
static uintptr_t r_fn_complete;
static int32_t r_p_mission = -1, r_off_reward = -1, r_off_ritems = -1, r_off_rpools = -1, r_off_pawn = -1;
typedef struct { uintptr_t def; int32_t items, pools; } Stripped;
static Stripped r_stripped[MAX_STRIPPED];
static int r_nstripped;
static volatile int r_on, r_ready;
static uint32_t r_retry, r_rng = 0x9E3779B9u;

static uint8_t g_seed[9];
static char g_seed_str[18];
static uint64_t g_tags, g_newtags = RANDO_DEFAULT_TAGS;
static int g_have_seed, g_nloc, g_nitem, g_guaranteed;
static int g_every;
static const int k_every[] = {0, 5, 10, 25, 50, 100};
#define N_EVERY ((int)(sizeof(k_every) / sizeof(k_every[0])))
static uint16_t g_loc_item[RANDO_NLOCS];
static uintptr_t g_cache[RANDO_NITEMS];
static char g_msg[96];
#define MAX_EXTRA 800
static uintptr_t g_extra[MAX_EXTRA];
static int g_nextra = -1;

static uint32_t roll(void) {
    r_rng ^= r_rng << 13; r_rng ^= r_rng >> 17; r_rng ^= r_rng << 5;
    return r_rng;
}

static void sync_player(void) {
    uintptr_t pc = bl2_live_pc();
        if (pc && pc != bl2_player_controller()) { bl2_init(); g_nextra = -1; }
}


static int apply_seed(const char* str) {
    uint8_t d[9];
    uint64_t tags;
    int ver;
    if (seed_from_string(str, d)) return -1;
    seed_parts(d, &tags, &ver);
    if (ver != SEED_VERSION) return -2;
    static uint64_t it[RANDO_NPOOLS], lt[RANDO_NLOCS];
    static uint16_t wi[RANDO_NLOCS + RANDO_NPOOLS], wl[RANDO_NLOCS], wt[RANDO_NLOCS + RANDO_NPOOLS], wp[RANDO_NLOCS + RANDO_NPOOLS];
    static uint8_t ws[RANDO_NLOCS + RANDO_NPOOLS];
    for (int i = 0; i < RANDO_NPOOLS; i++) it[i] = k_rando_pools[i].tags;
    for (int i = 0; i < RANDO_NLOCS; i++) lt[i] = k_rando_locs[i].tags;
    int nl, ni;
    if (seed_assign(d, tags, it, RANDO_NPOOLS, lt, RANDO_NLOCS, g_loc_item, wi, wl, wt, wp, ws, &nl, &ni)) return -3;
    memcpy(g_seed, d, 9);
    seed_to_string(d, g_seed_str);
    g_tags = tags; g_nloc = nl; g_nitem = ni; g_have_seed = 1;
    r_rng ^= (uint32_t)(tags * 2654435761u) ^ d[0] ^ ((uint32_t)d[8] << 24);
    if (!r_rng) r_rng = 1;
    return 0;
}

static void save_ini(void) {
    FILE* f = fopen(INI_PATH, "w");
    if (!f) return;
    fprintf(f, "seed=%s\non=%d\nnewtags=%llX\nguaranteed=%d\nevery=%d\n", g_have_seed ? g_seed_str : "", r_on ? 1 : 0,
            (unsigned long long)g_newtags, g_guaranteed, g_every);
    fclose(f);
}

static int load_ini(void) {
    FILE* f = fopen(INI_PATH, "r");
    if (!f) return -1;
    char line[96];
    int rc = 0, on = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' ')) line[--n] = 0;
        if (!strncmp(line, "seed=", 5) && line[5]) rc = apply_seed(line + 5);
        else if (!strncmp(line, "on=", 3)) on = atoi(line + 3);
        else if (!strncmp(line, "newtags=", 8)) g_newtags = strtoull(line + 8, NULL, 16);
        else if (!strncmp(line, "guaranteed=", 11)) g_guaranteed = atoi(line + 11) != 0;
        else if (!strncmp(line, "every=", 6)) { int v = atoi(line + 6); g_every = v < 0 ? 0 : v > 100 ? 100 : v; }
    }
    fclose(f);
    r_on = on && g_have_seed;
    return rc;
}

void rando_boot(void) { load_ini(); }

static int player_char(void) {
    static const char* const k[] = {"Assassin", "Mercenary", "Siren", "Soldier", "Lilac", "Mechromancer"};
    uintptr_t pc = bl2_player_controller();
    uintptr_t cls = (pc && r_off_class >= 0) ? *(uintptr_t*)(pc + r_off_class) : 0;
    const char* nm = cls ? bl2_object_name(cls) : "";
    for (int i = 0; i < 6; i++) if (strstr(nm, k[i])) return i;
    return -1;
}

static void spawn_drop(uintptr_t pawn, uintptr_t bal, int n, int to_bag) {
    uint8_t p[0x40];
    memset(p, 0, sizeof(p));
    *(uintptr_t*)(p + r_p_def) = bal;
    *(int32_t*)(p + r_p_qty) = 1;
    *(int32_t*)(p + r_p_stage) = bl2_player_level();
    *(uintptr_t*)(p + r_p_ctx) = pawn;
    bl2_process_event(r_pool, r_fn_spawn, p);
    RArray* out = (RArray*)(p + r_p_out);
    for (int32_t i = 0; out->data && i < out->count && i < 4; i++) {
        uintptr_t inv = ((uintptr_t*)out->data)[i];
        if (!inv) continue;
        if (to_bag && bl2_add_to_backpack(inv) == 0) continue;
        uint8_t d[0x40];
        memset(d, 0, sizeof(d));
        const float* from = (const float*)(pawn + r_off_loc);
        float* at = (float*)(d + r_p_loc);
        at[0] = from[0] + (float)((n % 3) - 1) * 25.0f; at[1] = from[1] + (float)((n / 3) % 3 - 1) * 25.0f; at[2] = from[2] + 60.0f;
        float* v = (float*)(d + r_p_vel);
        v[0] = (float)((int)(roll() % 201) - 100); v[1] = (float)((int)(roll() % 201) - 100); v[2] = 300.0f;
        bl2_process_event(inv, r_fn_drop, d);
    }
}

static void drop_pool(uintptr_t pawn, int pool, int n, int to_bag) {
    const RandoPool* P = &k_rando_pools[pool];
    int idx[24], cnt = 0;
    for (int i = 0; i < P->count && cnt < 24; i++) if (k_rando_items[P->first + i].content & g_tags) idx[cnt++] = P->first + i;
    if (cnt < P->count && P->fb_count && (k_rando_items[P->fb_first].content & g_tags) == k_rando_items[P->fb_first].content)
        for (int i = 0; i < P->fb_count && cnt < 24; i++) idx[cnt++] = P->fb_first + i;
    int chr = player_char();
    float w[24], total = 0;
    for (int i = 0; i < cnt; i++) {
        const RandoItem* it = &k_rando_items[idx[i]];
        w[i] = it->chr >= 0 ? (chr < 0 || it->chr == chr ? 1.0f : 0.0f) : it->weight;
        total += w[i];
    }
    if (total <= 0) return;
    float pick = (float)(roll() % 1000000) / 1000000.0f * total;
    int sel = cnt - 1;
    for (int i = 0; i < cnt; i++) { if (pick < w[i]) { sel = i; break; } pick -= w[i]; }
    int item = idx[sel];
    if (!g_cache[item]) g_cache[item] = bl2_find_path(k_rando_items[item].path);
    if (g_cache[item]) spawn_drop(pawn, g_cache[item], n, to_bag);
}

static int whitelisted(uintptr_t pool) {
    const char* nm = pool ? bl2_object_name(pool) : "";
    for (int i = 0; i < RANDO_NWHITE; i++) if (!strcmp(nm, k_rando_whitelist[i])) return 1;
    return 0;
}

static void strip_pools(uintptr_t pawn) {
    if (r_off_pools < 0) return;
    RArray* a = (RArray*)(pawn + r_off_pools);
    if (!a->data || a->count <= 0) return;
    int32_t keep = 0;
    for (int32_t i = 0; i < a->count; i++) {
        uint8_t* e = (uint8_t*)a->data + (size_t)i * POOLINFO_SIZE;
        if (!whitelisted(*(uintptr_t*)e)) continue;
        if (keep != i) memmove((uint8_t*)a->data + (size_t)keep * POOLINFO_SIZE, e, POOLINFO_SIZE);
        keep++;
    }
    a->count = keep;
}

static int inject_ok(const RandoPawn* p, int tt) {
    if (p->transform >= 0) return p->transform == tt;
    if (p->evolved >= 0 && p->evolved == tt) return !(g_tags & TAG_EVOLVED);
    return 1;
}

static int has(const char* s, const char* k) { return strstr(s, k) != NULL; }

static int extra_ok(int kind, const char* nm) {
    if (!nm[0] || has(nm, "Default__") || has(nm, "NODROP") || has(nm, "Test") || has(nm, "Enemy")) return 0;
    if (kind == 0) {
        static const char* const k[] = {"_4_VeryRare", "_5_Alien", "Seraph", "_4_Quartz", "_4_Emerald", "_4_Garnet", "_4_Citrine",
                                        "_4_CubicZerconia", "_4_Aquamarine", "_4_Diamond", "_4_Rock"};
        for (int i = 0; i < (int)(sizeof(k) / sizeof(k[0])); i++) if (has(nm, k[i])) return 1;
        return 0;
    }
    if (kind == 2) return has(nm, "_03_Rare") || has(nm, "_04_VeryRare");
    if (has(nm, "Seraph")) return 1;
    if (has(nm, "Shield")) return has(nm, "_03_Rare") || has(nm, "_04_VeryRare");
    if (!strncmp(nm, "GM_", 3)) return has(nm, "_3_Rare") || has(nm, "_4_VeryRare");
    if (!strncmp(nm, "A_", 2)) return has(nm, "_Rare") || has(nm, "_VeryRare");
    return 0;
}

static void scan_extras(void) {
    static const char* const cls[3] = {"WeaponBalanceDefinition", "InventoryBalanceDefinition", "ClassModBalanceDefinition"};
    static uintptr_t all[4096];
    g_nextra = 0;
    for (int c = 0; c < 3; c++) {
        int n = bl2_find_all(cls[c], all, 4096);
        for (int i = 0; i < n && g_nextra < MAX_EXTRA; i++)
            if (extra_ok(c, bl2_object_name(all[i]))) g_extra[g_nextra++] = all[i];
    }
}

static int class_ok(uintptr_t bal) {
    static const char* const k[6] = {"Assassin", "Mercenary", "Siren", "Soldier", "Psycho", "Mechromancer"};
    const char* nm = bl2_object_name(bal);
    if (!has(nm, "ClassMod")) return 1;
    int chr = player_char();
    return chr < 0 || has(nm, k[chr]);
}

static void drop_random(uintptr_t pawn, int n) {
    static uint16_t ok[RANDO_NPOOLS];
    if (g_nextra < 0) scan_extras();
    int np = 0;
    for (int i = 0; i < RANDO_NPOOLS; i++) if (k_rando_pools[i].tags & g_tags) ok[np++] = (uint16_t)i;
    if (np + g_nextra <= 0) return;
    for (int t = 0; t < 8; t++) {
        uint32_t r = roll() % (uint32_t)(np + g_nextra);
        if ((int)r < np) { drop_pool(pawn, ok[r], n, 0); return; }
        if (class_ok(g_extra[r - np])) { spawn_drop(pawn, g_extra[r - np], n, 0); return; }
    }
}

static void on_death(uintptr_t pawn) {
    if (pawn == r_last) return;
    uintptr_t bal = *(uintptr_t*)(pawn + r_off_bal);
    const char* bn = bal ? bl2_object_name(bal) : "";
    int tt = r_off_tt >= 0 ? *(uint8_t*)(pawn + r_off_tt) : 0, found = 0, loc = -1;
    for (int i = 0; bal && i < RANDO_NPAWNS; i++) {
        if (strcasecmp(k_rando_pawns[i].balance, bn)) continue;
        found = 1;
        if (inject_ok(&k_rando_pawns[i], tt)) { loc = k_rando_pawns[i].loc; break; }
    }
    int extra = g_every > 0 && (int)(roll() % 100) < g_every;
    if (!found && !extra) return;
    r_last = pawn;
    sync_player();
    int n = 0;
    if (found) {
        strip_pools(pawn);
        uint16_t it = loc >= 0 ? g_loc_item[loc] : SEED_NONE;
        if (it < RANDO_NPOOLS) {
            const RandoLoc* L = &k_rando_locs[loc];
            for (int r = 0; r < L->nrar; r++)
                if (g_guaranteed || (int)(roll() % 100) < L->rar[r]) drop_pool(pawn, it, n++, 0);
        }
    }
        if (extra) drop_random(pawn, n);

}

static void strip_reward(uintptr_t def) {
    if (r_off_reward < 0 || r_off_ritems < 0 || r_off_rpools < 0) return;
    for (int i = 0; i < r_nstripped; i++) if (r_stripped[i].def == def) return;
    if (r_nstripped >= MAX_STRIPPED) return;
    RArray* items = (RArray*)(def + r_off_reward + r_off_ritems);
    RArray* pools = (RArray*)(def + r_off_reward + r_off_rpools);
    Stripped* s = &r_stripped[r_nstripped++];
    s->def = def; s->items = items->count; s->pools = pools->count;
    items->count = 0;
    pools->count = 0;
}

static void restore_rewards(void) {
    for (int i = 0; i < r_nstripped; i++) {
        Stripped* s = &r_stripped[i];
        ((RArray*)(s->def + r_off_reward + r_off_ritems))->count = s->items;
        ((RArray*)(s->def + r_off_reward + r_off_rpools))->count = s->pools;
    }
    r_nstripped = 0;
}

static void on_complete(uintptr_t pc, uintptr_t def) {
    if (!def) return;
    const char* dn = bl2_object_name(def);
    int loc = -1;
    for (int i = 0; i < RANDO_NMISSIONS; i++) {
        if (strcasecmp(k_rando_missions[i].def, dn)) continue;
        if (loc < 0 || g_loc_item[k_rando_missions[i].loc] < RANDO_NPOOLS) loc = k_rando_missions[i].loc;
        if (g_loc_item[loc] < RANDO_NPOOLS) break;
    }
    if (loc < 0) return;
    sync_player();
    strip_reward(def);
    uint16_t it = g_loc_item[loc];
    uintptr_t pawn = (pc && r_off_pawn >= 0) ? *(uintptr_t*)(pc + r_off_pawn) : 0;
    if (it >= RANDO_NPOOLS || !pawn) return;
    const RandoLoc* L = &k_rando_locs[loc];
    for (int r = 0; r < L->nrar; r++) drop_pool(pawn, it, r, 1);
}

static void __attribute__((force_align_arg_pointer)) interp_hook(void* self, void* stack, void* result) {
    if (r_on && g_have_seed) {
        uintptr_t node = *(uintptr_t*)((uintptr_t)stack + FRAME_NODE);
        if (node == r_fn_died) on_death((uintptr_t)self);
        else if (node && node == r_fn_complete) {
            uintptr_t locals = *(uintptr_t*)((uintptr_t)stack + FRAME_LOCALS);
            if (locals) on_complete((uintptr_t)self, *(uintptr_t*)(locals + r_p_mission));
        }
    }
    Detour_Stub(&r_detour, ScriptFn, self, stack, result);
}

static int rando_init(void) {
    if (r_ready) return 0;
    uintptr_t fn = bl2_find_function("WillowAIPawn", "Died");
    r_fn_spawn = bl2_find_function("ItemPool", "SpawnBalancedInventoryFromInventoryBalanceDefinition");
    r_fn_drop = bl2_find_function("WillowInventory", "DropFrom");
    r_pool = bl2_find_object("ItemPool", "Default__ItemPool");
    r_off_bal = bl2_prop_offset("Pawn", "BalanceDefinitionState");
    r_off_loc = bl2_prop_offset("Actor", "Location");
    r_off_pools = bl2_prop_offset("WillowPawn", "ItemPoolList");
    r_off_tt = bl2_prop_offset("WillowAIPawn", "TransformType");
    r_off_class = bl2_prop_offset("WillowPlayerController", "PlayerClass");
    r_p_def = bl2_param_offset(r_fn_spawn, "InvBalanceDefinition");
    r_p_qty = bl2_param_offset(r_fn_spawn, "Quantity");
    r_p_stage = bl2_param_offset(r_fn_spawn, "GameStage");
    r_p_ctx = bl2_param_offset(r_fn_spawn, "ContextSource");
    r_p_out = bl2_param_offset(r_fn_spawn, "SpawnedInventory");
    r_p_loc = bl2_param_offset(r_fn_drop, "StartLocation");
    r_p_vel = bl2_param_offset(r_fn_drop, "StartVelocity");
    uintptr_t fc = bl2_find_function("WillowPlayerController", "ServerCompleteMission");
    r_p_mission = bl2_param_offset(fc, "Mission");
    r_off_reward = bl2_prop_offset("MissionDefinition", "Reward");
    r_off_ritems = bl2_prop_offset("RewardData", "RewardItems");
    r_off_rpools = bl2_prop_offset("RewardData", "RewardItemPools");
    r_off_pawn = bl2_prop_offset("Controller", "Pawn");
    r_fn_complete = (fc && r_p_mission >= 0 && r_off_pawn >= 0) ? fc : 0;
    if (!fn || !r_fn_spawn || !r_fn_drop || !r_pool || r_off_bal < 0 || r_off_loc < 0 || r_off_pools < 0 || r_p_def < 0 ||
        r_p_qty < 0 || r_p_stage < 0 || r_p_ctx < 0 || r_p_out < 0 || r_p_loc < 0 || r_p_vel < 0) return -1;
    r_fn_died = fn;
    Detour_Construct(&r_detour, DetourMode_x64);
    if (!Detour_DetourFunction(&r_detour, *(uintptr_t*)(fn + UFUNC_FUNC), (void*)interp_hook)) { r_fn_died = 0; return -2; }
    r_ready = 1;
    return 0;
}

void rando_tick(void) {
    r_last = 0;
    if (!r_on || r_ready) return;
    if (r_retry++ % 120) return;
    if (bl2_in_gameplay()) rando_init();
}

static const char* new_seed(void) {
    uint64_t t = sceKernelGetProcessTime();
    uint32_t v = (uint32_t)(t ^ (t >> 30) ^ ((uint64_t)roll() << 7)) & ((1u << 30) - 1);
    if (!v) v = 1;
    uint8_t d[9];
    char s[18];
    seed_make(v, g_newtags, SEED_VERSION, d);
    seed_to_string(d, s);
    int rc = apply_seed(s);
    if (rc == -3) { snprintf(g_msg, sizeof(g_msg), "Those options leave no items - turn on Base Game"); return g_msg; }
    if (rc) { snprintf(g_msg, sizeof(g_msg), "Couldn't make a seed (error %d)", rc); return g_msg; }
    r_on = 1;
    save_ini();
    snprintf(g_msg, sizeof(g_msg), "Seed %s: %d items in %d locations", g_seed_str, g_nitem, g_nloc);
    return g_msg;
}

int rando_rows(void) { return FIXED_ROWS + RANDO_NOPTS; }

const char* rando_label(int row) {
    static const char* const k[FIXED_ROWS] = {"Randomizer", "Seed", "New random seed", "Reload bl2_rando.ini", "Locations / items",
                                              "Guaranteed drops (test)", "Every enemy drops"};
    if (row < FIXED_ROWS) return k[row];
    return row - FIXED_ROWS < RANDO_NOPTS ? k_rando_opts[row - FIXED_ROWS].caption : "";
}

void rando_value(int row, char* out, int n) {
    switch (row) {
    case 0: snprintf(out, n, "<  %s  >", r_on ? (r_ready ? "ON" : "ON (open menu in game)") : "OFF"); return;
    case 1: snprintf(out, n, "%s", g_have_seed ? g_seed_str : "none"); return;
    case 2: case 3: snprintf(out, n, "press X"); return;
    case 4: if (g_have_seed) snprintf(out, n, "%d / %d", g_nloc, g_nitem); else snprintf(out, n, "-"); return;
    case 5: snprintf(out, n, "<  %s  >", g_guaranteed ? "ON" : "OFF"); return;
    case 6: if (g_every) snprintf(out, n, g_nextra > 0 ? "<  %d%%  >  +%d extra" : "<  %d%%  >", g_every, g_nextra); else snprintf(out, n, "<  OFF  >"); return;
    }
    int o = row - FIXED_ROWS;
    if (o >= 0 && o < RANDO_NOPTS) snprintf(out, n, "<  %s  >  new seeds", (g_newtags & k_rando_opts[o].bit) ? "ON" : "OFF");
    else out[0] = 0;
}

const char* rando_change(int row, int dir) {
    (void)dir;
    if (row == 0) {
        if (!r_on && !g_have_seed) return new_seed();
        r_on = !r_on;
        if (!r_on) restore_rewards();
        if (r_on && !r_ready && bl2_in_gameplay() && rando_init() != 0) { r_on = 0; save_ini(); return "Couldn't hook the game - load into gameplay first"; }
        save_ini();
        return r_on ? "Randomizer on" : "Randomizer off - enemies drop their normal loot";
    }
    if (row == 5) {
        g_guaranteed = !g_guaranteed;
        save_ini();
        return g_guaranteed ? "Guaranteed drops on - every roll drops the seeded item" : "Guaranteed drops off - normal drop rates";
    }
    if (row == 6) {
        int i = 0;
        while (i < N_EVERY - 1 && k_every[i] < g_every) i++;
        i = (i + (dir < 0 ? N_EVERY - 1 : 1)) % N_EVERY;
        g_every = k_every[i];
        save_ini();
        snprintf(g_msg, sizeof(g_msg), g_every ? "Every enemy now has a %d%% chance to drop a random item" : "Extra enemy drops off", g_every);
        return g_msg;
    }
    int o = row - FIXED_ROWS;
    if (o >= 0 && o < RANDO_NOPTS) {
        g_newtags ^= k_rando_opts[o].bit;
        save_ini();
        return "Option saved - it applies to the next new seed";
    }
    return NULL;
}

const char* rando_press(int row) {
    if (row == 2) return new_seed();
    if (row == 3) {
        int rc = load_ini();
        if (rc == -1) snprintf(g_msg, sizeof(g_msg), "Couldn't open %s", INI_PATH);
        else if (rc == -2) snprintf(g_msg, sizeof(g_msg), "That seed is from another Loot Randomizer version (need v%d)", SEED_VERSION);
        else if (rc) snprintf(g_msg, sizeof(g_msg), "The seed in %s isn't valid", INI_PATH);
        else snprintf(g_msg, sizeof(g_msg), "Loaded seed %s: %d items in %d locations", g_have_seed ? g_seed_str : "none", g_nitem, g_nloc);
        return g_msg;
    }
    if (row == 0 || row == 5 || row == 6) return rando_change(row, 1);
    return NULL;
}

void rando_unhook(void) {
    if (r_ready) { Detour_RestoreFunction(&r_detour); r_ready = 0; r_fn_died = 0; r_fn_complete = 0; }
    restore_rewards();
}
