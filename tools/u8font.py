"""Minimal u8g2 font decoder/encoder, used to patch broken glyphs in the project fonts."""
import re, ast

def parse_c_array(text, name):
    m = re.search(r'%s\s*\[\d*\]\s*(?:U8G2_FONT_SECTION\("[^"]*"\))?\s*=\s*((?:\s*"(?:[^"\\]|\\.)*")+)\s*;' % re.escape(name), text)
    if not m:
        raise KeyError(name)
    parts = re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1))
    return b''.join(ast.literal_eval('b"' + p + '"') for p in parts)


class BitReader:
    def __init__(self, d, pos):
        self.d, self.pos, self.bit = d, pos, 0

    def get(self, n):
        v = 0
        for i in range(n):
            v |= ((self.d[self.pos] >> self.bit) & 1) << i
            self.bit += 1
            if self.bit == 8:
                self.bit, self.pos = 0, self.pos + 1
        return v

    def sget(self, n):
        return self.get(n) - (1 << (n - 1))


class BitWriter:
    def __init__(self):
        self.out, self.cur, self.bit = bytearray(), 0, 0

    def put(self, v, n):
        for i in range(n):
            self.cur |= ((v >> i) & 1) << self.bit
            self.bit += 1
            if self.bit == 8:
                self.out.append(self.cur)
                self.cur, self.bit = 0, 0

    def sput(self, v, n):
        self.put(v + (1 << (n - 1)), n)

    def bytes(self):
        return bytes(self.out) + (bytes([self.cur]) if self.bit else b'')


class Glyph:
    def __init__(self, enc, w, h, x, y, dx, pixels):
        self.enc, self.w, self.h, self.x, self.y, self.dx, self.pixels = enc, w, h, x, y, dx, pixels

    def same_bitmap(self, other):
        return (self.w, self.h, self.pixels) == (other.w, other.h, other.pixels)

    def ascii(self):
        return '\n'.join(''.join('#' if self.pixels[r * self.w + c] else '.' for c in range(self.w)) for r in range(self.h))


class Font:
    def __init__(self, data):
        self.header = bytearray(data[:23])
        self.b0, self.b1, self.bw, self.bh, self.bx, self.by, self.bd = data[2:9]
        self.glyphs = {}
        pos = 23
        while data[pos + 1] != 0:
            enc, jump = data[pos], data[pos + 1]
            r = BitReader(data, pos + 2)
            w, h = r.get(self.bw), r.get(self.bh)
            x, y, dx = r.sget(self.bx), r.sget(self.by), r.sget(self.bd)
            px = []
            while len(px) < w * h:
                a, b = r.get(self.b0), r.get(self.b1)
                while True:
                    px += [0] * a + [1] * b
                    if r.get(1) == 0:
                        break
            self.glyphs[enc] = Glyph(enc, w, h, x, y, dx, px[:w * h])
            pos += jump
        self.unicode_tail = data[pos:]          # Glyph list terminator and the (empty) unicode table

    @property
    def ascent_A(self):
        return self.header[13] - 256 if self.header[13] > 127 else self.header[13]

    def encode(self):
        gl = [self.glyphs[k] for k in sorted(self.glyphs)]

        def bits_u(v):
            return max(1, v.bit_length())

        def bits_s(lo, hi):
            n = 1
            while not (-(1 << (n - 1)) <= lo and hi < (1 << (n - 1))):
                n += 1
            return n

        bw = max(bits_u(g.w) for g in gl)
        bh = max(bits_u(g.h) for g in gl)
        bx = bits_s(min(g.x for g in gl), max(g.x for g in gl))
        by = bits_s(min(g.y for g in gl), max(g.y for g in gl))
        bd = bits_s(min(g.dx for g in gl), max(g.dx for g in gl))
        b0, b1 = self.b0, self.b1
        max0, max1 = (1 << b0) - 1, (1 << b1) - 1

        body = bytearray()
        pos_A = pos_a = None
        for g in gl:
            # Runs of (zeros, ones), each within the RLE field sizes
            pairs, i, n = [], 0, len(g.pixels)
            while i < n:
                a = 0
                while i < n and g.pixels[i] == 0 and a < max0:
                    a, i = a + 1, i + 1
                b = 0
                if a == max0 and i < n and g.pixels[i] == 0:
                    pairs.append((a, 0))
                    continue
                while i < n and g.pixels[i] == 1 and b < max1:
                    b, i = b + 1, i + 1
                pairs.append((a, b))

            w = BitWriter()
            w.put(g.w, bw); w.put(g.h, bh)
            w.sput(g.x, bx); w.sput(g.y, by); w.sput(g.dx, bd)
            k = 0
            while k < len(pairs):
                w.put(pairs[k][0], b0); w.put(pairs[k][1], b1)
                while k + 1 < len(pairs) and pairs[k + 1] == pairs[k]:
                    w.put(1, 1)
                    k += 1
                w.put(0, 1)
                k += 1
            data = w.bytes()
            if g.enc >= ord('A') and pos_A is None:
                pos_A = len(body)
            if g.enc >= ord('a') and pos_a is None:
                pos_a = len(body)
            entry = bytes([g.enc, len(data) + 2]) + data
            assert len(entry) < 256
            body += entry

        hdr = bytearray(self.header)
        hdr[0] = len(gl)
        hdr[4:9] = bytes([bw, bh, bx, by, bd])
        hdr[9] = max(g.w for g in gl)
        hdr[10] = max(g.h for g in gl)
        hdr[17:19] = (pos_A or 0).to_bytes(2, 'big')
        hdr[19:21] = (pos_a or 0).to_bytes(2, 'big')
        hdr[21:23] = (len(body) + 2).to_bytes(2, 'big')   # unicode table follows the 0x00 0x00 terminator
        tail = self.unicode_tail
        return bytes(hdr) + bytes(body) + tail


def to_c(data, name):
    lines, cur = [], ''
    for i, c in enumerate(data):
        cur += '\\%03o' % c
        if len(cur) >= 96:
            lines.append(cur); cur = ''
    if cur:
        lines.append(cur)
    body = '\n'.join('  "%s"' % l for l in lines)
    return 'const uint8_t %s[%d] U8G2_FONT_SECTION("%s") = \n%s;\n' % (name, len(data) + 1, name, body)
