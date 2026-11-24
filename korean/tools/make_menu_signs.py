#!/usr/bin/env python3
"""2편 한글: 주 메뉴 간판(BTNSHNGL.ICN: NEW GAME / LOAD GAME / HIGH SCORES / CREDITS / QUIT, 간판마다 4장) 한글 그림.
간판마다:
  1. 글자 찾기: 2·3번째 장(보통·눌림)에서 금색 점(빨강-파랑 > 70, 초록-파랑 > 40, 밝기 > 110), 작은 점(징·테두리 불빛)은 뺌, 1칸 넓힘
  2. 네 장 모두 그 자리를 주변 그림으로 메움(cv2 inpaint) → 각 장의 불빛(글자 둘레 밝음)은 그대로 남음
  3. 한글을 그 장의 원래 글자 색으로 그림: 원래 글자 점을 '가장자리에서 얼마나 안쪽인지'로 줄 세워, 새 글자도 같은 깊이에 같은 색.
     오른쪽 아래 1칸 그림자(바탕을 어둡게), 원래 글자와 같은 높이·같은 가운데
usage: make_menu_signs.py HEROES2.AGG GALMURI_DIST_DIR OUT.ICN [PREVIEW.png]
"""
import sys

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont

import h2icn

# 간판 번호 -> (한글 줄들, 글꼴 파일, 크기). 말은 1편 한글판과 같음. 글꼴은 갈무리(주인 18:18), 점 그대로 2배(원래 글자 높이 20~22점에 맞춤).
# 줄기는 3점(2배 + 오른쪽·아래 1점). 불러오기만 보통 꼴로는 판(글자 자리 64점)에 안 들어가 좁은 꼴(Condensed)
# 넷째 값: 원래 글자 줄처럼 기울임(종료 판은 휘어 있어 글자가 오른쪽으로 올라감)
SIGNS = {0: (['새', '게임'], 'Galmuri11.ttf', 12, False), 1: (['불러', '오기'], 'Galmuri9.ttf', 10, False),
         2: (['최고', '기록'], 'Galmuri11.ttf', 12, False), 3: (['제작진'], 'Galmuri9.ttf', 10, False),
         4: (['종료'], 'Galmuri11.ttf', 12, True)}
# 세로 가운데 = 원래 글자 점들의 평균 높이(종료 판의 꼬리 'ᴛ'처럼 튀어나온 획에 끌리지 않게)
SCALE = 2
LINE_GAP = 4


def lum(c):
    return 0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2]


def is_gold(c):
    return c[0] - c[2] > 70 and c[1] - c[2] > 40 and lum(c) > 110


def components(points):
    points = set(points)
    out = []
    while points:
        stack = [points.pop()]
        comp = []
        while stack:
            p = stack.pop()
            comp.append(p)
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    q = (p[0] + dx, p[1] + dy)
                    if q in points:
                        points.remove(q)
                        stack.append(q)
        out.append(comp)
    return out


def depth_map(mask):
    """Distance (in pixels, 8-neighbour) from outside the mask; 1 = edge pixel."""
    m = mask.astype(np.uint8)
    d = cv2.distanceTransform(np.pad(m, 1), cv2.DIST_C, 3)[1:-1, 1:-1]
    return d


def text_mask(fs, pal):
    w, h = fs[0]['w'], fs[0]['h']
    pts = set()
    for f in fs[1:3]:
        for p, v in f['px'].items():
            if p[0] < w and p[1] < h and is_gold(pal[v * 3:v * 3 + 3]):
                pts.add(p)
    keep = set()
    for comp in components(pts):
        if len(comp) >= 6:
            keep.update(comp)
    m = np.zeros((h, w), bool)
    for x, y in keep:
        m[y, x] = True
    return m


