"""Run key sequences through Vim and through Lightpad's VimMode and diff them.

Usage:
    cmake -S scripts/vim-oracle -B scripts/vim-oracle/build
    cmake --build scripts/vim-oracle/build
    cd scripts/vim-oracle && python3 cases.py
    python3 harness.py grid.json all_grid.json

Failures are written to fails.json.  With a second argument every result is
saved so mkheader.py can build tests/unit/vim_oracle_cases.h.
"""

import json, subprocess, sys, os, re, tempfile
from concurrent.futures import ThreadPoolExecutor
HERE = os.path.dirname(os.path.abspath(__file__))
SETTINGS = "set nocompatible sw=4 ts=4 et nojs nrformats=bin,hex noai backspace=indent,eol,start noic noscs ws hls is"
SPECIAL = {"esc","cr","bs","del","tab","s-tab","left","right","up","down","home","end","lt","space","insert","pageup","pagedown"}
def vimstr(keys):
    out = ""
    i = 0
    while i < len(keys):
        c = keys[i]
        if c == '<':
            m = re.match(r'<([A-Za-z0-9-]+)>', keys[i:])
            if m and (m.group(1).lower() in SPECIAL or re.match(r'^[CcAa]-.$', m.group(1)) or re.match(r'^([CcAaSs]-)+[A-Za-z][A-Za-z0-9]+$', m.group(1))):
                out += '\\<' + m.group(1) + '>'
                i += len(m.group(0)); continue
        if c == ' ': out += '\\<Space>'
        elif c in '\\"': out += '\\' + c
        else: out += c
        i += 1
    return '"' + out + '"'
def vlist(s):
    return '[' + ','.join("'" + l.replace("'", "''") + "'" for l in s) + ']'
def run_vim_chunk(chunk, idx):
    d = tempfile.mkdtemp()
    script = [SETTINGS, "let g:out = []"]
    for c in chunk:
        lines = c['text'].split('\n')
        script.append(SETTINGS)
        script.append("for r in split('abcdefghijklmnopqrstuvwxyz0123456789-', '\\zs') | call setreg(r, '') | endfor")
        script.append("silent! %d _")
        script.append("call setline(1, %s)" % vlist(lines))
        script.append("call setreg('\"', '', 'v')")
        script.append("call cursor(%d, %d)" % (c['line']+1, c['col']+1))
        script.append("call feedkeys(%s, 'xtn')" % vimstr(c['keys']))
        script.append("call add(g:out, json_encode({'text': join(getline(1,'$'), \"\\n\"), 'line': line('.')-1, 'col': col('.')-1, 'reg': getreg('\"'), 'regtype': getregtype('\"')[0]}))")
    script.append("call writefile(g:out, '%s/out.jsonl')" % d)
    script.append("qa!")
    sp = os.path.join(d, 's.vim')
    open(sp, 'w').write('\n'.join(script) + '\n')
    subprocess.run(['vim','-u','NONE','-i','NONE','-N','-n','-Es','-S',sp], timeout=600, cwd=d)
    res = [json.loads(l) for l in open(os.path.join(d,'out.jsonl'))]
    return res
def main():
    cases = json.load(open(sys.argv[1]))
    n = 150
    chunks = [cases[i:i+n] for i in range(0, len(cases), n)]
    with ThreadPoolExecutor(8) as ex:
        results = list(ex.map(run_vim_chunk, chunks, range(len(chunks))))
    expected = [r for ch in results for r in ch]
    json.dump(cases, open(os.path.join(HERE,'cases_in.json'),'w'))
    subprocess.run([os.path.join(HERE,'build','driver'), os.path.join(HERE,'cases_in.json'), os.path.join(HERE,'actual.json')], env=dict(os.environ, QT_QPA_PLATFORM='offscreen'), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    actual = json.load(open(os.path.join(HERE,'actual.json')))
    fails = []
    for c, e, a in zip(cases, expected, actual):
        if e['regtype'] == '\x16': e['regtype'] = 'b'
        diffs = []
        if e['text'] != a['text']: diffs.append('text')
        if (e['line'], e['col']) != (a['line'], a['col']): diffs.append('cursor')
        if c.get('checkreg', True) and e['reg'] != a['reg']: diffs.append('reg')
        if diffs: fails.append((c, e, a, diffs))
    print("cases", len(cases), "fails", len(fails))
    json.dump([{'case':c,'vim':e,'ours':a,'diff':d} for c,e,a,d in fails], open(os.path.join(HERE,'fails.json'),'w'), indent=1)
    out = sys.argv[2] if len(sys.argv) > 2 else None
    if out:
        json.dump([{'case':c,'vim':e,'ours':a} for c,e,a in zip(cases, expected, actual)], open(out,'w'))
main()
