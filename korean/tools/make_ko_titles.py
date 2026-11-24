#!/usr/bin/env python3
"""2편 한글: 그림 속 제목 글(금색 몸 + 검은 테두리, 2편 제목 글꼴 모양)을 한글로 바꾼 ICN 파일을 만든다.
고칠 자리마다(SPEC): 상자 안에서 원래 글자 = 아주 밝은 점(몸·빛) + 아주 어두운 점(테두리) → 그 자리를 지움
  - 'rows' 지움: 줄마다 상자 밖 같은 줄의 가장 흔한 색으로 채움(가로로 고른 막대: 최고 기록 머리줄)
  - 'inpaint' 지움: 주변 그림으로 메움(cv2)
  - 'tile' 지움: 같은 줄의 바탕 네모(opt 'bg')에서 옮겨 붙임(붉은 띠처럼 가로 결이 있는 곳)
새 글자: 갈무리(주인 18:18) 꼴, 몸 색은 원래 글자 몸의 줄별 색(위→아래 같은 비율), 둘레 1점은 원래 테두리 색.
usage: make_ko_titles.py HEROES2.AGG GALMURI_DIST_DIR OUT_DIR [PREVIEW_DIR]
"""
import os
import sys
from collections import Counter

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont

import h2icn

# (ICN, 프레임, 상자 x0 y0 x1 y1 (지울 곳, 끝 포함), 지우는 법, 한글 줄들, 꼴, 크기, 배치)
# 배치: 'center' = 상자 가운데, 'left' = 상자 왼쪽 끝부터(줄 맞춤), 'vertical' = 글자를 세로로 쌓음
T = 'Galmuri11-Bold.ttf'
SPEC = [
    # 최고 기록 머리줄: 일반(지도), 캠페인(주군). 원래 낱말 자리 왼쪽에 맞춤 → 아래 칸(엔진이 그리는 이름·숫자)과 줄이 맞음
    ('HISCORE.ICN', 6, (36, 2, 122, 17), 'rows', ['플레이어'], T, 12, 'left', {}),
    ('HISCORE.ICN', 6, (198, 2, 264, 17), 'rows', ['지도'], T, 12, 'left', {}),
    ('HISCORE.ICN', 6, (329, 2, 390, 17), 'rows', ['날짜'], T, 12, 'left', {}),
    ('HISCORE.ICN', 6, (399, 2, 490, 17), 'rows', ['평가'], T, 12, 'left', {}),
    ('HISCORE.ICN', 7, (36, 2, 122, 17), 'rows', ['플레이어'], T, 12, 'left', {}),
    ('HISCORE.ICN', 7, (254, 2, 328, 17), 'rows', ['주군'], T, 12, 'left', {}),
    ('HISCORE.ICN', 7, (387, 2, 448, 17), 'rows', ['날짜'], T, 12, 'left', {}),
    # 최고 기록 제목
    ('HSBKG.ICN', 0, (99, 1, 540, 17), 'tile', ['히어로즈 오브 마이트 앤 매직 II의 전설적인 영웅들'], T, 12, 'center', {'bg': (20, 1, 92, 17)}),
    # 캠페인 화면 (선 = 롤랜드, 악 = 아치볼드): 첫 글자는 원래처럼 테 안에 크게
    ('CAMPBKGG.ICN', 0, (90, 37, 115, 60), 'inpaint', ['롤'], 'Galmuri11.ttf', 12, 'center', {'scale': 2, 'thick': False}),
    ('CAMPBKGG.ICN', 0, (119, 37, 345, 60), 'tile', ['랜드의 캠페인'], T, 12, 'left', {'bg': (350, 37, 384, 60)}),
    ('CAMPBKGE.ICN', 0, (75, 37, 102, 60), 'inpaint', ['아'], 'Galmuri11.ttf', 12, 'center', {'scale': 2, 'thick': False}),
    ('CAMPBKGE.ICN', 0, (106, 37, 362, 60), 'tile', ['치볼드의 캠페인'], T, 12, 'left', {'bg': (364, 37, 386, 60)}),
    ('CAMPBKGG.ICN', 0, (38, 87, 153, 104), 'inpaint', ['시나리오'], T, 12, 'center', {}),
    ('CAMPBKGE.ICN', 0, (38, 87, 153, 104), 'inpaint', ['시나리오'], T, 12, 'center', {}),
    ('CAMPBKGG.ICN', 0, (420, 30, 559, 48), 'tile', ['지난 날 수:'], T, 12, 'left', {'bg': (566, 31, 600, 47)}),
    ('CAMPBKGE.ICN', 0, (420, 30, 559, 48), 'tile', ['지난 날 수:'], T, 12, 'left', {'bg': (566, 31, 600, 47)}),
    ('CAMPBKGG.ICN', 0, (468, 73, 560, 90), 'inpaint', ['얻은 상'], T, 12, 'center', {}),
    ('CAMPBKGE.ICN', 0, (468, 73, 560, 90), 'inpaint', ['얻은 상'], T, 12, 'center', {}),
    ('CAMPBKGG.ICN', 0, (473, 181, 554, 197), 'inpaint', ['선택'], T, 12, 'center', {}),
    ('CAMPBKGE.ICN', 0, (473, 181, 554, 197), 'inpaint', ['선택'], T, 12, 'center', {}),
    # 왕국 개요 (왼쪽 세로 글)
    ('OVERBACK.ICN', 0, (3, 32, 22, 162), 'inpaint', ['왕', '국'], T, 12, 'center', {'gap': 10}),
    ('OVERBACK.ICN', 0, (3, 182, 22, 332), 'inpaint', ['개', '요'], T, 12, 'center', {'gap': 10}),
    # 퍼즐 보기
    ('VIEWPUZL.ICN', 0, (10, 8, 141, 33), 'inpaint', ['퍼즐 보기'], T, 12, 'center', {}),
    # 지도 보기 (주인 2026-11-20 08:49 사진에서 찾음, 노트 v278): 제목과 범례. 범례 글은 '=' 째로 바꾸고 기호 그림은 그대로
    ('VIEWWRLD.ICN', 0, (14, 20, 130, 36), 'patch', ['지도 보기'], T, 12, 'center', {'src': (14, 108)}),
    ('VIEWWRLD.ICN', 0, (22, 174, 63, 184), 'patch', ['=성'], 'Galmuri9.ttf', 10, 'left', {'src': (22, 105)}),
    ('VIEWWRLD.ICN', 0, (80, 174, 119, 184), 'patch', ['=광산'], 'Galmuri9.ttf', 10, 'left', {'src': (80, 105)}),
    ('VIEWWRLD.ICN', 0, (21, 193, 58, 203), 'patch', ['=영웅'], 'Galmuri9.ttf', 10, 'left', {'src': (21, 112)}),
    ('VIEWWRLD.ICN', 0, (80, 193, 136, 203), 'patch', ['=아티팩트'], 'Galmuri9.ttf', 10, 'left', {'src': (80, 112)}),
    ('VIEWWRLD.ICN', 0, (48, 212, 108, 222), 'patch', ['=자원'], 'Galmuri9.ttf', 10, 'left', {'src': (48, 112)}),
    # 악 종족 화면의 같은 창
    ('EVIWWRLD.ICN', 0, (14, 20, 130, 36), 'patch', ['지도 보기'], T, 12, 'center', {'src': (14, 108)}),
    ('EVIWWRLD.ICN', 0, (22, 174, 66, 184), 'patch', ['=성'], 'Galmuri9.ttf', 10, 'left', {'src': (22, 112)}),
    ('EVIWWRLD.ICN', 0, (80, 174, 119, 184), 'patch', ['=광산'], 'Galmuri9.ttf', 10, 'left', {'src': (80, 112)}),
    ('EVIWWRLD.ICN', 0, (21, 193, 58, 203), 'patch', ['=영웅'], 'Galmuri9.ttf', 10, 'left', {'src': (21, 112)}),
    ('EVIWWRLD.ICN', 0, (80, 193, 136, 203), 'patch', ['=아티팩트'], 'Galmuri9.ttf', 10, 'left', {'src': (80, 112)}),
    ('EVIWWRLD.ICN', 0, (48, 212, 108, 222), 'patch', ['=자원'], 'Galmuri9.ttf', 10, 'left', {'src': (48, 112)}),
    # 축하합니다 (승리 화면 깃발)
    # (v268: 'inpaint'는 글자 그림자가 남아 'vfit'. v269: 주인 15:21 "좀 이상하다" → 원래처럼 작은 글자를 띠에 고루 벌려 굽은 띠를 따라 놓고 그림자, 띠는 글자 자리만 다시 칠함)
    ('CONGRATS.ICN', 0, (203, 55, 446, 81), 'vfit', ['축하합니다'], T, 22, 'center', {'erase': (196, 50, 454, 90), 'arc': (252, 398), 'depth': 12}),
]


