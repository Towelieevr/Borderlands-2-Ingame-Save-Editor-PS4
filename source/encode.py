import sys

s = " ".join(sys.argv[1:])
e = [ord(c) ^ ((0x5A + i * 0x1F) & 0xFF) for i, c in enumerate(s)]
print(f"ob((char[{len(s) + 1}]){{0}}, (const unsigned char[]){{{','.join('0x%02x' % b for b in e)}}}, {len(s)})")
