#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "plugin_common.h"
#include "bl2.h"

#define GNAMES_OFF   0x2482BF0
#define GOBJECTS_OFF 0x2503100
#define O_OUTER 0x40
#define O_NAME  0x48
#define O_CLASS 0x50
#define P_OFFSET 0x7C
#define POOL_VALUE 0xA0
#define POOL_OWNER 0x158
#define CURRENCY_STRIDE 20
#define CURRENCY_VALUE  0x0C
enum { CUR_CREDITS = 0, CUR_ERIDIUM = 1 };

typedef struct { uintptr_t data; int32_t count, max; } TArray;

static uint64_t g_base;
static uintptr_t g_pc, g_pri, g_xp_pool;
static int32_t g_off_skill = -1, g_off_currency = -1, g_off_tokens = -1;

void bl2_set_base(uint64_t base) { g_base = base; }

static TArray* gnames(void)   { return (TArray*)(g_base + GNAMES_OFF); }
static TArray* gobjects(void) { return (TArray*)(g_base + GOBJECTS_OFF); }

static const char* name_str(int32_t idx) {
    TArray* n = gnames();
    if (idx < 0 || idx >= n->count) return "";
    uintptr_t e = ((uintptr_t*)n->data)[idx];
    return e ? (const char*)(e + 0x10) : "";
}
static int32_t name_index(const char* s) {
    TArray* n = gnames();
    for (int32_t i = 0; i < n->count; i++) {
        uintptr_t e = ((uintptr_t*)n->data)[i];
        if (e && strcmp((const char*)(e + 0x10), s) == 0) return i;
    }
    return -1;
}
static inline int32_t obj_name(uintptr_t o)  { return *(int32_t*)(o + O_NAME); }
static inline uintptr_t obj_class(uintptr_t o) { return *(uintptr_t*)(o + O_CLASS); }
static inline uintptr_t obj_outer(uintptr_t o) { return *(uintptr_t*)(o + O_OUTER); }

static uintptr_t find_instance(int32_t cls_name, int32_t level_name) {
    TArray* g = gobjects();
    for (int32_t i = 0; i < g->count; i++) {
        uintptr_t o = ((uintptr_t*)g->data)[i];
        if (!o) continue;
        uintptr_t c = obj_class(o), out = obj_outer(o);
        if (c && obj_name(c) == cls_name && out && obj_name(out) == level_name) return o;
    }
    return 0;
}
static int32_t prop_offset(int32_t cls_name, int32_t prop_name) {
    TArray* g = gobjects();
    for (int32_t i = 0; i < g->count; i++) {
        uintptr_t o = ((uintptr_t*)g->data)[i];
        if (!o || obj_name(o) != prop_name) continue;
        uintptr_t out = obj_outer(o);
        if (out && obj_name(out) == cls_name) return *(int32_t*)(o + P_OFFSET);
    }
    return -1;
}

int bl2_init(void) {
    if (!g_base || gobjects()->count <= 0 || gnames()->count <= 0) return -1;
    int32_t n_pc = name_index("WillowPlayerController"), n_pri = name_index("WillowPlayerReplicationInfo");
    int32_t n_lvl = name_index("PersistentLevel"), n_xp = name_index("ExperienceResourcePool");
    if (n_pc < 0 || n_pri < 0 || n_lvl < 0) return -1;
    g_pc = find_instance(n_pc, n_lvl);
    g_pri = find_instance(n_pri, n_lvl);
    if (!g_pc || !g_pri) return -1;
    if (g_off_skill < 0) {
        g_off_skill    = prop_offset(n_pri, name_index("GeneralSkillPoints"));
        g_off_currency = prop_offset(n_pri, name_index("Currency"));
        g_off_tokens   = prop_offset(n_pc,  name_index("BadassTokensAvailable"));
    }
    g_xp_pool = 0;
    TArray* g = gobjects();
    for (int32_t i = 0; n_xp >= 0 && i < g->count; i++) {
        uintptr_t o = ((uintptr_t*)g->data)[i];
        if (o && obj_class(o) && obj_name(obj_class(o)) == n_xp && *(uintptr_t*)(o + POOL_OWNER) == g_pc) { g_xp_pool = o; break; }
    }
    return 0;
}

uintptr_t bl2_player_controller(void) { return g_pc; }

uintptr_t bl2_find_object(const char* cls, const char* name) {
    int32_t nc = name_index(cls), nn = name_index(name);
    if (nc < 0 || nn < 0) return 0;
    TArray* g = gobjects();
    for (int32_t i = 0; i < g->count; i++) {
        uintptr_t o = ((uintptr_t*)g->data)[i];
        if (o && obj_name(o) == nn && obj_class(o) && obj_name(obj_class(o)) == nc) return o;
    }
    return 0;
}
uintptr_t bl2_find_instance(const char* cls) {
    int32_t nc = name_index(cls);
    if (nc < 0) return 0;
    TArray* g = gobjects();
    for (int32_t i = 0; i < g->count; i++) {
        uintptr_t o = ((uintptr_t*)g->data)[i];
        if (!o || !obj_class(o) || obj_name(obj_class(o)) != nc) continue;
        if (strncmp(name_str(obj_name(o)), "Default__", 9) == 0) continue;
        return o;
    }
    return 0;
}
int32_t bl2_param_offset(uintptr_t fn, const char* name) {
    int32_t nn = name_index(name);
    if (!fn || nn < 0) return -1;
    TArray* g = gobjects();
    for (int32_t i = 0; i < g->count; i++) {
        uintptr_t o = ((uintptr_t*)g->data)[i];
        if (o && obj_outer(o) == fn && obj_name(o) == nn) return *(int32_t*)(o + P_OFFSET);
    }
    return -1;
}
uintptr_t bl2_find_function(const char* owner, const char* name) {
    int32_t no = name_index(owner), nn = name_index(name), nf = name_index("Function");
    if (no < 0 || nn < 0 || nf < 0) return 0;
    TArray* g = gobjects();
    for (int32_t i = 0; i < g->count; i++) {
        uintptr_t o = ((uintptr_t*)g->data)[i];
        if (!o || obj_name(o) != nn) continue;
        uintptr_t c = obj_class(o), out = obj_outer(o);
        if (c && obj_name(c) == nf && out && obj_name(out) == no) return o;
    }
    return 0;
}

