import base64, re, sys, zlib

def decode(code):
    m = re.search(r'BL2\(([^)]*)\)', code, re.I)
    s = (m.group(1) if m else code).strip().replace('-', '+').replace('_', '/')
    s += '=' * (-len(s) % 4)
    return bytearray(base64.b64decode(s))

def check(code):
    try:
        d = decode(code)
    except Exception as e:
        return False, 'base64'
    if len(d) < 7 or len(d) > 40: return False, f'len {len(d)}'
    if d[0] & 0x7F != 7: return False, f'version {d[0] & 0x7F}'
    seed = int.from_bytes(d[1:5], 'big')
    body = d[5:]
    if seed:
        n = len(body)
        right = (seed % 32) % n
        left = n - right
        x = seed >> 5
        for i in range(n):
            x = (x * 0x10A860C1) % 0xFFFFFFFB
            body[i] ^= x & 0xFF
        body = body[left:] + body[:left]
    want = int.from_bytes(body[0:2], 'big')
    buf = bytearray(d[:5]) + bytearray(b'\xff\xff') + body[2:]
    buf += b'\xff' * (40 - len(buf))
    c = zlib.crc32(bytes(buf)) & 0xFFFFFFFF
    got = (c >> 16) ^ (c & 0xFFFF)
    return got == want, ('weapon' if d[0] & 0x80 else 'item')

if __name__ == '__main__':
    for line in open(sys.argv[1], encoding='utf-8'):
        if 'BL2(' not in line.upper() or line.startswith('#'): continue
        ok, info = check(line)
        print('OK ' if ok else 'BAD', info, line.strip()[:70])
