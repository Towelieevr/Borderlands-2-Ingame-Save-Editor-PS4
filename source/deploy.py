#!/usr/bin/env python3

import difflib, ftplib, io, os, sys

PS4, PORT = "YOUR_PS4_IP", 2121
HERE = os.path.dirname(os.path.abspath(__file__))
TITLE = "CUSA01401"
PRX_LINE = "/data/GoldHEN/plugins/bl2_editor.prx"
XEMO_LINE = "/data/GoldHEN/plugins/XeMo.prx"
FILES = [
    (os.path.join(HERE, "bl2_editor", "build", "bl2_editor.prx"), PRX_LINE),
    (os.path.join(HERE, "assets", "bl2_editor_icon.png"), "/data/GoldHEN/bl2_editor_icon.png"),
    (os.path.join(HERE, "bl2_codes.txt"), "/data/GoldHEN/bl2_codes.txt"),
]

def is_head(line):
    s = line.strip()
    return s.startswith("[") and s.endswith("]")

def sections(lines, title):
    heads = [i for i, l in enumerate(lines) if is_head(l)] + [len(lines)]
    return [(h, nxt) for h, nxt in zip(heads, heads[1:]) if lines[h].strip().upper() == f"[{title}]"]

def add_line(ini):
    lines = ini.splitlines()
    secs = sections(lines, TITLE)
    if any(l.strip() == PRX_LINE for h, nxt in secs for l in lines[h + 1:nxt]): return ini
    if secs:
        h, j = secs[-1]
        while j > h + 1 and not lines[j - 1].strip(): j -= 1
        lines.insert(j, PRX_LINE)
    else:
        while lines and not lines[-1].strip(): lines.pop()
        lines += ["", f"[{TITLE}]", PRX_LINE]
    return "\n".join(lines) + "\n"

def remove_line(ini):
    lines = ini.splitlines()
    drop = {i for i, l in enumerate(lines) if l.strip() == PRX_LINE}
    for h, nxt in sections(lines, TITLE):
        body = lines[h + 1:nxt]
        if all(l.strip() in ("", PRX_LINE) for l in body):
            drop.update(range(h, nxt))
            if h > 0 and not lines[h - 1].strip(): drop.add(h - 1)
    return "\n".join(l for i, l in enumerate(lines) if i not in drop) + "\n"

def order_ok(ini):
    lines = ini.splitlines()
    tagged = [l.strip() for h, nxt in sections(lines, TITLE) for l in lines[h + 1:nxt]]
    if PRX_LINE not in tagged or XEMO_LINE not in tagged: return True
    return tagged.index(PRX_LINE) > max(i for i, l in enumerate(tagged) if l == XEMO_LINE)

def read(ftp, path):
    buf = io.BytesIO(); ftp.retrbinary("RETR " + path, buf.write)
    return buf.getvalue()

def put(ftp, local, remote):
    with open(local, "rb") as f: ftp.storbinary("STOR " + remote, f)
    if remote.endswith(".prx"): ftp.sendcmd("SITE CHMOD 777 " + remote)
    want = local[:-4] + ".oelf" if local.endswith(".prx") else local
    got, exp = read(ftp, remote), open(want, "rb").read()
    if want != local: got, exp = got[:0x3A] + got[0x40:], exp[:0x3A] + exp[0x40:]
    if got != exp: sys.exit(f"upload check failed: {remote} read back {len(got)} bytes, doesn't match {os.path.basename(want)}")
    print("uploaded", remote, os.path.getsize(local), "bytes (verified)")

def write_ini(ftp, old, new):
    backup = os.path.join(HERE, "plugins.ini.backup")
    if not os.path.exists(backup): open(backup, "w", newline="").write(old)
    ftp.storbinary("STOR /data/GoldHEN/plugins.ini", io.BytesIO(new.encode()))

def main():
    flags = set(sys.argv[1:])
    dry = "--dry-run" in flags
    missing = [l for l, r in FILES if not os.path.exists(l)]
    if missing and "--remove" not in flags: sys.exit("missing: " + " ".join(missing) + " (run build.py plugin bl2_editor first)")
    ftp = ftplib.FTP(); ftp.connect(PS4, PORT, timeout=10); ftp.login()
    ini = read(ftp, "/data/GoldHEN/plugins.ini").decode("utf-8", "replace")
    if "--remove" in flags:
        new = remove_line(ini)
        if new != ini and not dry: write_ini(ftp, ini, new)
        print("plugins.ini: bl2_editor.prx removed" if new != ini else "plugins.ini didn't load bl2_editor.prx")
        return ftp.quit()
    if not dry:
        for local, remote in FILES:
            if remote.endswith("bl2_codes.txt") and "--no-codes" in flags: continue
            put(ftp, local, remote)
    new = add_line(ini)
    if dry:
        sys.stdout.writelines(difflib.unified_diff(ini.splitlines(True), new.splitlines(True), "plugins.ini", "plugins.ini (new)"))
    elif new != ini:
        write_ini(ftp, ini, new)
        print(f"plugins.ini: bl2_editor.prx added under [{TITLE}]")
    else:
        print(f"plugins.ini already loads bl2_editor.prx under [{TITLE}]")
    if not order_ok(new): print(f"warning: bl2_editor.prx is listed before XeMo.prx under [{TITLE}] - the L3+R3 menu won't open")
    ftp.quit()
    if not dry: print("restart Borderlands 2 to load the new build")

if __name__ == "__main__":
    main()
