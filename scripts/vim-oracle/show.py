"""Print failures from fails.json, optionally filtered by a regex on the keys.

The regex may be given directly or as @file (the regex is read from the file).
"""
import json
import re
import sys

fails = json.load(open('fails.json'))
pat = sys.argv[1] if len(sys.argv) > 1 else ''
if pat.startswith('@'):
    pat = open(pat[1:]).read().strip()
limit = int(sys.argv[2]) if len(sys.argv) > 2 else 400
shown = 0
for x in fails:
    c = x['case']
    if pat and not re.search(pat, c['keys']):
        continue
    shown += 1
    if shown > limit:
        break
    print(repr(c['keys']), x['diff'], c['line'], c['col'])
    for who in ('vim', 'ours'):
        print('  ', who, repr(x[who]['text']), x[who]['line'], x[who]['col'],
              repr(x[who]['reg']) if 'reg' in x['diff'] else '')