typedef void (*PE_t)(void* self, void* fn, void* parms);
void bl2_process_event(uintptr_t obj, uintptr_t fn, void* parms) {
    PE_t f = (PE_t)(*(void***)obj)[0x200 / 8];
    f((void*)obj, (void*)fn, parms);
}

int bl2_player_level(void) {
    static int32_t off = -2;
    if (off == -2) off = prop_offset(name_index("WillowPlayerReplicationInfo"), name_index("ExpLevel"));
    return (g_pri && off >= 0) ? *(int32_t*)(g_pri + off) : 1;
}

typedef struct { uintptr_t* data; int32_t count, max; } ObjArray;
static uintptr_t s_fn_spawn, s_fn_add, s_cdo_itempool;
static int32_t s_p_def, s_p_stage, s_p_awesome, s_p_ctx, s_p_out, s_p_add_inv, s_off_pawn = -1, s_off_invmgr = -1;

static int spawner_init(void) {
    if (s_fn_spawn) return 0;
    s_fn_spawn = bl2_find_function("ItemPool", "SpawnBalancedInventoryFromPool");
    s_fn_add = bl2_find_function("WillowInventoryManager", "AddInventoryToBackpack");
    s_cdo_itempool = bl2_find_object("ItemPool", "Default__ItemPool");
    if (!s_fn_spawn || !s_fn_add || !s_cdo_itempool) { s_fn_spawn = 0; return -1; }
    s_p_def = bl2_param_offset(s_fn_spawn, "Definition");
    s_p_stage = bl2_param_offset(s_fn_spawn, "GameStage");
    s_p_awesome = bl2_param_offset(s_fn_spawn, "AwesomeLevel");
    s_p_ctx = bl2_param_offset(s_fn_spawn, "ContextSource");
    s_p_out = bl2_param_offset(s_fn_spawn, "SpawnedInventory");
    s_p_add_inv = bl2_param_offset(s_fn_add, "Inv");
    s_off_pawn = prop_offset(name_index("Controller"), name_index("Pawn"));
    s_off_invmgr = prop_offset(name_index("Pawn"), name_index("InvManager"));
    if (s_p_def < 0 || s_p_stage < 0 || s_p_ctx < 0 || s_p_out < 0 || s_p_add_inv < 0 || s_off_pawn < 0 || s_off_invmgr < 0) { s_fn_spawn = 0; return -2; }
    return 0;
}

uintptr_t bl2_find_pool(const char* name) { return bl2_find_object("ItemPoolDefinition", name); }

int bl2_spawn_from_pool(uintptr_t pool, int level, int quantity) {
    if (!pool || !g_pc || spawner_init() != 0) return -1;
    uintptr_t pawn = *(uintptr_t*)(g_pc + s_off_pawn);
    if (!pawn) return -2;
    uintptr_t invmgr = *(uintptr_t*)(pawn + s_off_invmgr);
    if (!invmgr) return -3;
    int added = 0;
    for (int q = 0; q < quantity; q++) {
        uint8_t parms[0x60];
        memset(parms, 0, sizeof(parms));
        *(uintptr_t*)(parms + s_p_def) = pool;
        *(int32_t*)(parms + s_p_stage) = level;
        if (s_p_awesome >= 0) *(int32_t*)(parms + s_p_awesome) = 0;
        *(uintptr_t*)(parms + s_p_ctx) = pawn;
        bl2_process_event(s_cdo_itempool, s_fn_spawn, parms);
        ObjArray* out = (ObjArray*)(parms + s_p_out);
        for (int32_t i = 0; out->data && i < out->count && i < 32; i++) {
            uintptr_t inv = out->data[i];
            if (!inv) continue;
            uint8_t ap[0x10];
            memset(ap, 0, sizeof(ap));
            *(uintptr_t*)(ap + s_p_add_inv) = inv;
            bl2_process_event(invmgr, s_fn_add, ap);
            added++;
        }
    }
    return added;
}

static uintptr_t c_fn_create, c_cdo_inventory;
static int32_t c_p_serial, c_p_source, c_p_ret, c_off_slotmax = -1, c_off_count = -1;

static int b64val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}
const char* bl2_find_prefix(const char* s) {
    for (; *s; s++)
        if ((s[0] | 0x20) == 'b' && (s[1] | 0x20) == 'l' && s[2] == '2' && s[3] == '(') return s;
    return NULL;
}

