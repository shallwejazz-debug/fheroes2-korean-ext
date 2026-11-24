#!/usr/bin/env python3
"""2편 한글: TTF 픽셀 글꼴로 HANGUL.BIN을 만든다 (주인 2026-11-18 18:01 "글자가 가늘고 얍삽해 보인다", "숫자들이 어색").
형식은 1편 make_hangul_bin.py와 같음: 'H1KO', u16 11172, u16 12, u16 10, 큰 글자 12줄(u16, 비트 x = 가로 x) × 11172, 작은 글자 10줄 × 11172.
큰 글자는 BIG_TTF를 12픽셀로, 작은 글자는 SMALL_TTF를 10픽셀로 그려 반 넘게 칠해진 점만 씀 (안티에일리어싱 없음).
갈무리(Galmuri, quiple, SIL OFL 1.1): Galmuri11은 12픽셀에서 11×11, Galmuri9는 10픽셀에서 9×9로 칸 안에 들어옴 [확인: 글자 상자 계산].
usage: make_hangul_bin_ttf.py BIG_TTF SMALL_TTF OUT"""
import struct
import sys

from PIL import Image, ImageDraw, ImageFont


def cell(font, ch, size):
    im = Image.new('L', (size + 4, size + 4), 0)
    ImageDraw.Draw(im).text((0, 0), ch, font=font, fill=255)
    px = im.load()
    return [sum(1 << x for x in range(size) if px[x, y] >= 128) for y in range(size)]


def main():
    big = ImageFont.truetype(sys.argv[1], 12)
    small = ImageFont.truetype(sys.argv[2], 10)
    out = bytearray(b'H1KO') + struct.pack('<HHH', 11172, 12, 10)
    for n in range(11172):
        for row in cell(big, chr(0xAC00 + n), 12):
            out += struct.pack('<H', row)
    for n in range(11172):
        for row in cell(small, chr(0xAC00 + n), 10):
            out += struct.pack('<H', row)
    open(sys.argv[3], 'wb').write(out)
    print(len(out))


if __name__ == '__main__':
    main()
