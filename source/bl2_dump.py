import os, socket, struct, sys

IP = sys.argv[1] if len(sys.argv) > 1 else "YOUR_PS4_IP"
BASE = 0x400000
GNAMES, GOBJECTS = 0x2482BF0, 0x2503100
O_OUTER, O_NAME, O_CLASS, P_OFFSET = 0x40, 0x48, 0x50, 0x7C
MAGIC, CMD_LIST, CMD_READ, OK = 0xFFAABBCC, 0xBDAA0001, 0xBDAA0002, 0x80000000
HERE = os.path.dirname(os.path.abspath(__file__))

class Dbg:
    def __init__(self):
        self.s = socket.create_connection((IP, 744), timeout=30)

    def recv(self, n):
        b = bytearray()
        while len(b) < n:
            p = self.s.recv(n - len(b))
            if not p: raise ConnectionError("closed")
            b += p
        return bytes(b)

    def cmd(self, c, data=b""):
        self.s.sendall(struct.pack("<III", MAGIC, c, len(data)) + data)
        st, = struct.unpack("<I", self.recv(4))
        if st != OK: raise RuntimeError(hex(st))

    def pid(self):
        self.cmd(CMD_LIST)
        n, = struct.unpack("<I", self.recv(4))
        for _ in range(n):
            r = self.recv(36)
            if b"MAGNOLIA" in r[:32]: p = struct.unpack("<i", r[32:])[0]
        return p

    def read(self, pid, addr, n):
        out = bytearray()
        while n:
            k = min(0x10000, n)
            self.cmd(CMD_READ, struct.pack("<IQI", pid, addr, k))
            out += self.recv(k); addr += k; n -= k
        return bytes(out)

def clustered(dbg, pid, ptrs, size, gap=0x4000):
    out, ps = {}, sorted(set(p for p in ptrs if p))
    i = 0
    while i < len(ps):
        j = i
        while j + 1 < len(ps) and ps[j + 1] - ps[j] < gap and ps[j + 1] + size - ps[i] < 0x40000: j += 1
        lo, hi = ps[i], ps[j] + size
        try:
            blk = dbg.read(pid, lo, hi - lo)
            for p in ps[i:j + 1]: out[p] = blk[p - lo:p - lo + size]
        except RuntimeError:
            for p in ps[i:j + 1]:
                try: out[p] = dbg.read(pid, p, size)
                except RuntimeError: pass
        i = j + 1
    return out

def tarray(dbg, pid, va):
    data, cnt, mx = struct.unpack("<Qii", dbg.read(pid, BASE + va, 16))
    raw = dbg.read(pid, data, cnt * 8)
    return list(struct.unpack(f"<{cnt}Q", raw))

def main():
    dbg = Dbg(); pid = dbg.pid()
    print("pid", pid)
    nptr = tarray(dbg, pid, GNAMES)
    ent = clustered(dbg, pid, nptr, 0x50)
    names = []
    for p in nptr:
        b = ent.get(p, b"")[0x10:]
        names.append(b.split(b"\0")[0].decode("latin-1") if b else "")
    with open(os.path.join(HERE, "bl2_names.txt"), "w") as f:
        for i, n in enumerate(names): f.write(f"{i}\t{n}\n")
    print("names", len(names))
    optr = tarray(dbg, pid, GOBJECTS)
    obj = clustered(dbg, pid, optr, 0x80)
    info = {}
    for p in optr:
        b = obj.get(p)
        if b: info[p] = (struct.unpack_from("<Q", b, O_OUTER)[0], struct.unpack_from("<i", b, O_NAME)[0],
                         struct.unpack_from("<Q", b, O_CLASS)[0], struct.unpack_from("<i", b, P_OFFSET)[0])
    nm = lambda i: names[i] if 0 <= i < len(names) else "?"
    oname = lambda p: nm(info[p][1]) if p in info else ("None" if not p else "?")
    with open(os.path.join(HERE, "bl2_objects.txt"), "w") as f:
        for idx, p in enumerate(optr):
            if p not in info: continue
            outer, n, cls, off = info[p]
            c = oname(cls)
            line = f"{idx}\t{c}\t{oname(outer)}.{nm(n)}"
            if c.endswith("Property"): line += f"\t0x{off:X}"
            f.write(line + "\n")
    print("objects", len(optr))

if __name__ == "__main__":
    main()