int bl2_decode_code(const char* code, uint8_t* out, int max) {
        const char* p = bl2_find_prefix(code);
    p = p ? p + 4 : code;
    uint32_t acc = 0; int bits = 0, n = 0;
    for (; *p && *p != ')'; p++) {
        if (*p == '=' ) break;
        int v = b64val(*p);
        if (v < 0) { if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') continue; return -1; }
        acc = (acc << 6) | (uint32_t)v; bits += 6;
        if (bits >= 8) { bits -= 8; if (n >= max) return -1; out[n++] = (uint8_t)(acc >> bits); }
    }
    return n;
}

static int codes_init(void) {
    if (c_fn_create) return 0;
    c_fn_create = bl2_find_function("WillowInventory", "CreateInventoryFromSerialNumber");
    c_cdo_inventory = bl2_find_object("WillowInventory", "Default__WillowInventory");
    if (!c_fn_create || !c_cdo_inventory) { c_fn_create = 0; return -1; }
    c_p_serial = bl2_param_offset(c_fn_create, "SerialNumber");
    c_p_source = bl2_param_offset(c_fn_create, "InAdditionalQueryInterfaceSource");
    c_p_ret = bl2_param_offset(c_fn_create, "ReturnValue");
    c_off_slotmax = prop_offset(name_index("WillowInventoryManager"), name_index("InventorySlotMax_Misc"));
    c_off_count = prop_offset(name_index("WillowInventoryManager"), name_index("BackpackInventoryCount"));
    if (c_p_serial < 0 || c_p_ret < 0) { c_fn_create = 0; return -2; }
    return 0;
}

static uintptr_t inventory_manager(void) {
    if (spawner_init() != 0 || !g_pc) return 0;
    uintptr_t pawn = *(uintptr_t*)(g_pc + s_off_pawn);
    return pawn ? *(uintptr_t*)(pawn + s_off_invmgr) : 0;
}

int bl2_in_gameplay(void) { return bl2_init() == 0 && inventory_manager() != 0; }

int bl2_backpack_free(void) {
    uintptr_t im = inventory_manager();
    if (!im || codes_init() != 0 || c_off_slotmax < 0 || c_off_count < 0) return -1;
    return *(int32_t*)(im + c_off_slotmax) - *(int32_t*)(im + c_off_count);
}

#define DEF_GRADE 0x18
#define DEF_STAGE 0x78
static uintptr_t l_fn_init_w, l_fn_init_i;
static int32_t l_off_def_w = -1, l_off_def_i = -1, l_p_w[3], l_p_i[3], l_off_exp_w = -1, l_off_exp_i = -1;
static int32_t exp_level_offset(const char* cls) {
    int32_t n = name_index("ExpLevel"), o = prop_offset(name_index(cls), n);
    if (o < 0) o = prop_offset(name_index("WillowInventory"), n);
    if (o < 0) o = prop_offset(name_index("Inventory"), n);
    return o;
}
static int relevel_init(void) {
    if (l_fn_init_w) return 0;
    l_fn_init_w = bl2_find_function("WillowWeapon", "InitializeFromDefinitionData");
    l_fn_init_i = bl2_find_function("WillowItem", "InitializeFromDefinitionData");
    l_off_def_w = prop_offset(name_index("WillowWeapon"), name_index("DefinitionData"));
    l_off_def_i = prop_offset(name_index("WillowItem"), name_index("DefinitionData"));
    if (!l_fn_init_w || !l_fn_init_i || l_off_def_w < 0 || l_off_def_i < 0) { l_fn_init_w = 0; return -1; }
    const char* pn[3] = {"NewDefinitionData", "InAdditionalQueryInterfaceSource", "bForceSelectNameParts"};
    for (int k = 0; k < 3; k++) { l_p_w[k] = bl2_param_offset(l_fn_init_w, pn[k]); l_p_i[k] = bl2_param_offset(l_fn_init_i, pn[k]); }
    if (l_p_w[0] < 0 || l_p_i[0] < 0) { l_fn_init_w = 0; return -2; }
    l_off_exp_w = exp_level_offset("WillowWeapon");
    l_off_exp_i = exp_level_offset("WillowItem");
    return 0;
}
static int relevel(uintptr_t inv, bool weapon, int level, uintptr_t source) {
    if (relevel_init() != 0) return -1;
    int32_t off = weapon ? l_off_def_w : l_off_def_i;
    int32_t* p = weapon ? l_p_w : l_p_i;
    int32_t exp = weapon ? l_off_exp_w : l_off_exp_i;
    uint8_t parms[0xA0];
    memset(parms, 0, sizeof(parms));
    memcpy(parms + p[0], (void*)(inv + off), 128);
    *(int32_t*)(parms + p[0] + DEF_GRADE) = level;
    *(int32_t*)(parms + p[0] + DEF_STAGE) = level;
    memcpy((void*)(inv + off), parms + p[0], 128);
    if (p[1] >= 0) *(uintptr_t*)(parms + p[1]) = source;
    bl2_process_event(inv, weapon ? l_fn_init_w : l_fn_init_i, parms);
    if (exp >= 0 && *(int32_t*)(inv + exp) != level) *(int32_t*)(inv + exp) = level;
    return 0;
}

int bl2_unlock_all_cosmetics(void) {
    if (!g_pc) return -1;
    uintptr_t mgr = bl2_find_instance("WillowCustomizationManager");
    uintptr_t fn_lock = bl2_find_function("WillowCustomizationManager", "SetCustomizationLocked");
    uintptr_t fn_save = bl2_find_function("WillowPlayerController", "WriteCustomizationUnlocksToProfile");
    int32_t n_def = name_index("CustomizationDefinition");
    if (!mgr || !fn_lock || n_def < 0) return -2;
    int32_t pd = bl2_param_offset(fn_lock, "Definition"), pc = bl2_param_offset(fn_lock, "Controller"), pl = bl2_param_offset(fn_lock, "bLocked");
    if (pd < 0 || pc < 0 || pl < 0) return -3;
    int count = 0;
    TArray* g = gobjects();
    for (int32_t i = 0; i < g->count; i++) {
        uintptr_t o = ((uintptr_t*)g->data)[i];
        if (!o || !obj_class(o) || obj_name(obj_class(o)) != n_def) continue;
        if (strncmp(name_str(obj_name(o)), "Default__", 9) == 0) continue;
        uint8_t parms[0x20];
        memset(parms, 0, sizeof(parms));
        *(uintptr_t*)(parms + pd) = o;
        *(uintptr_t*)(parms + pc) = g_pc;
        *(uint32_t*)(parms + pl) = 0;
        bl2_process_event(mgr, fn_lock, parms);
        count++;
    }
    if (fn_save) { uint8_t none[8] = {0}; bl2_process_event(g_pc, fn_save, none); }
    return count;
}

int bl2_spawn_code(const char* code, int level) {
    uint8_t bytes[40];
    int len = bl2_decode_code(code, bytes, sizeof(bytes));
    if (len < 6 || (bytes[0] & 0x7F) != 7) return -1;
    if (codes_init() != 0) return -2;
    uintptr_t im = inventory_manager();
    if (!im) return -2;
    int freeslots = bl2_backpack_free();
    if (freeslots == 0) return -4;
    uint8_t parms[0x50];
    memset(parms, 0, sizeof(parms));
    uint8_t* sn = parms + c_p_serial;
    memset(sn, 0xFF, 40);
    memcpy(sn, bytes, len);
    sn[0x28] = 4;
    *(int32_t*)(sn + 0x2C) = 0;
    *(int32_t*)(sn + 0x30) = len;
    if (c_p_source >= 0) *(uintptr_t*)(parms + c_p_source) = *(uintptr_t*)(g_pc + s_off_pawn);
    bl2_process_event(c_cdo_inventory, c_fn_create, parms);
    uintptr_t inv = *(uintptr_t*)(parms + c_p_ret);
    if (!inv) return -3;
    if (level > 0) relevel(inv, (bytes[0] & 0x80) != 0, level, *(uintptr_t*)(g_pc + s_off_pawn));
    uint8_t ap[0x10];
    memset(ap, 0, sizeof(ap));
    *(uintptr_t*)(ap + s_p_add_inv) = inv;
    bl2_process_event(im, s_fn_add, ap);
    return 1;
}

static int get_i32(uintptr_t obj, int32_t off, int64_t* out) {
    if (!obj || off < 0) return -1; *out = *(int32_t*)(obj + off); return 0;
}
static int set_i32(uintptr_t obj, int32_t off, int64_t v) {
    if (!obj || off < 0) return -1; *(int32_t*)(obj + off) = (int32_t)v; return 0;
}
static int get_currency(int idx, int64_t* out) {
    if (!g_pri || g_off_currency < 0) return -1;
    *out = *(int32_t*)(g_pri + g_off_currency + idx * CURRENCY_STRIDE + CURRENCY_VALUE); return 0;
}
static int set_currency(int idx, int64_t v) {
    if (!g_pri || g_off_currency < 0) return -1;
    uintptr_t e = g_pri + g_off_currency + idx * CURRENCY_STRIDE;
    *(int32_t*)(e + CURRENCY_VALUE) = (int32_t)v;
    *(int32_t*)(e + CURRENCY_VALUE + 4) = (int32_t)v;
    return 0;
}

int bl2_get_money(int64_t* o)          { return get_currency(CUR_CREDITS, o); }
int bl2_set_money(int64_t v)           { return set_currency(CUR_CREDITS, v); }
int bl2_get_eridium(int64_t* o)        { return get_currency(CUR_ERIDIUM, o); }
int bl2_set_eridium(int64_t v)         { return set_currency(CUR_ERIDIUM, v); }
int bl2_get_skill_points(int64_t* o)   { return get_i32(g_pri, g_off_skill, o); }
int bl2_set_skill_points(int64_t v)    { return set_i32(g_pri, g_off_skill, v); }
int bl2_get_badass_tokens(int64_t* o)  { return get_i32(g_pc, g_off_tokens, o); }
int bl2_set_badass_tokens(int64_t v)   { return set_i32(g_pc, g_off_tokens, v); }
int bl2_get_xp(int64_t* o) {
    if (!g_xp_pool) return -1; *o = (int64_t)*(float*)(g_xp_pool + POOL_VALUE); return 0;
}
int bl2_set_xp(int64_t v) {
    if (!g_xp_pool) return -1; *(float*)(g_xp_pool + POOL_VALUE) = (float)v; return 0;
}

static const char* k_ammo_res[7] = {"Ammo_Combat_Rifle", "Ammo_Repeater_Pistol", "Ammo_Patrol_SMG", "Ammo_Combat_Shotgun",
                                    "Ammo_Sniper_Rifle", "Ammo_Rocket_Launcher", "Ammo_Grenade_Protean"};
static uintptr_t u_fn_bank, u_fn_ready, u_fn_calc;
static const char* k_ammo_bm[7] = {"Upgrades_Ammo_AssaultRifle", "Upgrades_Ammo_Pistol", "Upgrades_Ammo_SMG", "Upgrades_Ammo_Shotgun",
                                   "Upgrades_Ammo_SniperRifles", "Upgrades_Ammo_RocketLauncher", "Upgrades_Ammo_Grenades"};
static int32_t u_off_ready = -1, u_off_bank = -1, u_off_chest = -1, u_off_maxslots = -1, u_off_bmarr = -1,
               u_off_pooldef = -1, u_off_res = -1, u_off_total = -1, u_off_bmidx = -1,
               u_p_bank = -1, u_p_ready = -1, u_p_calc = -1;

static int sdu_init(void) {
    if (u_fn_bank) return 0;
    if (codes_init() != 0 || c_off_slotmax < 0) return -1;
    int32_t n_im = name_index("WillowInventoryManager"), n_st = name_index("WillowInventoryStorage");
    int32_t n_rp = name_index("ResourcePool"), n_rpd = name_index("ResourcePoolDefinition");
    u_off_ready = prop_offset(n_im, name_index("WeaponReadyMax"));
    u_off_bank = prop_offset(n_im, name_index("TheBank"));
    u_off_chest = prop_offset(n_st, name_index("ChestSlots"));
    u_off_maxslots = prop_offset(n_st, name_index("MaxSlots"));
    u_off_bmarr = prop_offset(name_index("WillowPlayerController"), name_index("BlackMarketUpgrades"));
    u_off_pooldef = prop_offset(n_rp, name_index("Definition"));
    u_off_res = prop_offset(n_rpd, name_index("Resource"));
    u_off_total = prop_offset(n_rpd, name_index("TotalUpgradeCount"));
    u_off_bmidx = prop_offset(name_index("BlackMarketUpgradeDefinition"), name_index("CachedUpgradeIndex"));
    u_fn_ready = bl2_find_function("WillowInventoryManager", "SetWeaponReadyMax");
    u_fn_calc = bl2_find_function("ResourcePool", "CalculateBaseValues");
    uintptr_t fb = bl2_find_function("WillowInventoryManager", "ClientSetBankSlots");
    u_p_bank = bl2_param_offset(fb, "NewSlotCount");
    u_p_ready = bl2_param_offset(u_fn_ready, "NewSize");
    u_p_calc = bl2_param_offset(u_fn_calc, "bOnlyCalculateAttributeInitializedState");
    if (!fb || u_p_bank < 0 || u_off_bank < 0 || u_off_chest < 0 || u_off_pooldef < 0 || u_off_res < 0) return -2;
    u_fn_bank = fb;
    return 0;
}

static uintptr_t player_bank(uintptr_t im) { return u_off_bank >= 0 ? *(uintptr_t*)(im + u_off_bank) : 0; }

static uintptr_t s_pool[7], s_res[7];
static int32_t s_bm_idx[7] = {-1, -1, -1, -1, -1, -1, -1}, s_n_ammo_cls = -1, s_n_pool_cls = -1;
static bool is_pool(uintptr_t o) {
    uintptr_t c = obj_class(o);
    return c && (obj_name(c) == s_n_ammo_cls || obj_name(c) == s_n_pool_cls);
}

void bl2_sdu_refresh(void) {
    memset(s_pool, 0, sizeof(s_pool)); memset(s_res, 0, sizeof(s_res)); memset(s_bm_idx, 0xFF, sizeof(s_bm_idx));
    if (sdu_init() != 0 || !g_pc) return;
    int32_t n_res[7];
    s_n_ammo_cls = name_index("AmmoResourcePool");
    s_n_pool_cls = name_index("ResourcePool");
    for (int i = 0; i < 7; i++) n_res[i] = name_index(k_ammo_res[i]);
    TArray* g = gobjects();
    for (int32_t k = 0; k < g->count; k++) {
        uintptr_t o = ((uintptr_t*)g->data)[k];
        if (!o || !is_pool(o) || *(uintptr_t*)(o + POOL_OWNER) != g_pc) continue;
        uintptr_t def = *(uintptr_t*)(o + u_off_pooldef);
        uintptr_t res = def ? *(uintptr_t*)(def + u_off_res) : 0;
        for (int i = 0; res && i < 7; i++) if (obj_name(res) == n_res[i]) { s_pool[i] = o; s_res[i] = res; }
    }
    for (int i = 0; i < 7; i++) {
        uintptr_t up = bl2_find_object("BlackMarketUpgradeDefinition", k_ammo_bm[i]);
        s_bm_idx[i] = (up && u_off_bmidx >= 0) ? *(int32_t*)(up + u_off_bmidx) : -1;
    }
}
static uintptr_t ammo_pool(int i, uintptr_t* res_out) {
    uintptr_t o = s_pool[i];
    if (!o || !is_pool(o) || *(uintptr_t*)(o + POOL_OWNER) != g_pc) return 0;
    if (res_out) *res_out = s_res[i];
    return o;
}
static int32_t* ammo_level_slot(int i) {
    if (u_off_bmarr < 0 || !g_pc) return NULL;
    TArray* bm = (TArray*)(g_pc + u_off_bmarr);
    int32_t idx = s_bm_idx[i];
    return (idx >= 0 && idx < bm->count && bm->data) ? (int32_t*)bm->data + idx : NULL;
}

int bl2_sdu_max(int which) {
    if (sdu_init() != 0) return -1;
    uintptr_t im = inventory_manager();
    if (!im) return -1;
    switch (which) {
    case SDU_BACKPACK: return 39;
    case SDU_WEAPONS:  return 4;
    case SDU_BANK: { uintptr_t b = player_bank(im); return (b && u_off_maxslots >= 0) ? *(int32_t*)(b + u_off_maxslots) : -1; }
    default: {
        int mx = -1;
        for (int k = 0; k <= 7 && mx < 0; k++) {
            uintptr_t pool = ammo_pool(k == 0 ? which - SDU_AMMO_AR : k - 1, NULL);
            uintptr_t def = pool ? *(uintptr_t*)(pool + u_off_pooldef) : 0;
            if (def && u_off_total >= 0) mx = *(int32_t*)(def + u_off_total);
        }
        return mx;
    } }
}

int bl2_sdu_get(int which, int64_t* out) {
    if (sdu_init() != 0) return -1;
    uintptr_t im = inventory_manager();
    if (!im) return -1;
    switch (which) {
    case SDU_BACKPACK: *out = *(int32_t*)(im + c_off_slotmax); return 0;
    case SDU_WEAPONS:  if (u_off_ready < 0) return -1; *out = *(int32_t*)(im + u_off_ready); return 0;
    case SDU_BANK: { uintptr_t b = player_bank(im); if (!b) return -1; *out = *(int32_t*)(b + u_off_chest); return 0; }
    default: {
        int32_t* lv = ammo_level_slot(which - SDU_AMMO_AR);
        if (!lv) return -1;
        *out = *lv; return 0;
    } }
}

int bl2_sdu_set(int which, int64_t v) {
    if (sdu_init() != 0) return -1;
    uintptr_t im = inventory_manager();
    if (!im) return -1;
    int mx = bl2_sdu_max(which);
    if (mx >= 0 && v > mx) v = mx;
    if (v < 0) v = 0;
    uint8_t p[0x10]; memset(p, 0, sizeof(p));
    switch (which) {
    case SDU_BACKPACK: *(int32_t*)(im + c_off_slotmax) = (int32_t)v; return 0;
    case SDU_BANK:
        *(int32_t*)(p + u_p_bank) = (int32_t)v;
        bl2_process_event(im, u_fn_bank, p);
        return 0;
    case SDU_WEAPONS:
        if (!u_fn_ready || u_p_ready < 0) return -1;
        *(int32_t*)(p + u_p_ready) = (int32_t)v;
        bl2_process_event(im, u_fn_ready, p);
        return 0;
    default: {
        int32_t* lv = ammo_level_slot(which - SDU_AMMO_AR);
        if (!lv) return -1;
        *lv = (int32_t)v;
        uintptr_t pool = ammo_pool(which - SDU_AMMO_AR, NULL);
        if (pool && u_fn_calc && u_p_calc >= 0) { *(uint32_t*)(p + u_p_calc) = 1; bl2_process_event(pool, u_fn_calc, p); }
        return 0;
    } }
}

int bl2_sdu_max_all(void) {
    int n = 0;
    for (int i = 0; i < N_SDU; i++) { int mx = bl2_sdu_max(i); if (mx > 0 && bl2_sdu_set(i, mx) == 0) n++; }
    return n;
}

typedef struct { int32_t index, number; } FName;
typedef struct { FName id; TArray payload; int32_t eid; uint32_t consumable; int32_t consumed, amount; } EntitlementResult;
static uintptr_t p_fn_keycount, p_fn_replace, p_fn_rank, p_fn_rank_for, p_fn_setup, p_fn_toggle, p_account;
static int32_t p_off_ents = -1, p_off_e_id = -1, p_off_e_eid = -1, p_off_e_cons = -1, p_off_e_used = -1, p_off_e_amt = -1,
               p_off_e_payload = -1, p_off_points = -1, p_off_spent = -1, p_off_rewards = -1, n_goldenkey = -1;

static int profile_init(void) {
    if (!g_pc) return -1;
    if (p_fn_rank) {
        if (!p_account || !obj_class(p_account)) p_account = bl2_find_instance("GearboxAccountData");
        return 0;
    }
    int32_t n_ga = name_index("GearboxAccountData"), n_ge = name_index("GearboxAccountEntitlement"), n_pc = name_index("WillowPlayerController");
    p_fn_keycount = bl2_find_function("GearboxAccountData", "GetGoldenKeyCount");
    p_fn_replace = bl2_find_function("GearboxAccountData", "ReplaceEntitlements");
    p_fn_rank = bl2_find_function("WillowPlayerController", "GetBadassRank");
    p_fn_rank_for = bl2_find_function("WillowPlayerController", "GetBadassRankForPointsSpent");
    p_fn_setup = bl2_find_function("WillowPlayerController", "SetupBadassInfo");
    p_fn_toggle = bl2_find_function("WillowPlayerController", "ToggleBadassSkill");
    p_off_ents = prop_offset(n_ga, name_index("Entitlements"));
    p_off_e_id = prop_offset(n_ge, name_index("Identifier"));
    p_off_e_eid = prop_offset(n_ge, name_index("Id"));
    p_off_e_cons = prop_offset(n_ge, name_index("Consumable"));
    p_off_e_used = prop_offset(n_ge, name_index("Consumed"));
    p_off_e_amt = prop_offset(n_ge, name_index("ConsumableAmount"));
    p_off_e_payload = prop_offset(n_ge, name_index("Payload"));
    p_off_points = prop_offset(n_pc, name_index("BadassPoints"));
    p_off_spent = prop_offset(n_pc, name_index("BadassPointsSpent"));
    p_off_rewards = prop_offset(n_pc, name_index("BadassRewardsEarned"));
    n_goldenkey = name_index("GoldenKey");
    p_account = bl2_find_instance("GearboxAccountData");
    if (!p_fn_rank || !p_fn_rank_for) { p_fn_rank = 0; return -2; }
    return 0;
}

static int get_golden_keys(int64_t* out) {
    if (!p_account || !p_fn_keycount) return -1;
    uint8_t p[0x10]; memset(p, 0, sizeof(p));
    bl2_process_event(p_account, p_fn_keycount, p);
    *out = *(int32_t*)(p + 4); return 0;
}
static int set_golden_keys(int64_t v) {
    if (!p_account || !p_fn_replace || p_off_ents < 0 || p_off_e_id < 0 || p_off_e_amt < 0 || p_off_e_used < 0 || n_goldenkey < 0) return -1;
    TArray* cur = (TArray*)(p_account + p_off_ents);
    EntitlementResult list[64];
    int n = 0; bool found = false;
    for (int32_t i = 0; i < cur->count && n < 63; i++) {
        uintptr_t e = ((uintptr_t*)cur->data)[i];
        if (!e) continue;
        EntitlementResult* r = &list[n++];
        memset(r, 0, sizeof(*r));
        r->id = *(FName*)(e + p_off_e_id);
        if (p_off_e_payload >= 0) r->payload = *(TArray*)(e + p_off_e_payload);
        r->eid = p_off_e_eid >= 0 ? *(int32_t*)(e + p_off_e_eid) : 0;
        r->consumable = p_off_e_cons >= 0 ? (*(uint32_t*)(e + p_off_e_cons) & 1) : 1;
        r->consumed = *(int32_t*)(e + p_off_e_used);
        r->amount = *(int32_t*)(e + p_off_e_amt);
        if (r->id.index == n_goldenkey) { found = true; r->consumable = 1; r->amount = r->consumed + (int32_t)v; }
    }
    if (!found) {
        EntitlementResult* r = &list[n++];
        memset(r, 0, sizeof(*r));
        r->id.index = n_goldenkey; r->consumable = 1; r->amount = (int32_t)v;
    }
    uint8_t p[0x40]; memset(p, 0, sizeof(p));
    TArray* ne = (TArray*)p;
    ne->data = (uintptr_t)list; ne->count = ne->max = n;
    bl2_process_event(p_account, p_fn_replace, p);
    return 0;
}

static int rank_for_points(int32_t pts) {
    uint8_t p[0x10]; memset(p, 0, sizeof(p));
    *(int32_t*)p = pts;
    bl2_process_event(g_pc, p_fn_rank_for, p);
    return *(int32_t*)(p + 4);
}
static int get_badass_rank(int64_t* out) {
    uint8_t p[0x10]; memset(p, 0, sizeof(p));
    bl2_process_event(g_pc, p_fn_rank, p);
    *out = *(int32_t*)p; return 0;
}
static int set_badass_rank(int64_t v) {
    if (p_off_points < 0 || p_off_spent < 0) return -1;
    int32_t lo = 0, hi = 100000000;
    if (rank_for_points(hi) < v) v = rank_for_points(hi);
    while (lo < hi) { int32_t mid = lo + (hi - lo) / 2; if (rank_for_points(mid) >= v) hi = mid; else lo = mid + 1; }
    *(int32_t*)(g_pc + p_off_points) = lo;
    *(int32_t*)(g_pc + p_off_spent) = lo;
    if (p_fn_setup) { uint8_t none[8] = {0}; bl2_process_event(g_pc, p_fn_setup, none); }
    return 0;
}

static int32_t* badass_reward(int i) {
    if (p_off_rewards < 0) return NULL;
    TArray* a = (TArray*)(g_pc + p_off_rewards);
    return (a->data && i < a->count) ? (int32_t*)a->data + i : NULL;
}
static void reapply_badass_skill(void) {
    if (!p_fn_toggle) return;
    uint8_t none[8] = {0};
    bl2_process_event(g_pc, p_fn_toggle, none);
    bl2_process_event(g_pc, p_fn_toggle, none);
}

int bl2_prof_get(int which, int64_t* out) {
    if (profile_init() != 0) return -1;
    if (which == PROF_KEYS) return get_golden_keys(out);
    if (which == PROF_RANK) return get_badass_rank(out);
    int32_t* r = badass_reward(which - PROF_STAT0);
    if (!r) return -1;
    *out = *r; return 0;
}
int bl2_prof_set(int which, int64_t v) {
    if (profile_init() != 0) return -1;
    if (v < 0) v = 0;
    if (which == PROF_KEYS) return set_golden_keys(v);
    if (which == PROF_RANK) return set_badass_rank(v);
    int32_t* r = badass_reward(which - PROF_STAT0);
    if (!r) return -1;
    *r = (int32_t)v;
    reapply_badass_skill();
    return 0;
}

#define POOL_MAX     0x88
#define POOLREF_DATA 0x10
#define WPP_VTABLE   0x22B6800
#define SPEED_SLOT   0xD00
#define SPEED_FN     0x10D2030
static uintptr_t k_engine, k_grenade, k_fn_cd_on, k_fn_cd_reset;
static int32_t k_off_players = -1, k_off_actor = -1, k_off_pawn = -1, k_off_health = -1, k_off_shield = -1,
               k_n_pool = -1, k_n_grenade = -1, k_off_weapon = -1, k_off_ammo = -1, k_off_reload = -1, k_n_ppawn = -1;
static uint32_t k_frames;
static bool k_ready;

static int cheats_init(void) {
    if (k_ready) return k_engine ? 0 : -1;
    k_ready = true;
    k_engine = bl2_find_instance("WillowGameEngine");
    k_off_players = prop_offset(name_index("Engine"), name_index("GamePlayers"));
    k_off_actor = prop_offset(name_index("Player"), name_index("Actor"));
    k_off_pawn = prop_offset(name_index("Controller"), name_index("Pawn"));
    k_off_health = prop_offset(name_index("Pawn"), name_index("HealthPool"));
    k_off_shield = prop_offset(name_index("WillowPawn"), name_index("ShieldArmor"));
    k_n_pool = name_index("ResourcePool");
    k_n_grenade = name_index("Ammo_Grenade_Protean");
    k_fn_cd_on = bl2_find_function("WillowPlayerController", "IsActionSkillOnCooldown");
    k_fn_cd_reset = bl2_find_function("WillowPlayerController", "ResetSkillCooldown");
        k_off_weapon = prop_offset(name_index("Pawn"), name_index("Weapon"));
    k_off_ammo = prop_offset(name_index("WillowWeapon"), name_index("AmmoPool"));
    k_off_reload = prop_offset(name_index("WillowWeapon"), name_index("ReloadCnt"));
    k_n_ppawn = name_index("WillowPlayerPawn");
    sdu_init();
    if (!k_engine || k_off_players < 0 || k_off_actor < 0 || k_off_pawn < 0) { k_engine = 0; return -1; }
    return 0;
}

static uintptr_t live_pc(void) {
    TArray* lp = (TArray*)(k_engine + k_off_players);
    uintptr_t p = (lp->data && lp->count > 0) ? ((uintptr_t*)lp->data)[0] : 0;
    return p ? *(uintptr_t*)(p + k_off_actor) : 0;
}
static uintptr_t pool_ref(uintptr_t obj, int32_t off) {
    return (obj && off >= 0) ? *(uintptr_t*)(obj + off + POOLREF_DATA) : 0;
}
static void refill(uintptr_t pool) {
    if (pool) *(float*)(pool + POOL_VALUE) = *(float*)(pool + POOL_MAX);
}
static bool is_grenade_pool(uintptr_t o, uintptr_t pc) {
    uintptr_t c = obj_class(o);
    if (!c || obj_name(c) != k_n_pool || u_off_pooldef < 0 || u_off_res < 0) return false;
    if (*(uintptr_t*)(o + POOL_OWNER) != pc) return false;
    uintptr_t def = *(uintptr_t*)(o + u_off_pooldef);
    uintptr_t res = def ? *(uintptr_t*)(def + u_off_res) : 0;
    return res && obj_name(res) == k_n_grenade;
}
static uintptr_t find_grenade_pool(uintptr_t pc) {
    TArray* g = gobjects();
    for (int32_t i = 0; i < g->count; i++) {
        uintptr_t o = ((uintptr_t*)g->data)[i];
        if (o && is_grenade_pool(o, pc)) return o;
    }
    return 0;
}

void bl2_cheats_tick(int flags) {
    if (!flags || cheats_init() != 0) return;
    uintptr_t pc = live_pc();
    uintptr_t pawn = pc ? *(uintptr_t*)(pc + k_off_pawn) : 0;
    if (!pawn || !obj_class(pawn) || obj_name(obj_class(pawn)) != k_n_ppawn) return;
    if (flags & CHEAT_HEALTH) refill(pool_ref(pawn, k_off_health));
    if (flags & CHEAT_SHIELD) refill(pool_ref(pawn, k_off_shield));
    if (flags & CHEAT_GRENADES) {
        if (k_frames++ % 60 == 0) k_grenade = find_grenade_pool(pc);
        if (k_grenade && is_grenade_pool(k_grenade, pc)) refill(k_grenade);
    }
    uintptr_t wpn = k_off_weapon >= 0 ? *(uintptr_t*)(pawn + k_off_weapon) : 0;
    if (wpn && (flags & CHEAT_AMMO)) refill(pool_ref(wpn, k_off_ammo));
    if (wpn && (flags & CHEAT_NORELOAD) && k_off_reload >= 0) *(int32_t*)(wpn + k_off_reload) = 0;
    if ((flags & CHEAT_COOLDOWN) && k_fn_cd_on && k_fn_cd_reset) {
        uint8_t p[0x10]; memset(p, 0, sizeof(p));
        bl2_process_event(pc, k_fn_cd_on, p);
        if (*(uint32_t*)p & 1) { memset(p, 0, sizeof(p)); bl2_process_event(pc, k_fn_cd_reset, p); }
    }
}

uintptr_t bl2_speed_slot(void) {
    if (!g_base) return 0;
    uintptr_t slot = g_base + WPP_VTABLE + SPEED_SLOT;
    return *(uintptr_t*)slot == g_base + SPEED_FN ? slot : 0;
}
