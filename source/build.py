#!/usr/bin/env python3

import glob, os, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.abspath(os.path.join(HERE, "..", "tools"))
SDK = os.path.join(TOOLS, "GoldHEN_Plugins_SDK")
REPO = os.path.join(TOOLS, "GoldHEN_Plugins_Repository")
sys.path.insert(0, os.path.join(TOOLS, "ps4sdk"))
import ps4sdk

LIBS = ["kernel", "SceLibcInternal", "ScePad", "SceSysmodule", "SceSystemService", "SceUserService"]

def plugin(d):
    d = os.path.abspath(d); name = os.path.basename(d.rstrip("\\/"))
    out = os.path.join(d, "build"); os.makedirs(out, exist_ok=True)
    for o in glob.glob(os.path.join(out, "*.o")): os.remove(o)
    gv = os.path.join(REPO, "common", "git_ver.h")
    if not os.path.exists(gv):
        open(gv, "w").write('#pragma once\n#define GIT_COMMIT "local"\n#define GIT_VER "local"\n#define GIT_NUM 0\n#define BUILD_DATE __DATE__\n')
    inc = ["-I", os.path.join(SDK, "include"), "-I", os.path.join(REPO, "common"), "-I", os.path.join(d, "include")]
    sdk_flags = ["-O2", "-std=c11", "-march=btver2", "-mtune=btver2", "-fvisibility=hidden", *inc]
    flags = ["-O2", "-Wno-c99-designator", "-fvisibility=hidden", "-D__FINAL__=1", *inc]
    objs = []
    def cc(src, f, tag=""):
        o = os.path.join(out, tag + os.path.basename(src) + ".o"); ps4sdk.compile(src, o, f); objs.append(o)
    for src in glob.glob(os.path.join(SDK, "source", "*.c")): cc(src, sdk_flags, "sdk_")
    cc(os.path.join(SDK, "crt", "crtprx.c"), ["-O2", "-ffreestanding", "-fno-builtin", *inc], "sdk_")
    for src in glob.glob(os.path.join(REPO, "common", "*.c")) + glob.glob(os.path.join(d, "source", "*.c")): cc(src, flags)
    elf = os.path.join(out, name + ".elf")
    ps4sdk.link(objs, elf, LIBS)
    prx = ps4sdk.prx(elf, os.path.join(out, name + ".prx"), name)
    print("built", prx, os.path.getsize(prx), "bytes")

SEAL_MAGIC = struct.pack("<Q", 0x4c41455345324c42)
SEAL_SALT = 0x546f77656c696565

def sections(d):
    shoff, = struct.unpack_from("<Q", d, 0x28)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 0x3A)
    hdrs = [struct.unpack_from("<IIQQQQIIQQ", d, shoff + i * shentsize) for i in range(shnum)]
    strtab = hdrs[shstrndx][4]
    return {d[strtab + h[0]:d.index(b"\0", strtab + h[0])].decode(): h for h in hdrs}

def seal(elf):
    d = bytearray(open(elf, "rb").read())
    s = sections(d)
    text, ro, data = s[".text"], s[".rodata"], s[".data"]
    def fnv(h, b):
        for x in b: h = ((h ^ x) * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
        return h
    tb, rb = bytes(d[text[4]:text[4] + text[5]]), bytes(d[ro[4]:ro[4] + ro[5]])
    h = fnv(fnv(0xcbf29ce484222325 ^ SEAL_SALT, tb), rb)
    lo, hi = data[4], data[4] + data[5]
    at = d.find(SEAL_MAGIC, lo, hi)
    if at < 0 or d.find(SEAL_MAGIC, at + 1, hi) >= 0: sys.exit("seal: g_seal not found (exactly once) in .data")
    struct.pack_into("<5Q", d, at + 8, 0, text[5], ro[3] - text[3], ro[5], h)
    open(elf, "wb").write(d)
    return [tb, rb]

if __name__ == "__main__":
    {"plugin": lambda: plugin(sys.argv[2])}[sys.argv[1]]()
