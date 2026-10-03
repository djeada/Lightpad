"""Build the oracle case headers from harness.py results (all_*.json).

    python3 mkheader.py        # tests/unit/vim_oracle_cases.h (original suites)
    python3 mkheader.py wb     # tests/unit/vim_oracle_cases_wb.h (second wave)

Only cases where our result already equals Vim's are written, so the unit test
asserts the behavior Vim 9.1 produced.
"""

import json, os, random, sys
random.seed(7)
wave = len(sys.argv) > 1 and sys.argv[1] == 'wb'
texts = []
def tid(t):
    if t not in texts: texts.append(t)
    return texts.index(t)
selected = []
suites = [('wb', 3)] if wave else [('grid', 1), ('cmds', 2), ('more', 2), ('gn', 3), ('misc', 2), ('audit', 10)]
for src, per in suites:
    data = json.load(open(f'all_{src}.json'))
    random.shuffle(data)
    seen = {}
    for x in data:
        c, e, a = x['case'], x['vim'], x['ours']
        k = c['keys']
        if seen.get(k, 0) >= per: continue
        ok = e['text'] == a['text'] and (e['line'], e['col']) == (a['line'], a['col'])
        regok = e['reg'] == a['reg']
        if not ok: continue
        seen[k] = seen.get(k, 0) + 1
        selected.append((tid(c['text']), c['line'], c['col'], k, e['text'], e['line'], e['col'], e['reg'] if regok and c.get('checkreg', True) else None))
def cstr(s):
    out = '"'
    for ch in s:
        if ch == '\\': out += '\\\\'
        elif ch == '"': out += '\\"'
        elif ch == '\n': out += '\\n'
        elif ch == '\t': out += '\\t'
        elif ord(ch) < 32: out += '\\x%02x" "' % ord(ch)
        else: out += ch
    return out + '"'
suffix = 'Wb' if wave else ''
guard = 'VIM_ORACLE_CASES_WB_H' if wave else 'VIM_ORACLE_CASES_H'
lines = ['#ifndef ' + guard, '#define ' + guard, '']
if not wave:
    lines += ['struct VimOracleCase {', '  int text;', '  int line;', '  int col;', '  const char *keys;', '  const char *expectedText;', '  int expectedLine;', '  int expectedCol;', '  const char *expectedRegister;', '};', '']
lines.append('static const char *const kVimOracle%sTexts[] = {' % suffix)
for t in texts: lines.append('    ' + cstr(t) + ',')
lines.append('};')
lines.append('')
lines.append('static const VimOracleCase kVimOracle%sCases[] = {' % suffix)
for (t, l, c, k, et, el, ec, reg) in selected:
    lines.append('    {%d, %d, %d, %s, %s, %d, %d, %s},' % (t, l, c, cstr(k), cstr(et), el, ec, cstr(reg) if reg is not None else 'nullptr'))
lines.append('};')
lines.append('')
lines.append('#endif')
name = 'vim_oracle_cases_wb.h' if wave else 'vim_oracle_cases.h'
open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tests', 'unit', name), 'w').write('\n'.join(lines) + '\n')
print(len(selected), len(texts))
