"""Convert Borderlands 2 saves between PS4 (Handsome Collection, decrypted with Apollo) and PC.

    python bl2_save_convert.py ps4topc Save0001.sav  Save0001_pc.sav
    python bl2_save_convert.py pctops4 Save0001_pc.sav Save0001.sav
    python bl2_save_convert.py info    <file>

Both platforms wrap the same inner block (big-endian size, "WSG", little-endian version/CRC/size,
Huffman-coded protobuf) the same way:  SHA1(rest) [20] + uncompressed size (BE u32) + compressed data.
The only difference is the compressor: PS4 uses zlib, PC uses LZO1X (what Gibbed's editor reads).
The LZO writer emits one literal run plus the end marker - a valid LZO1X stream any decompressor reads.
"""
import hashlib, struct, sys, zlib

def lzo1x_decompress(src):
    out = bytearray()
    ip = 0

    def copy_match(dist_back, n):
        nonlocal out
        pos = len(out) - dist_back
        if pos < 0:
            raise ValueError("LZO: match before start of output")
        for _ in range(n):
            out.append(out[pos]); pos += 1

    state = "start"
    t = 0
    while True:
        if state == "start":
            t = src[ip]
            if t > 17:
                ip += 1; t -= 17
                if t < 4:
                    state = "match_next"; continue
                out += src[ip:ip + t]; ip += t
                state = "first_literal_run"; continue
            state = "loop"; continue
        if state == "loop":
            t = src[ip]; ip += 1
            if t >= 16:
                state = "match"; continue
            if t == 0:
                while src[ip] == 0:
                    t += 255; ip += 1
                t += 15 + src[ip]; ip += 1
            out += src[ip:ip + t + 3]; ip += t + 3
            state = "first_literal_run"; continue
        if state == "first_literal_run":
            t = src[ip]; ip += 1
            if t >= 16:
                state = "match"; continue
            dist = 1 + 0x800 + (t >> 2) + (src[ip] << 2); ip += 1
            copy_match(dist, 3)
            state = "match_done"; continue
        if state == "match":
            if t >= 64:
                dist = 1 + ((t >> 2) & 7) + (src[ip] << 3); ip += 1
                copy_match(dist, (t >> 5) - 1 + 2)
            elif t >= 32:
                t &= 31
                if t == 0:
                    while src[ip] == 0:
                        t += 255; ip += 1
                    t += 31 + src[ip]; ip += 1
                dist = 1 + (src[ip] >> 2) + (src[ip + 1] << 6); ip += 2
                copy_match(dist, t + 2)
            elif t >= 16:
                dist = (t & 8) << 11
                t &= 7
                if t == 0:
                    while src[ip] == 0:
                        t += 255; ip += 1
                    t += 7 + src[ip]; ip += 1
                dist += (src[ip] >> 2) + (src[ip + 1] << 6); ip += 2
                if dist == 0:
                    return bytes(out)
                copy_match(dist + 0x4000, t + 2)
            else:
                dist = 1 + (t >> 2) + (src[ip] << 2); ip += 1
                copy_match(dist, 2)
            state = "match_done"; continue
        if state == "match_done":
            t = src[ip - 2] & 3
            if t == 0:
                state = "loop"; continue
            state = "match_next"; continue
        if state == "match_next":
            out += src[ip:ip + t]; ip += t
            t = src[ip]; ip += 1
            state = "match"; continue

def lzo1x_store(data):
    """LZO1X stream holding `data` as a single literal run, then the end marker."""
    n = len(data)
    if n == 0:
        return b"\x11\x00\x00"
    if n <= 238:
        head = bytes([n + 17])
    elif n < 18 + 1:
        raise ValueError("unreachable")
    else:
        rest = n - 18
        zeros, last = divmod(rest, 255)
        if last == 0:
            zeros -= 1; last = 255
        head = b"\x00" + b"\x00" * zeros + bytes([last])
    return head + data + b"\x11\x00\x00"

def unwrap(blob, platform):
    if hashlib.sha1(blob[20:]).digest() != blob[:20]:
        raise ValueError("SHA1 mismatch - not a decrypted BL2 save, or it is damaged")
    size = struct.unpack(">I", blob[20:24])[0]
    inner = zlib.decompress(blob[24:]) if platform == "ps4" else lzo1x_decompress(blob[24:])
    if len(inner) != size or inner[4:7] != b"WSG":
        raise ValueError("unexpected inner block (size %d/%d, magic %r)" % (len(inner), size, inner[4:7]))
    return inner

def wrap(inner, platform):
    comp = zlib.compress(inner, 9) if platform == "ps4" else lzo1x_store(inner)
    body = struct.pack(">I", len(inner)) + comp
    return hashlib.sha1(body).digest() + body

def detect(blob):
    return "ps4" if blob[24:25] == b"\x78" else "pc"

def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    cmd, src = sys.argv[1], sys.argv[2]
    blob = open(src, "rb").read()
    if cmd == "info":
        plat = detect(blob)
        inner = unwrap(blob, plat)
        ver, crc, usize = struct.unpack("<iIi", inner[7:19])
        print(f"{src}: {plat.upper()} save, inner {len(inner)} bytes, WSG v{ver}, crc {crc:08x}, payload {usize} bytes")
        return
    frm, to = {"ps4topc": ("ps4", "pc"), "pctops4": ("pc", "ps4")}[cmd]
    inner = unwrap(blob, frm)
    out = wrap(inner, to)
    if unwrap(out, to) != inner:
        raise SystemExit("round-trip check failed - nothing written")
    open(sys.argv[3], "wb").write(out)
    print(f"wrote {sys.argv[3]} ({to.upper()}, {len(out)} bytes, inner block unchanged: {len(inner)} bytes)")

if __name__ == "__main__":
    main()
