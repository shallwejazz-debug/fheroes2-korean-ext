"""HoMM2 .AGG / .ICN helpers for the Korean patch: frames as palette indices (fheroes2 decodeICNSprite format).
A frame = dict(x, y, w, h, typ, px={(x, y): index}, tr={(x, y): transform value}) ."""
import struct


class Agg:
    def __init__(self, path):
        d = open(path, 'rb').read()
        n = struct.unpack_from('<H', d, 0)[0]
        ents = [struct.unpack_from('<III', d, 2 + i * 12) for i in range(n)]
        names = [d[len(d) - 15 * n + i * 15:len(d) - 15 * n + i * 15 + 15].split(b'\0')[0].decode().upper() for i in range(n)]
        self.d = d
        self.e = {nm: (o, sz) for nm, (_, o, sz) in zip(names, ents)}

    def get(self, name):
        o, sz = self.e[name.upper()]
        return self.d[o:o + sz]


def palette(agg):
    return [min(255, v * 4) for v in agg.get('KB.PAL')]


def decode(data, w, h):
    px, tr = {}, {}
    p = x = y = 0
    while p < len(data):
        c = data[p]
        p += 1
        if c == 0:
            y += 1
            x = 0
        elif c < 0x80:
            for k in range(c):
                px[(x, y)] = data[p + k]
                x += 1
            p += c
        elif c == 0x80:
            break
        elif c < 0xc0:
            x += c - 0x80
        elif c == 0xc0:
            t = data[p]
            p += 1
            n = t % 4
            if n == 0:
                n = data[p]
                p += 1
            v = ((t & 0x3c) >> 2) + 2 if t & 0x40 else 1
            for _ in range(n):
                tr[(x, y)] = v
                x += 1
        elif c == 0xc1:
            n, v = data[p], data[p + 1]
            p += 2
            for _ in range(n):
                px[(x, y)] = v
                x += 1
        else:
            v = data[p]
            p += 1
            for _ in range(c - 0xc0):
                px[(x, y)] = v
                x += 1
    return px, tr


def frames(agg, name):
    b = agg.get(name)
    cnt = struct.unpack_from('<H', b, 0)[0]
    out = []
    for i in range(cnt):
        x, y, w, h, typ, off = struct.unpack_from('<hhHHBI', b, 6 + i * 13)
        end = struct.unpack_from('<I', b, 6 + (i + 1) * 13 + 9)[0] if i + 1 < cnt else struct.unpack_from('<I', b, 2)[0]
        raw = b[6 + off:6 + end]
        px, tr = decode(raw, w, h) if typ != 32 else ({}, {})
        out.append(dict(x=x, y=y, w=w, h=h, typ=typ, px=px, tr=tr, raw=raw))
    return out


def encode(f):
    """Inverse of decode for pixels and transform pixels (type 1 = full transparency is never used here)."""
    out = bytearray()
    w, h, px, tr = f['w'], f['h'], f['px'], f.get('tr', {})
    for y in range(h):
        x = 0
        # last used column of the row
        last = max([cx for (cx, cy) in list(px) + list(tr) if cy == y], default=-1)
        while x <= last:
            if (x, y) in px:
                run = []
                while x <= last and (x, y) in px and len(run) < 0x7f:
                    run.append(px[(x, y)])
                    x += 1
                out.append(len(run))
                out += bytes(run)
            elif (x, y) in tr:
                v = tr[(x, y)]
                n = 0
                while x <= last and tr.get((x, y)) == v and (x, y) not in px and n < 255:
                    x += 1
                    n += 1
                t = 0x40 | ((v - 2) << 2) if v >= 2 else 0
                if n < 4:
                    out += bytes([0xc0, t | n])
                else:
                    out += bytes([0xc0, t, n])
            else:
                n = 0
                while x <= last and (x, y) not in px and (x, y) not in tr and n < 0x3f:
                    x += 1
                    n += 1
                out.append(0x80 + n)
        out.append(0)
    out.append(0x80)
    return bytes(out)


def write_icn(path, frs):
    """Frames not marked 'changed' keep their original bytes ('raw'), so frame kinds this encoder does not know stay exact."""
    datas = [f['raw'] if f.get('raw') is not None and not f.get('changed') else encode(f) for f in frs]
    head = bytearray(struct.pack('<HI', len(frs), 0))
    off = 13 * len(frs)
    for f, dt in zip(frs, datas):
        head += struct.pack('<hhHHBI', f['x'], f['y'], f['w'], f['h'], f.get('typ', 0), off)
        off += len(dt)
    struct.pack_into('<HI', head, 0, len(frs), off)
    open(path, 'wb').write(bytes(head) + b''.join(datas))
