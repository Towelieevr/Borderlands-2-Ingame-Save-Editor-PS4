#pragma once
#include <stdint.h>
#include <stdbool.h>

void bl2_set_base(uint64_t base);
int  bl2_init(void);
int  bl2_in_gameplay(void);
uintptr_t bl2_player_controller(void);
uintptr_t bl2_find_object(const char* cls, const char* name);
uintptr_t bl2_find_function(const char* owner, const char* name);
uintptr_t bl2_find_instance(const char* cls);
int       bl2_fonts(uintptr_t* out, int max); 
int       bl2_find_all(const char* cls, uintptr_t* out, int max);
const char* bl2_object_name(uintptr_t o);
int32_t   bl2_prop_offset(const char* cls, const char* prop);
uintptr_t bl2_find_path(const char* path);
int32_t   bl2_param_offset(uintptr_t fn, const char* name);
void      bl2_process_event(uintptr_t obj, uintptr_t fn, void* parms);
int       bl2_player_level(void);
uintptr_t bl2_find_pool(const char* name);
int       bl2_spawn_from_pool(uintptr_t pool, int level, int quantity);
int       bl2_decode_code(const char* code, uint8_t* out, int max);
const char* bl2_find_prefix(const char* s);
int       bl2_spawn_code(const char* code, int level);
int       bl2_unlock_all_cosmetics(void);
int       bl2_backpack_free(void);
int       bl2_add_to_backpack(uintptr_t inv);
void      bl2_backpack_keep(void);
uintptr_t bl2_live_pc(void);

int bl2_get_money(int64_t* out);          int bl2_set_money(int64_t v);
int bl2_get_eridium(int64_t* out);        int bl2_set_eridium(int64_t v);
int bl2_get_seraph(int64_t* out);         int bl2_set_seraph(int64_t v);
int bl2_get_torgue(int64_t* out);         int bl2_set_torgue(int64_t v);
int bl2_get_skill_points(int64_t* out);   int bl2_set_skill_points(int64_t v);
int bl2_get_badass_tokens(int64_t* out);  int bl2_set_badass_tokens(int64_t v);
int bl2_get_xp(int64_t* out);             int bl2_set_xp(int64_t v);

enum { SDU_BACKPACK, SDU_BANK, SDU_WEAPONS, SDU_AMMO_AR, SDU_AMMO_PISTOL, SDU_AMMO_SMG, SDU_AMMO_SHOTGUN,
       SDU_AMMO_SNIPER, SDU_AMMO_LAUNCHER, SDU_AMMO_GRENADE, N_SDU };
int bl2_sdu_get(int which, int64_t* out);
int bl2_sdu_set(int which, int64_t v);
int bl2_sdu_max(int which);
int bl2_sdu_max_all(void);
void bl2_sdu_refresh(void);
enum { CHEAT_HEALTH = 1, CHEAT_SHIELD = 2, CHEAT_GRENADES = 4, CHEAT_COOLDOWN = 8, CHEAT_AMMO = 16, CHEAT_NORELOAD = 32 };
void      bl2_cheats_tick(int flags);
uintptr_t bl2_speed_slot(void);
enum { PROF_KEYS, PROF_RANK, PROF_STAT0, N_PROF = PROF_STAT0 + 14 };
int bl2_prof_get(int which, int64_t* out);

int bl2_prof_set(int which, int64_t v);