def lum(c):
    return 0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2]


class Quant:
    def __init__(self, pal):
        self.idx = [i for i in range(256) if not 214 <= i <= 241]
        self.cols = np.array([pal[i * 3:i * 3 + 3] for i in self.idx], float)

    def __call__(self, c):
        d = ((self.cols - np.array(c, float)) ** 2).sum(1)
        return self.idx[int(d.argmin())]


def glyphs(lines, font, scale=1, thick=True):
    rows = []
    for t in lines:
        l, tp, r, b = font.getbbox(t)
        im = Image.new('L', (r - l + 4, b - tp + 4), 0)
        ImageDraw.Draw(im).text((2 - l, 2 - tp), t, font=font, fill=255)
        a = np.array(im) > 127
        ys, xs = np.nonzero(a)
        a = a[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
        if scale > 1:
            # 간판과 같게: 점 그대로 키운 뒤 오른쪽·아래 1점 더해 줄기 굵기 맞춤
            a = np.kron(a, np.ones((scale, scale), bool))
            if thick:
                a = np.pad(a, ((0, 1), (0, 1)))
                a[:, 1:] |= a[:, :-1].copy()
                a[1:, :] |= a[:-1, :].copy()
        rows.append(a)
    return rows


def letter_mask(f, pal, box):
    """원래 글자(밝은 몸 + 어두운 테두리) 점."""
    x0, y0, x1, y1 = box
    body, edge = set(), set()
    for (x, y), v in f['px'].items():
        if x0 <= x <= x1 and y0 <= y <= y1:
            c = pal[v * 3:v * 3 + 3]
            if lum(c) >= 125:
                body.add((x, y))
    for (x, y), v in f['px'].items():
        if x0 <= x <= x1 and y0 <= y <= y1 and lum(pal[v * 3:v * 3 + 3]) < 30:
            if any((x + dx, y + dy) in body for dx in (-2, -1, 0, 1, 2) for dy in (-2, -1, 0, 1, 2)):
                edge.add((x, y))
    return body, edge


def apply(frame, pal, q, fonts, items):
    f = frame
    w, h = f['w'], f['h']
    rgb = np.zeros((h, w, 3), np.uint8)
    inside = np.zeros((h, w), bool)
    for (x, y), v in f['px'].items():
        if x < w and y < h:
            rgb[y, x] = pal[v * 3:v * 3 + 3]
            inside[y, x] = True
    px = dict(f['px'])
    for (_, _, box, how, lines, fname, size, place, opt) in items:
        body, edge = letter_mask(f, pal, box)
        # 원래 몸 색 (줄별, 위→아래) 과 테두리 색
        ys = sorted({y for _, y in body})
        top, bot = ys[0], ys[-1]
        byrow = {}
        for (x, y) in body:
            byrow.setdefault(y, []).append(f['px'][(x, y)])
        grad = [Counter(byrow[y]).most_common(1)[0][0] if y in byrow else None for y in range(top, bot + 1)]
        for i in range(len(grad)):
            if grad[i] is None:
                grad[i] = grad[i - 1]
        outline = Counter(f['px'][p] for p in edge).most_common(1)[0][0] if edge else 0
        x0, y0, x1, y1 = box
        if how == 'vfit':
            # 굽은 띠 위의 글자와 그 그림자(띠 색과 비슷한 어두운 보라라서 색으로는 못 가림): 글자 점을 넓혀(그림자 3점까지) 지울 곳으로 삼고,
            # 칸마다 그 위·아래의 깨끗한 띠 점(붉은 테두리·하늘 빼고)으로 세로 밝기 선을 맞춰 칠함, 원래 띠처럼 이웃 점끼리 조금 밝기 차이
            # 지울 곳은 opt 'erase' 상자(글자 끝·그림자까지)에서 금색 글자 점(하늘의 밝은 파랑은 빼고)을 넓힌 것, 하늘 점은 칠하지 않음
            ex0, ey0, ex1, ey1 = opt.get('erase', box)
            mk = np.zeros((h, w), np.uint8)
            for (x, y), v in f['px'].items():
                c = pal[v * 3:v * 3 + 3]
                if ex0 <= x <= ex1 and ey0 <= y <= ey1 and lum(c) >= 125 and c[0] > c[2] + 30:
                    mk[y, x] = 1
            mk = cv2.dilate(mk, np.ones((9, 9), np.uint8))
            box_m = np.zeros((h, w), np.uint8)
            box_m[ey0:ey1 + 1, ex0:ex1 + 1] = 1
            mk = mk & box_m & inside
            for (x, y), v in f['px'].items():
                c = pal[v * 3:v * 3 + 3]
                if y < h and x < w and mk[y, x] and ( lum(c) > 150 and c[2] > c[0] ):
                    mk[y, x] = 0  # 하늘
            # 글자 그림자는 오른쪽 아래로 몇 점 → 지울 곳을 그쪽으로 더 넓힘
            sh = np.zeros_like(mk)
            sh[5:, 4:] = mk[:-5, :-4]
            mk = ( mk | sh ) & box_m & inside
            for (x, y), v in f['px'].items():
                c = pal[v * 3:v * 3 + 3]
                if y < h and x < w and mk[y, x] and ( ( lum(c) > 150 and c[2] > c[0] ) or ( c[0] > 140 and c[1] < 100 and c[2] < 100 ) ):
                    mk[y, x] = 0  # 하늘, 붉은 테두리
            # 띠 전체의 밝기 모양(띠 안 깊이 d = 위 테두리에서 아래 테두리까지 0..1, 가로 위치 x)을 글자 없는 띠 점들로 구해,
            # 지울 상자의 가로 범위 안 띠 안쪽을 통째로 그 모양으로 다시 칠함(글자·그림자 흔적이 남지 않게), 점 무늬는 이웃 점 밝기 차이로
            def is_red(x, y):
                v = f['px'].get((x, y))
                if v is None:
                    return False
                c = pal[v * 3:v * 3 + 3]
                return c[0] > 140 and c[1] < 100 and c[2] < 100
            borders = {}
            for x in range(0, w):
                reds = [y for y in range(max(0, ey0 - 30), min(h, ey1 + 30)) if is_red(x, y)]
                if not reds:
                    continue
                topb = reds[0]
                while topb + 1 in reds:
                    topb += 1
                botb = next((y for y in reds if y > topb + 10), None)
                if botb is not None:
                    borders[x] = (topb, botb)
            NB, XB = 16, 12
            acc = np.zeros((XB, NB, 3))
            cnt = np.zeros((XB, NB))
            xs_ = sorted(borders)
            xmin, xmax = xs_[0], xs_[-1]
            for x, (tb, bb) in borders.items():
                for y in range(tb + 1, bb):
                    if mk[y, x]:
                        continue  # 글자·그림자 자리는 빼고
                    v = f['px'].get((x, y))
                    if v is None:
                        continue
                    c = pal[v * 3:v * 3 + 3]
                    if lum(c) > 150 or c[0] > c[2] + 30:
                        continue
                    d = ( y - tb ) / ( bb - tb )
                    i = min(XB - 1, int(( x - xmin ) * XB / ( xmax - xmin + 1 )))
                    j = min(NB - 1, int(d * NB))
                    acc[i, j] += c
                    cnt[i, j] += 1
            # 빈 칸은 같은 깊이의 가까운 가로 칸 값으로
            model = np.zeros((XB, NB, 3))
            for j in range(NB):
                for i in range(XB):
                    if cnt[i, j] > 0:
                        model[i, j] = acc[i, j] / cnt[i, j]
                    else:
                        near = [k for k in range(XB) if cnt[k, j] > 0]
                        if near:
                            k = min(near, key=lambda k: abs(k - i))
                            model[i, j] = acc[k, j] / cnt[k, j]
            for x in range(ex0, ex1 + 1):
                if x not in borders:
                    continue
                tb, bb = borders[x]
                for y in range(tb + 1, bb):
                    if not mk[y, x] or (x, y) not in px:
                        continue
                    d = ( y - tb ) / ( bb - tb )
                    fx = ( x - xmin ) * XB / ( xmax - xmin + 1 ) - 0.5
                    fy = d * NB - 0.5
                    i0 = int(np.clip(np.floor(fx), 0, XB - 1)); i1 = min(XB - 1, i0 + 1); ax = np.clip(fx - i0, 0, 1)
                    j0 = int(np.clip(np.floor(fy), 0, NB - 1)); j1 = min(NB - 1, j0 + 1); ay = np.clip(fy - j0, 0, 1)
                    c = ( model[i0, j0] * ( 1 - ax ) + model[i1, j0] * ax ) * ( 1 - ay ) + ( model[i0, j1] * ( 1 - ax ) + model[i1, j1] * ax ) * ay
                    px[(x, y)] = q(np.clip(c * ( 1.04 if ( x + y ) % 2 else 0.96 ), 0, 255))
        elif how == 'rows':
            for y in range(y0, y1 + 1):
                fill = Counter(v for (x, yy), v in f['px'].items() if yy == y and not (x0 - 3 <= x <= x1 + 3)).most_common(1)[0][0]
                for x in range(x0, x1 + 1):
                    if (x, y) in px:
                        px[(x, y)] = fill
        else:
            # 글자의 중간 밝기 점(몸의 어두운 금색)은 밝은 점에서 2점 안, 테두리는 그 밖 1점 → 둘 다 덮게 넓힘
            mb = np.zeros((h, w), np.uint8)
            me = np.zeros((h, w), np.uint8)
            for (x, y) in body:
                mb[y, x] = 1
            for (x, y) in edge:
                me[y, x] = 1
            m = cv2.dilate(mb, np.ones((5, 5), np.uint8)) | cv2.dilate(me, np.ones((3, 3), np.uint8))
            box_m = np.zeros((h, w), np.uint8)
            box_m[y0:y1 + 1, x0:x1 + 1] = 1
            # 바탕에 없는 색 = 글자(그림자 포함): 상자 왼쪽·오른쪽 바깥 2~10점(같은 줄들)의 색 목록에 없는 색이면 글자로 봄
            # (위아래는 띠의 테두리라 넣지 않음)
            # 'bg' 를 주면 그 네모(글자 없는 바탕)의 색 목록을 씀 (띠 왼쪽에 큰 첫 글자 테가 있는 캠페인 제목처럼 옆이 바탕이 아닐 때)
            ring = Counter()
            bgbox = opt.get('bg')
            for (x, y), v in f['px'].items():
                if bgbox:
                    if bgbox[0] <= x <= bgbox[2] and bgbox[1] <= y <= bgbox[3]:
                        ring[v] += 1
                elif y0 <= y <= y1 and ( x0 - 10 <= x <= x0 - 2 or x1 + 2 <= x <= x1 + 10 ):
                    ring[v] += 1
            bg = {v for v, n in ring.items() if n >= 2}
            mf = np.zeros((h, w), np.uint8)
            for (x, y), v in f['px'].items():
                if x0 <= x <= x1 and y0 <= y <= y1 and v not in bg:
                    mf[y, x] = 1
            mf = cv2.morphologyEx(mf, cv2.MORPH_CLOSE, np.ones((3, 3), np.uint8))
            m = (m | cv2.dilate(mf, np.ones((3, 3), np.uint8))) & box_m & inside
            if how == 'patch':
                # 상자 전체를 글자 없는 같은 바탕 무늬 자리(opt 'src' = 왼쪽 위)에서 통째로 옮겨 붙임(잔 점 무늬 바탕: 메우기는 흐려짐, 지도 보기)
                sx, sy = opt['src']
                for y in range(y0, y1 + 1):
                    for x in range(x0, x1 + 1):
                        if (x, y) in f['px'] and (sx + x - x0, sy + y - y0) in f['px']:
                            px[(x, y)] = f['px'][(sx + x - x0, sy + y - y0)]
            elif how == 'tile' and bgbox:
                # 같은 줄의 바탕 네모에서 그대로 옮겨 붙임(가로로 결이 같은 띠: 무늬가 흐려지지 않음)
                bw = bgbox[2] - bgbox[0] + 1
                for y, x in zip(*np.nonzero(m)):
                    src = (bgbox[0] + (x - x0) % bw, y)
                    if src in f['px']:
                        px[(x, y)] = f['px'][src]
            else:
                filled = cv2.inpaint(rgb, m, 3, cv2.INPAINT_TELEA)
                for y, x in zip(*np.nonzero(m)):
                    px[(x, y)] = q(filled[y, x])
        # 새 글자
        if opt.get('arc'):
            # 굽은 띠 위 글자(축하합니다 깃발): 원래처럼 글자를 띠 너비에 고루 벌려 놓고, 글자마다 그 자리 위쪽 붉은 테두리에서 같은 깊이에 둠
            # (원래 글자 몸 = 테두리 아래 18~26점), 오른쪽 아래로 어두운 보라 그림자
            font = fonts(fname, size)
            chars = list(lines[0])
            sx0, sx1 = opt['arc']
            shadow = q(opt.get('shadow', (24, 0, 56)))
            for n, ch in enumerate(chars):
                r = glyphs([ch], font, 1, opt.get('thick', True))[0]
                gh, gw = r.shape
                cx = sx0 + ( sx1 - sx0 ) * n / max(1, len(chars) - 1)
                gx = int(round(cx - gw / 2))
                tb = None
                for yy in range(0, h):
                    v = f['px'].get((int(cx), yy))
                    if v is not None:
                        c = pal[v * 3:v * 3 + 3]
                        if c[0] > 140 and c[1] < 100 and c[2] < 100:
                            tb = yy
                            while True:
                                v2 = f['px'].get((int(cx), tb + 1))
                                c2 = pal[v2 * 3:v2 * 3 + 3] if v2 is not None else None
                                if c2 and c2[0] > 140 and c2[1] < 100 and c2[2] < 100:
                                    tb += 1
                                else:
                                    break
                            break
                gy = tb + opt.get('depth', 17) if tb is not None else y0
                ring = cv2.dilate(np.pad(r, 1).astype(np.uint8), np.ones((3, 3), np.uint8)).astype(bool)
                sh = cv2.dilate(np.pad(r, 2).astype(np.uint8), np.ones((3, 3), np.uint8)).astype(bool)
                for yy, xx in zip(*np.nonzero(sh)):
                    X, Y = gx + xx - 2 + 2, gy + yy - 2 + 3
                    if (X, Y) in px:
                        px[(X, Y)] = shadow
                for yy, xx in zip(*np.nonzero(ring)):
                    X, Y = gx + xx - 1, gy + yy - 1
                    if (X, Y) in px:
                        px[(X, Y)] = outline
                for yy, xx in zip(*np.nonzero(r)):
                    X, Y = gx + xx, gy + yy
                    if (X, Y) in px:
                        k = int(round(yy * (len(grad) - 1) / max(1, gh - 1)))
                        px[(X, Y)] = grad[k]
            continue
        font = fonts(fname, size)
        rows = glyphs(lines, font, opt.get('scale', 1), opt.get('thick', True))
        gap = opt.get('gap', 2)
        th = sum(r.shape[0] for r in rows) + gap * (len(rows) - 1)
        if len(lines) > 1:  # 여러 줄(세로 글): 상자 세로 가운데
            y = int(round((y0 + y1 + 1) / 2 - th / 2))
        else:  # 한 줄: 원래 글자 몸의 세로 가운데
            y = int(round((top + bot + 1) / 2 - th / 2))
        for r in rows:
            gh, gw = r.shape
            if place == 'left':
                gx = min(x for x, _ in body)
            else:
                gx = int(round((x0 + x1 + 1) / 2 - gw / 2))
            ring = cv2.dilate(np.pad(r, 1).astype(np.uint8), np.ones((3, 3), np.uint8)).astype(bool)
            for yy, xx in zip(*np.nonzero(ring)):
                X, Y = gx + xx - 1, y + yy - 1
                if (X, Y) in px:
                    px[(X, Y)] = outline
            for yy, xx in zip(*np.nonzero(r)):
                X, Y = gx + xx, y + yy
                if (X, Y) in px:
                    k = int(round(yy * (len(grad) - 1) / max(1, gh - 1)))
                    px[(X, Y)] = grad[k]
            y += gh + gap
    return dict(f, px=px, changed=True)


def main():
    agg = h2icn.Agg(sys.argv[1])
    font_dir, out_dir = sys.argv[2], sys.argv[3]
    prev = sys.argv[4] if len(sys.argv) > 4 else None
    pal = h2icn.palette(agg)
    q = Quant(pal)
    cache = {}

    def fonts(name, size):
        if (name, size) not in cache:
            cache[(name, size)] = ImageFont.truetype(os.path.join(font_dir, name), size)
        return cache[(name, size)]

    os.makedirs(out_dir, exist_ok=True)
    for icn in sorted({s[0] for s in SPEC}):
        frs = h2icn.frames(agg, icn)
        for i in sorted({s[1] for s in SPEC if s[0] == icn}):
            frs[i] = apply(frs[i], pal, q, fonts, [s for s in SPEC if s[0] == icn and s[1] == i])
            if prev:
                f = frs[i]
                im = Image.new('RGB', (f['w'], f['h']), (255, 0, 255))
                for (x, y), v in f['px'].items():
                    if x < f['w'] and y < f['h']:
                        im.putpixel((x, y), tuple(pal[v * 3:v * 3 + 3]))
                im.save(os.path.join(prev, '%s_%d.png' % (icn[:-4], i)))
        h2icn.write_icn(os.path.join(out_dir, icn), frs)
        print(icn)


if __name__ == '__main__':
    main()
