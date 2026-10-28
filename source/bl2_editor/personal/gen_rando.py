import ast, os, sys

SRC = sys.argv[1]
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "rando_data.h")

TAGS = ["BaseGame", "PiratesBooty", "CampaignOfCarnage", "HammerlocksHunt", "DragonKeep", "FightForSanctuary",
        "BloodyHarvest", "WattleGobbler", "MercenaryDay", "WeddingDayMassacre", "SonOfCrawmerax", "UVHMPack",
        "DigistructPeak", "ShortMission", "LongMission", "VeryLongMission", "Slaughter", "UniqueEnemy", "SlowEnemy",
        "RareEnemy", "VeryRareEnemy", "MobFarm", "Raid", "MissionLocation", "EvolvedEnemy", "DigistructEnemy",
        "Vendor", "Miscellaneous", "DuplicateItems", "EnableHints", "VehicleMission", "Freebie"]
BIT = {n: 1 << i for i, n in enumerate(TAGS)}
BIT["Excluded"] = 1 << 36
CONTENT = sum(BIT[n] for n in TAGS[:13])
ENEMY = sum(BIT[n] for n in ("UniqueEnemy", "SlowEnemy", "RareEnemy", "VeryRareEnemy", "MobFarm", "EvolvedEnemy", "DigistructEnemy"))
CHARS = ["Assassin", "Mercenary", "Siren", "Soldier", "Psycho", "Mechromancer"]


def tag(node):
    if node is None: return 0
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.BitOr): return tag(node.left) | tag(node.right)
    if isinstance(node, ast.Attribute) and isinstance(node.value, ast.Name) and node.value.id == "Tag": return BIT[node.attr]
    if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id == "Tag": return ast.literal_eval(node.args[0]) if node.args else 0
    if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id == "getattr": return BIT.get(ast.literal_eval(node.args[1]), 0)
    raise ValueError(ast.dump(node))


def tree(path):
    return ast.parse(open(os.path.join(SRC, path), encoding="utf-8").read())


def tuple_of(t, name):
    for n in ast.walk(t):
        if isinstance(n, (ast.Assign, ast.AnnAssign)):
            tgt = n.targets[0] if isinstance(n, ast.Assign) else n.target
            if isinstance(tgt, ast.Name) and tgt.id == name: return n.value.elts
    raise KeyError(name)


def fname(call): return call.func.id if isinstance(call.func, ast.Name) else call.func.attr
def kw(call, k): return next((x.value for x in call.keywords if x.arg == k), None)


def weight(node):
    if isinstance(node, ast.Attribute) and isinstance(node.value, ast.Name) and node.value.id == "Character": return -1 - CHARS.index(node.attr)
    return float(ast.literal_eval(node))


def item(call):
    f, a = fname(call), call.args
    path = ast.literal_eval(a[0])
    if f in ("ClassMod", "Gen2ClassMod"): w, c = weight(a[2]), (a[3] if len(a) > 3 else kw(call, "content") or kw(call, "tags"))
    elif f in ("BanditGrenade", "MultiBarrelWeapon"): w, c = 1.0, (a[2] if len(a) > 2 else kw(call, "content"))
    else:
        w = weight(a[1]) if len(a) > 1 else (weight(kw(call, "weight")) if kw(call, "weight") else 1.0)
        c = a[2] if len(a) > 2 else kw(call, "content")
    content = tag(c)
    if not content & CONTENT: content |= BIT["BaseGame"]
    return path, w, content


pools = {}
for call in tuple_of(tree("Mod/bl2/items.py"), "Items"):
    name = ast.literal_eval(call.args[0])
    fb = kw(call, "fallbacks")
    pools[name] = ([item(x) for x in call.args[2:]], [item(x) for x in fb.elts] if fb is not None else [])

missions, enemies, mdefs = {}, {}, {}
for call in tuple_of(tree("Mod/bl2/locations.py"), "Locations"):
    f = fname(call)
    name = ast.literal_eval(call.args[0])
    if f == "Mission":
        missions[name] = tag(kw(call, "tags"))
        mdefs.setdefault(name, [ast.literal_eval(d.args[0]).split(".")[-1] for d in call.args[1:]
                                if isinstance(d, ast.Call) and fname(d) == "MissionDefinition"])
        continue
    if f != "Enemy": continue
    pawns = []
    for d in call.args[1:]:
        if isinstance(d, ast.Call) and fname(d) == "Pawn":
            tr, ev = kw(d, "transform"), kw(d, "evolved")
            for b in d.args:
                pawns.append((ast.literal_eval(b).split(".")[-1], ast.literal_eval(tr) if tr is not None else -1, ast.literal_eval(ev) if ev is not None else -1))
    r = kw(call, "rarities")
    enemies.setdefault(name, (pawns, tag(kw(call, "tags")), ast.literal_eval(kw(call, "mission")) if kw(call, "mission") is not None else None, list(ast.literal_eval(r)) if r is not None else None))


def rarities(spec_tags, mission, spec):
    tags = spec_tags
    if mission:
        tags |= BIT["MissionLocation"] | missions[mission]
    if not tags & CONTENT: tags |= BIT["BaseGame"]
    if not tags & ENEMY: tags |= BIT["UniqueEnemy"]
    if spec: return spec
    out = []
    if tags & BIT["SlowEnemy"]: out += [33]
    if tags & BIT["MobFarm"]: out += [3]
    if tags & BIT["RareEnemy"]: out += [33]
    if tags & BIT["VeryRareEnemy"]: out += [100, 50, 50]
    if tags & BIT["Raid"]: out += [100, 100, 100, 50, 50, 50]
    if tags & BIT["EvolvedEnemy"]: out += [50, 50]
    if tags & BIT["MissionLocation"]: out += [50]
    if tags & BIT["LongMission"]: out += [50]
    if tags & BIT["VeryLongMission"]: out += [100, 50]
    return out or [15]


