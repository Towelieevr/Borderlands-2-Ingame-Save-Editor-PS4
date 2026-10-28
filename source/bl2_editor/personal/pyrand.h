#pragma once
#include <stdint.h>

typedef struct { uint64_t h[8]; uint8_t buf[128]; uint64_t len; int fill; } Sha512;

static const uint64_t k_sha512[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL,
    0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
    0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL, 0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL,
    0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL, 0x983e5152ee66dfabULL,
    0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
    0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL,
    0x53380d139d95b3dfULL, 0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL,
    0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL, 0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
    0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL,
    0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL, 0xca273eceea26619cULL,
    0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
    0x113f9804bef90daeULL, 0x1b710b35131c471bULL, 0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
    0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL};

static uint64_t sha_ror(uint64_t x, int n) { return (x >> n) | (x << (64 - n)); }

static void sha512_block(Sha512* s, const uint8_t* p) {
    uint64_t w[80], a, b, c, d, e, f, g, h;
    for (int i = 0; i < 16; i++) {
        w[i] = 0;
        for (int j = 0; j < 8; j++) w[i] = (w[i] << 8) | p[i * 8 + j];
    }
    for (int i = 16; i < 80; i++) {
        uint64_t s0 = sha_ror(w[i - 15], 1) ^ sha_ror(w[i - 15], 8) ^ (w[i - 15] >> 7);
        uint64_t s1 = sha_ror(w[i - 2], 19) ^ sha_ror(w[i - 2], 61) ^ (w[i - 2] >> 6);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (int i = 0; i < 80; i++) {
        uint64_t t1 = h + (sha_ror(e, 14) ^ sha_ror(e, 18) ^ sha_ror(e, 41)) + ((e & f) ^ (~e & g)) + k_sha512[i] + w[i];
        uint64_t t2 = (sha_ror(a, 28) ^ sha_ror(a, 34) ^ sha_ror(a, 39)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void sha512(const uint8_t* data, int n, uint8_t out[64]) {
    Sha512 s = {{0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
                 0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL}};
    uint8_t blk[256];
    int full = n / 128;
    for (int i = 0; i < full; i++) sha512_block(&s, data + i * 128);
    int rem = n - full * 128, tot = rem + 17 <= 128 ? 128 : 256;
    for (int i = 0; i < tot; i++) blk[i] = 0;
    for (int i = 0; i < rem; i++) blk[i] = data[full * 128 + i];
    blk[rem] = 0x80;
    uint64_t bits = (uint64_t)n * 8;
    for (int i = 0; i < 8; i++) blk[tot - 1 - i] = (uint8_t)(bits >> (8 * i));
    for (int i = 0; i < tot; i += 128) sha512_block(&s, blk + i);
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++) out[i * 8 + j] = (uint8_t)(s.h[i] >> (56 - 8 * j));
}

typedef struct { uint32_t mt[624]; int index; } PyRand;

static void py_init_genrand(PyRand* r, uint32_t s) {
    r->mt[0] = s;
    for (int i = 1; i < 624; i++) r->mt[i] = 1812433253U * (r->mt[i - 1] ^ (r->mt[i - 1] >> 30)) + (uint32_t)i;
    r->index = 624;
}

static void py_init_by_array(PyRand* r, const uint32_t* key, int len) {
    py_init_genrand(r, 19650218U);
    int i = 1, j = 0;
    for (int k = 624 > len ? 624 : len; k; k--) {
        r->mt[i] = (r->mt[i] ^ ((r->mt[i - 1] ^ (r->mt[i - 1] >> 30)) * 1664525U)) + key[j] + (uint32_t)j;
        i++; j++;
        if (i >= 624) { r->mt[0] = r->mt[623]; i = 1; }
        if (j >= len) j = 0;
    }
    for (int k = 623; k; k--) {
        r->mt[i] = (r->mt[i] ^ ((r->mt[i - 1] ^ (r->mt[i - 1] >> 30)) * 1566083941U)) - (uint32_t)i;
        i++;
        if (i >= 624) { r->mt[0] = r->mt[623]; i = 1; }
    }
    r->mt[0] = 0x80000000U;
}

static uint32_t py_genrand(PyRand* r) {
    if (r->index >= 624) {
        for (int k = 0; k < 624; k++) {
            uint32_t y = (r->mt[k] & 0x80000000U) | (r->mt[(k + 1) % 624] & 0x7fffffffU);
            r->mt[k] = r->mt[(k + 397) % 624] ^ (y >> 1) ^ ((y & 1) ? 0x9908b0dfU : 0);
        }
        r->index = 0;
    }
    uint32_t y = r->mt[r->index++];
    y ^= y >> 11;
    y ^= (y << 7) & 0x9d2c5680U;
    y ^= (y << 15) & 0xefc60000U;
    y ^= y >> 18;
    return y;
}

static void py_seed_bytes(PyRand* r, const uint8_t* data, int n) {
    uint8_t all[128], dig[64];
    sha512(data, n, dig);
    int len = 0;
    for (int i = 0; i < n; i++) all[len++] = data[i];
    for (int i = 0; i < 64; i++) all[len++] = dig[i];
    int lead = 0;
    while (lead < len && all[lead] == 0) lead++;
    int bytes = len - lead;
    uint32_t key[32];
    int words = bytes ? (bytes + 3) / 4 : 1;
    for (int w = 0; w < words; w++) {
        uint32_t v = 0;
        for (int b = 3; b >= 0; b--) {
            int idx = len - 1 - (w * 4 + b);
            v = (v << 8) | (idx >= lead ? all[idx] : 0);
        }
        key[w] = v;
    }
    py_init_by_array(r, key, words);
}

static uint32_t py_getrandbits(PyRand* r, int k) { return k <= 0 ? 0 : py_genrand(r) >> (32 - k); }

static uint32_t py_randbelow(PyRand* r, uint32_t n) {
    if (!n) return 0;
    int k = 0;
    while (k < 32 && (n >> k)) k++;
    uint32_t v = py_getrandbits(r, k);
    while (v >= n) v = py_getrandbits(r, k);
    return v;
}

static void py_shuffle(PyRand* r, uint16_t* x, int n) {
    for (int i = n - 1; i >= 1; i--) {
        uint32_t j = py_randbelow(r, (uint32_t)i + 1);
        uint16_t t = x[i]; x[i] = x[j]; x[j] = t;
    }
}

static void py_sample(PyRand* r, const uint16_t* pop, int n, int k, uint16_t* out, uint16_t* pool, uint8_t* seen) {
    long setsize = 21;
    if (k > 5) {
        long p = 1;
        while (p < (long)k * 3) p *= 4;
        setsize += p;
    }
    if (n <= setsize) {
        for (int i = 0; i < n; i++) pool[i] = pop[i];
        for (int i = 0; i < k; i++) {
            uint32_t j = py_randbelow(r, (uint32_t)(n - i));
            out[i] = pool[j];
            pool[j] = pool[n - i - 1];
        }
    } else {
        for (int i = 0; i < n; i++) seen[i] = 0;
        for (int i = 0; i < k; i++) {
            uint32_t j = py_randbelow(r, (uint32_t)n);
            while (seen[j]) j = py_randbelow(r, (uint32_t)n);
            seen[j] = 1;
            out[i] = pop[j];
        }
    }
}

#define SEED_DUD 0xFFFE
#define SEED_NONE 0xFFFF
#define TAG_DUPLICATE (1ULL << 28)

static const char k_b32[] = "abcdefghijklmnopqrstuvwxyz234567";

static void seed_to_string(const uint8_t d[9], char out[18]) {
    char s[16];
    int n = 0;
    uint32_t acc = 0;
    int bits = 0;
    for (int i = 0; i < 9; i++) {
        acc = (acc << 8) | d[i]; bits += 8;
        while (bits >= 5) { s[n++] = k_b32[(acc >> (bits - 5)) & 31]; bits -= 5; }
    }
    if (bits) s[n++] = k_b32[(acc << (5 - bits)) & 31];
    int o = 0;
    for (int i = 0; i < 15; i++) { if (i == 5 || i == 10) out[o++] = '-'; out[o++] = s[i]; }
    out[o] = 0;
}

static int seed_from_string(const char* str, uint8_t d[9]) {
    uint8_t v[16];
    int n = 0;
    for (const char* p = str; *p && n < 16; p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c >= 'a' && c <= 'z') v[n++] = (uint8_t)(c - 'a');
        else if (c >= '2' && c <= '7') v[n++] = (uint8_t)(c - '2' + 26);
        else if (c >= '0' && c <= '9') return -1;
    }
    if (n != 15) return -1;
    uint64_t acc = 0;
    int bits = 0, o = 0;
    for (int i = 0; i < 15; i++) {
        acc = (acc << 5) | v[i]; bits += 5;
        if (bits >= 8) { d[o++] = (uint8_t)(acc >> (bits - 8)); bits -= 8; }
    }
    return o == 9 ? 0 : -1;
}

static void seed_make(uint32_t value30, uint64_t tags, int version, uint8_t d[9]) {
    uint64_t lo = ((tags & ((1ULL << 36) - 1)) << 6) | (uint64_t)(version & 63);
    uint64_t hi = (uint64_t)(value30 & ((1u << 30) - 1));
    unsigned __int128 v = ((unsigned __int128)hi << 42) | lo;
    for (int i = 8; i >= 0; i--) { d[i] = (uint8_t)v; v >>= 8; }
}

static void seed_parts(const uint8_t d[9], uint64_t* tags, int* version) {
    unsigned __int128 v = 0;
    for (int i = 0; i < 9; i++) v = (v << 8) | d[i];
    *version = (int)(v & 63);
    *tags = (uint64_t)((v >> 6) & ((1ULL << 36) - 1));
}

static int seed_assign(const uint8_t d[9], uint64_t tags, const uint64_t* item_tags, int nitems, const uint64_t* loc_tags,
                       int nlocs, uint16_t* loc_item, uint16_t* w_items, uint16_t* w_locs, uint16_t* w_tmp,
                       uint16_t* w_pool, uint8_t* w_seen, int* out_nloc, int* out_nitem) {
    int ic = 0, lc = 0;
    for (int i = 0; i < nitems; i++) if (item_tags[i] & tags) w_items[ic++] = (uint16_t)i;
    for (int i = 0; i < nlocs; i++) if ((loc_tags[i] & tags) == loc_tags[i]) w_locs[lc++] = (uint16_t)i;
    for (int i = 0; i < nlocs; i++) loc_item[i] = SEED_NONE;
    *out_nloc = lc; *out_nitem = ic;
    if (!ic) return -1;
    PyRand r;
    py_seed_bytes(&r, d, 9);
    if (lc < ic) {
        py_sample(&r, w_items, ic, lc, w_tmp, w_pool, w_seen);
        for (int i = 0; i < lc; i++) w_items[i] = w_tmp[i];
    } else if (lc == ic) {
        py_shuffle(&r, w_items, ic);
    } else if (tags & TAG_DUPLICATE) {
        int mult = lc / ic, extra = lc % ic, n = ic * mult;
        for (int m = 1; m < mult; m++)
            for (int i = 0; i < ic; i++) w_items[m * ic + i] = w_items[i];
        py_sample(&r, w_items, n, extra, w_tmp, w_pool, w_seen);
        for (int i = 0; i < extra; i++) w_items[n + i] = w_tmp[i];
        py_shuffle(&r, w_items, lc);
    } else {
        for (int i = ic; i < lc; i++) w_items[i] = SEED_DUD;
        py_shuffle(&r, w_items, lc);
    }
    for (int i = 0; i < lc; i++) loc_item[w_locs[i]] = w_items[i];
    return 0;
}