def glyph_mask(lines, font, scale, line_gap):
    """Bool image of the text lines, centred line by line."""
    rows = []
    for t in lines:
        l, tp, r, b = font.getbbox(t)
        im = Image.new('L', (r - l + 4, b - tp + 4), 0)
        ImageDraw.Draw(im).text((2 - l, 2 - tp), t, font=font, fill=255)
        a = np.array(im) > 127
        ys, xs = np.nonzero(a)
        a = a[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
        if scale > 1:
            a = np.kron(a, np.ones((scale, scale), bool))
        if scale > 1 or font.size > 12:
            # 2배 줄기(2점)를 오른쪽·아래로 1점 더해 3점 굵기 (원래 간판 글자 줄기 굵기)
            a = np.pad(a, ((0, 1), (0, 1)))
            a[:, 1:] |= a[:, :-1].copy()
            a[1:, :] |= a[:-1, :].copy()
        rows.append(a)
    W = max(r.shape[1] for r in rows)
    H = sum(r.shape[0] for r in rows) + line_gap * (len(rows) - 1)
    out = np.zeros((H, W), bool)
    y = 0
    for r in rows:
        x = (W - r.shape[1]) // 2
        out[y:y + r.shape[0], x:x + r.shape[1]] = r
        y += r.shape[0] + line_gap
    return out


class Quant:
    def __init__(self, pal):
        # 214..241 are the colour-cycling entries of the HoMM2 palette: never used for still pictures
        self.idx = [i for i in range(256) if not 214 <= i <= 241]
        self.cols = np.array([pal[i * 3:i * 3 + 3] for i in self.idx], float)

    def __call__(self, c):
        d = ((self.cols - np.array(c, float)) ** 2).sum(1)
        return self.idx[int(d.argmin())]


def build(agg_path, font_dir):
    a = h2icn.Agg(agg_path)
    pal = h2icn.palette(a)
    q = Quant(pal)
    frs = h2icn.frames(a, 'BTNSHNGL.ICN')
    out = []
    for s in range(5):
        fs = frs[s * 4:s * 4 + 4]
        w, h = fs[0]['w'], fs[0]['h']
        m = text_mask(fs, pal)
        ys, xs = np.nonzero(m)
        box = (xs.min(), ys.min(), xs.max(), ys.max())
        erase = cv2.dilate(m.astype(np.uint8), np.ones((3, 3), np.uint8))
        lines, fname, size, tilt = SIGNS[s]
        # 크기 12 = 점 그대로 2배, 그보다 크면 그 크기로 바로 그림(불러오기·제작진: 주인 19:42 "폰트 크기 조금만 조절" → 판에 꽉 차지 않게 약 1.5배)
        g = glyph_mask(lines, ImageFont.truetype(font_dir + '/' + fname, size), SCALE if size <= 12 else 1, LINE_GAP)
        if g.shape[1] > box[2] - box[0] + 5 or g.shape[0] > box[3] - box[1] + 9:
            print('넘침', s, g.shape, box)
        cx, cy = (box[0] + box[2] + 1) / 2, ys.mean() + 0.5
        gx, gy = int(round(cx - g.shape[1] / 2)), int(round(cy - g.shape[0] / 2))
        # 원래 글자 줄이 기울어 있으면(종료 판) 같은 기울기로: 열마다 글자 점 가운데 높이의 기울기
        cols = [(x, ys[xs == x].mean()) for x in range(box[0], box[2] + 1) if (xs == x).any()]
        slope = np.polyfit([c[0] for c in cols], [c[1] for c in cols], 1)[0] if len(cols) > 4 else 0
        shift = [0] * g.shape[1]
        if tilt:
            # 글자 모양이 계단처럼 깨지지 않게 글자(빈 열로 나뉜 덩어리)마다 통째로 올림
            used = g.any(0)
            x = 0
            while x < g.shape[1]:
                if not used[x]:
                    x += 1
                    continue
                x0 = x
                while x < g.shape[1] and used[x]:
                    x += 1
                d = int(round(slope * ((x0 + x) / 2 - g.shape[1] / 2)))
                for k in range(x0, x):
                    shift[k] = d
        gd = depth_map(g)
        for f in fs:
            fh, fw = f['h'], f['w']
            rgb = np.zeros((fh, fw, 3), np.uint8)
            inside = np.zeros((fh, fw), bool)
            for (x, y), v in f['px'].items():
                rgb[y, x] = pal[v * 3:v * 3 + 3]
                inside[y, x] = True
            em = np.zeros((fh, fw), np.uint8)
            em[:min(h, fh), :min(w, fw)] = erase[:fh, :fw]
            em &= inside
            filled = cv2.inpaint(rgb, em, 3, cv2.INPAINT_TELEA)
            # colours of the original letters by depth
            fm = np.zeros((fh, fw), bool)
            fm[:min(h, fh), :min(w, fw)] = m[:fh, :fw]
            od = depth_map(fm)
            bydepth = {}
            for y, x in zip(*np.nonzero(fm)):
                bydepth.setdefault(min(int(od[y, x]), 3), []).append(rgb[y, x].astype(float))
            # 같은 깊이의 원래 글자 점 색을 자리마다 하나씩 골라 씀 → 원래 글자처럼 얼룩진 금색
            pools = {}
            for d in range(1, 4):
                pool = bydepth.get(d) or bydepth.get(d - 1) or pools[d - 1]
                pool = sorted(pool, key=lum)
                pools[d] = pool[len(pool) * 3 // 10:] or pool  # 가장 어두운 30%(글자 가장자리의 바탕 섞인 점)는 뺌
            res = filled.astype(float)
            # shadow down-right
            for y, x in zip(*np.nonzero(g)):
                X, Y = gx + x + 1, gy + y + 1 + shift[x]
                if 0 <= X < fw and 0 <= Y < fh and inside[Y, X]:
                    if not (y + 1 < g.shape[0] and x + 1 < g.shape[1] and g[y + 1, x + 1]):
                        res[Y, X] *= 0.45
            for y, x in zip(*np.nonzero(g)):
                X, Y = gx + x, gy + y + shift[x]
                if 0 <= X < fw and 0 <= Y < fh and inside[Y, X]:
                    pool = pools[min(int(gd[y, x]), 3)]
                    res[Y, X] = pool[(X * 7919 + Y * 104729) % len(pool)]
            px = {}
            for (x, y) in f['px']:
                px[(x, y)] = q(res[y, x])
            out.append(dict(x=f['x'], y=f['y'], w=fw, h=fh, typ=f['typ'], px=px, tr={}, changed=True))
    return frs, out, pal


def preview(frs, out, pal, path):
    tiles = []
    for group in (frs, out):
        for f in group:
            im = Image.new('RGB', (f['w'], f['h']), (48, 48, 48))
            for (x, y), v in f['px'].items():
                im.putpixel((x, y), tuple(pal[v * 3:v * 3 + 3]))
            tiles.append(im)
    W, H = 92, 117
    sheet = Image.new('RGB', (W * 8, H * 5), (48, 48, 48))
    for k in range(20):
        s, j = divmod(k, 4)
        sheet.paste(tiles[k], (j * W, s * H))
        sheet.paste(tiles[20 + k], ((4 + j) * W, s * H))
    sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST).save(path)


def main():
    agg, font_dir, out_icn = sys.argv[1], sys.argv[2], sys.argv[3]
    frs, out, pal = build(agg, font_dir)
    h2icn.write_icn(out_icn, out)
    if len(sys.argv) > 4:
        preview(frs, out, pal, sys.argv[4])


if __name__ == '__main__':
    main()