v7 = tree("Mod/bl2/v7.py")
v7_items = [(ast.literal_eval(c.args[0]), tag(c.args[1])) for c in tuple_of(v7, "Items")]
v7_locs = [(ast.literal_eval(c.args[0]), tag(c.args[1])) for c in tuple_of(v7, "Locations")]


def cs(s): return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


o = ["#pragma once", "#include <stdint.h>", ""]
o.append("typedef struct { const char* path; float weight; int8_t chr; uint64_t content; } RandoItem;")
o.append("typedef struct { const char* name; uint64_t tags; uint16_t first, count, fb_first, fb_count; } RandoPool;")
o.append("typedef struct { const char* balance; int16_t transform, evolved; uint16_t loc; } RandoPawn;")
o.append("typedef struct { const char* name; uint64_t tags; uint8_t nrar, rar[12]; } RandoLoc;")
o.append("")
flat, plist = [], []
for name, t in v7_items:
    its, fbs = pools[name]
    first = len(flat); flat += its
    fbf = len(flat); flat += fbs
    plist.append((name, t, first, len(its), fbf, len(fbs)))
o.append(f"#define RANDO_NITEMS {len(flat)}")
o.append("static const RandoItem k_rando_items[RANDO_NITEMS] = {")
for p, w, c in flat:
    chr_ = -1 - int(w) if w < 0 else -1
    o.append(f"    {{{cs(p)}, {1.0 if w < 0 else w:.4f}f, {chr_}, 0x{c:X}ULL}},")
o.append("};")
o.append(f"#define RANDO_NPOOLS {len(plist)}")
o.append("static const RandoPool k_rando_pools[RANDO_NPOOLS] = {")
for name, t, f_, n_, ff, fn in plist: o.append(f"    {{{cs(name)}, 0x{t:X}ULL, {f_}, {n_}, {ff}, {fn}}},")
o.append("};")
locs, pawns, mlist = [], [], []
for i, (name, t) in enumerate(v7_locs):
    rar = []
    if name.startswith("Enemy: "):
        e = enemies.get(name[7:])
        if e:
            rar = rarities(e[1], e[2], e[3])
            for b, tr, ev in e[0]: pawns.append((b, tr, ev, i))
    elif name.startswith("Mission: ") and mdefs.get(name[9:]):
        rar = [100]
        if t & BIT["LongMission"]: rar += [100]
        if t & BIT["VeryLongMission"]: rar += [100, 100, 100]
        if t & BIT["Raid"]: rar += [100, 100, 100]
        for d in mdefs[name[9:]]: mlist.append((d, i))
    locs.append((name, t, rar[:12]))
o.append(f"#define RANDO_NLOCS {len(locs)}")
o.append("static const RandoLoc k_rando_locs[RANDO_NLOCS] = {")
for name, t, rar in locs:
    o.append(f"    {{{cs(name)}, 0x{t:X}ULL, {len(rar)}, {{{', '.join(map(str, rar)) or '0'}}}}},")
o.append("};")
o.append(f"#define RANDO_NPAWNS {len(pawns)}")
o.append("static const RandoPawn k_rando_pawns[RANDO_NPAWNS] = {")
for b, tr, ev, i in pawns: o.append(f"    {{{cs(b)}, {tr}, {ev}, {i}}},")
o.append("};")
o.append("typedef struct { const char* def; uint16_t loc; } RandoMission;")
o.append(f"#define RANDO_NMISSIONS {len(mlist)}")
o.append("static const RandoMission k_rando_missions[RANDO_NMISSIONS] = {")
for d, i in mlist: o.append(f"    {{{cs(d)}, {i}}},")
o.append("};")
o.append("static const char* const k_rando_whitelist[] = {")
wl = None
for n in ast.walk(tree("Mod/bl2/__init__.py")):
    if isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name) and n.targets[0].id == "pool_whitelist": wl = [ast.literal_eval(x) for x in n.value.elts]
o += [f"    {cs(x)}," for x in wl] + ["};", f"#define RANDO_NWHITE {len(wl)}", ""]
opts = []
for n in ast.walk(tree("Mod/bl2/__init__.py")):
    if isinstance(n, ast.For) and isinstance(n.target, ast.Tuple) and len(n.target.elts) == 5:
        for e in n.iter.elts:
            opts.append((tag(e.elts[0]), e.elts[1].attr, ast.literal_eval(e.elts[2]), ast.literal_eval(e.elts[3])))
o.append("typedef struct { uint64_t bit; const char* caption; uint8_t category, def; } RandoOpt;")
cats = ["Content", "Missions", "Enemies", "Other", "Settings"]
o.append(f"#define RANDO_NOPTS {len(opts)}")
o.append("static const RandoOpt k_rando_opts[RANDO_NOPTS] = {")
for b, c, d, cap in opts: o.append(f"    {{0x{b:X}ULL, {cs(cap)}, {cats.index(c)}, {int(d)}}},")
o.append("};")
o.append(f"#define RANDO_DEFAULT_TAGS 0x{sum(b for b, c, d, cap in opts if d):X}ULL")
o.append("")
open(OUT, "w", newline="\n").write("\n".join(o))
print(f"items {len(flat)}  pools {len(plist)}  locations {len(locs)}  enemy pawns {len(pawns)}  whitelist {len(wl)}  -> {OUT}")
missing = [n for n, _ in v7_items if n not in pools]
print("v7 items missing from items.py:", missing or "none")
print("enemy locations without Pawn data:", sum(1 for n, _, r in locs if n.startswith("Enemy: ") and not r))
