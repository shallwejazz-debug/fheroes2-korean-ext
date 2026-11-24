"""2편 한글: 번역 조각(part*.tsv)을 모아 게임이 읽는 h2/KOREAN_H2.TXT를 만든다.

part*.tsv 한 줄 = 영어 원문 <탭> 한국어 (줄바꿈은 \\n 으로). '#'으로 시작하는 줄은 메모.
원문은 fheroes2 소스에서 뽑은 fheroes2.pot의 msgid(복수형 msgid_plural 포함)와 글자 하나까지 같아야 한다.

usage: make_korean_h2.py CATALOG_DIR OUT_TXT
  - pot에 없는 원문(오타)과 같은 원문이 두 번 나온 것을 알려 주고, 번역된 비율을 디렉터리별로 보여 준다.
"""
import collections
import glob
import os
import sys


def unescape(s):
    out = []
    i = 0
    while i < len(s):
        if s[i] == '\\' and i + 1 < len(s):
            out.append({'n': '\n', 't': '\t', '\\': '\\'}.get(s[i + 1], '\\' + s[i + 1]))
            i += 2
        else:
            out.append(s[i])
            i += 1
    return ''.join(out)


def escape(s):
    return s.replace('\\', '\\\\').replace('\n', '\\n').replace('\t', '\\t')


def read_pot(path):
    import polib
    po = polib.pofile(path)
    ids = {}
    for e in po:
        if not e.msgid:
            continue
        src = e.occurrences[0][0] if e.occurrences else '?'
        group = src.split('/')[1] if src.startswith('fheroes2/') else src.split('/')[0]
        ids[e.msgid] = group
        if e.msgid_plural:
            ids[e.msgid_plural] = group
    return ids


def main():
    cat, out = sys.argv[1], sys.argv[2]
    ids = read_pot(os.path.join(cat, 'fheroes2.pot'))
    # 원작 지도·캠페인 안의 글 (tools/extract_map_text.py가 뽑은 목록): 엔진 문장과 따로 셈
    for line in open(os.path.join(cat, 'maptext_source.tsv'), encoding='utf-8'):
        en = unescape(line.rstrip('\n').split('\t')[0])
        ids.setdefault(en, '원작지도')
    table = {}
    unknown = []
    for path in sorted(glob.glob(os.path.join(cat, 'part*.tsv'))):
        for n, line in enumerate(open(path, encoding='utf-8'), 1):
            line = line.rstrip('\n').rstrip('\r')
            if not line or line.startswith('#') or '\t' not in line:
                continue
            en, ko = line.split('\t', 1)
            en = unescape(en)
            if en not in ids:
                unknown.append(f'{os.path.basename(path)}:{n}: {en[:60]}')
            if en in table and table[en] != unescape(ko):
                print(f'중복(다른 번역): {os.path.basename(path)}:{n}: {en[:60]}')
            table[en] = unescape(ko)
    for u in unknown:
        print('pot에 없음:', u)
    total = collections.Counter(ids.values())
    done = collections.Counter(g for en, g in ids.items() if en in table)
    for g in sorted(total, key=lambda g: -total[g]):
        print(f'{g:10s} {done[g]:5d} / {total[g]:5d}')
    print(f'전체      {sum(done.values()):5d} / {sum(total.values()):5d}')
    with open(out, 'w', encoding='utf-8') as o:
        o.write('# HoMM2 Korean table (h2korean/catalog/make_korean_h2.py)\n')
        for en, ko in table.items():
            o.write(escape(en) + '\t' + escape(ko) + '\n')


if __name__ == '__main__':
    main()
