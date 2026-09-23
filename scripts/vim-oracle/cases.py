"""Key sequences compared against Vim by harness.py.

Each generator returns dicts with the buffer text, the 0-based cursor line and
column, and the keys in Vim notation.
"""

import json
import random
import sys


def grid_cases():
    random.seed(1)
    texts = [
     "foo bar.baz  qux\n    indented line(arg, [1, 2])\n\nlast_word end.",
     "int main() {\n    if (x) {\n        call(a, \"str\", 'c');\n    }\n    return 0;\n}",
     "This is one. And two! Three?\nNext para line.\n\nSecond para here.\nMore text",
     "a\n\n\nb c\n  d",
     "<div><p>hi <b>x</b></p></div>",
     "x = foo(bar(1), \"q u\") + [a, {b: c}]\n\tword\t tab",
    ]
    motions = ["h","l","j","k","w","W","b","B","e","E","ge","gE","0","^","$","g_","G","gg","2w","3e","2b","}","{",")","(","%","f.","F(","ta","T ","fa;","ta,","+","-","_","2j","5l","3|","<BS>"," ","2$","3G"]
    objects = ["iw","aw","iW","aW","i(","a(","ib","i[","a[","i{","a{","iB","i\"","a\"","i'","a'","ip","ap","is","as","it","at","2iw","2aw","3aw","2i(","i<","a<"]
    ops = ["", "d", "c", "y", "g~", "gU", ">"]
    cases = []
    for t in texts:
        lines = t.split('\n')
        positions = []
        for li, l in enumerate(lines):
            cols = list(range(len(l))) or [0]
            positions += [(li, c) for c in random.sample(cols, min(len(cols), 3))]
        for (li, col) in positions:
            for op in ops:
                for m in motions:
                    keys = op + m + ("XY<Esc>" if op == "c" else "")
                    cases.append({"text": t, "line": li, "col": col, "keys": keys})
                if op:
                    for o in objects:
                        keys = op + o + ("XY<Esc>" if op == "c" else "")
                        cases.append({"text": t, "line": li, "col": col, "keys": keys})
                else:
                    for o in objects:
                        cases.append({"text": t, "line": li, "col": col, "keys": "v" + o + "d"})
    return cases


def command_cases():
    T1 = "foo bar.baz  qux\n    indented line(arg, [1, 2])\n\nlast_word end."
    T2 = "int main() {\n    if (x) {\n        call(a, \"str\", 'c');\n    }\n    return 0;\n}"
    T3 = "alpha beta gamma\ndelta epsilon\nzeta eta theta\niota kappa\nlambda mu"
    T4 = "x1 = 10\ny = -5 + 0x0f\nz = 007\nw = 9"
    T5 = "b\na\nc\na\n10\n9\nB"
    cases = []
    def add(text, line, col, keys, checkreg=True):
        cases.append({"text": text, "line": line, "col": col, "keys": keys, "checkreg": checkreg})
    simple = ["x","3x","X","2X","D","2D","C<Esc>","CXY<Esc>","sXY<Esc>","3sXY<Esc>","SXY<Esc>","2SXY<Esc>","Y","2Y","yyp","yyP","2yyp","yy3p","ywP","yw2p","yiwP","ddp","ddP","dd",
     "3dd","5dd","cc<Esc>","ccnew<Esc>","2ccnew<Esc>","J","3J","gJ","3gJ","rx","3rx","r<CR>","2r<CR>","~","5~","gUU","guu","g~~","gUiw","3gUU",">>","3>>","<<",">j","<k","ifoo<Esc>","3ifoo<Esc>","afoo<Esc>","Afoo<Esc>","Ifoo<Esc>","gIfoo<Esc>","ofoo<Esc>","Ofoo<Esc>","2ofoo<Esc>","3Obar<Esc>",
     "ia<CR>b<Esc>","ifoo<BS><BS>x<Esc>","A<C-w><Esc>","A x<C-w><Esc>","A<C-u><Esc>","ifoo<C-u><Esc>","Rxyz<Esc>","2Rab<Esc>","Rabc<BS><BS><Esc>","xp","ddjp","dwwP","dw.","dw..",
     "x.","dd.","cwnew<Esc>w.","ifoo<Esc>j.","Afoo<Esc>j.","A;<Esc>j.","ofoo<Esc>.","2dd.","d2w.","3dw2.","x3.","cc<Esc>j.",">>j.","~.","ddu","ddu<C-r>","xxxuu","xxxuuu<C-r>","ifoo<Esc>u","ifoo<CR>bar<Esc>u","cwnew<Esc>u","ddjddu","3xu",
     "vd","vld","v$d","vjd","vjy","vjyP","v2ed","vey$p","vwc<Esc>","vwcX<Esc>","vj>","vj<","v~","vU","vu","vjU","vjJ","vlr*","vjr*","vwo<Esc>","vwohd","vwP","vwp","viwyviwp",
     "Vd","Vjd","Vy","VjyP","Vjyp","Vc<Esc>","VcX<Esc>","Vjcnew<Esc>","V>","Vj>","V2>","Vj<","VJ","VjJ","V~","VU","Vr-","Vjr-","Vp","Vjp","yyjVp","Vd.","Vjd.",
     "<C-v>jd","<C-v>jld","<C-v>2jlld","<C-v>jy","<C-v>jyP","<C-v>jyp","<C-v>jIXX<Esc>","<C-v>jAXX<Esc>","<C-v>j$AXX<Esc>","<C-v>jlcZ<Esc>","<C-v>jr#","<C-v>jU","<C-v>j$d","<C-v>jlo<Esc>","<C-v>jlOd","<C-v>jD","<C-v>jC-<Esc>",
     "ma3jd'a","majd`a","mbjy'b","maG'a","ma3j`a","2jmakk'a","*","#","*n","*N","#n","g*","g#","/a<CR>","/a<CR>n","/a<CR>N","?a<CR>","?a<CR>n","/e<CR>x","/xyz<CR>","d/a<CR>","c/a<CR>X<Esc>","y/a<CR>","/a/e<CR>","/a/+1<CR>","/\\<a<CR>","/a\\|e<CR>","/[aeiou]\\{2}<CR>","/l.*a<CR>",
     "qaxq@a","qaddq2@a","qaA!<Esc>jq@a","qajq@a@@","qadwq@a","qqI-<Esc>jq3@q","qaxjq:normal @a<CR>",
     "\"ayy\"ap","\"ayyj\"Ayy\"ap","\"add\"ap","\"_ddp","\"byw\"bP","yy\"0p","ddyy\"1p","ddjdd\"2p","dw\"-p","\"add\"aP.","\"ayiw\"AyiwG\"ap",
     "<C-a>","5<C-a>","<C-x>","12<C-x>","j<C-a>","jw<C-a>","j$<C-a>","jj<C-a>","jjj<C-x>","<C-a>.","wv<C-a>","Vjjj<C-a>","Vjjjg<C-a>",
     "%","j%","jf(%","jjf(%","d%","jf(d%","jf(y%","[(","jjf,[(","jj$[{","jj$d[{","]}","jjl])","]]","[[",
     "gg","G","3G","3gg","dG","dgg","yG","gUG","3|","d3|",
     "vjl<Esc>gvd","vj<Esc>gvy","wgi","ifoo<Esc>gix<Esc>",
     "\"ayiw:s/a/<C-r>a/<CR>","yiw/<C-r>\"<CR>","/<C-r><C-w><CR>",
     "ZZ",":2<CR>",":$<CR>",":+2<CR>",":2d<CR>",":2,3d<CR>",":%d<CR>",":.,+1y<CR>P",":2,3y<CR>G:put<CR>",":1t$<CR>",":1m$<CR>",":$m0<CR>",":2,3m0<CR>",":1,2t3<CR>",":1co.<CR>",":j<CR>",":1,3j<CR>",":2,3j!<CR>",":2>><CR>",":%><CR>",":2<<CR>",
     ":s/a/X/<CR>",":s/a/X/g<CR>",":%s/a/X/<CR>",":%s/a/X/g<CR>",":%s/\\(a\\)\\(l\\)/\\2\\1/g<CR>",":%s/a\\+/[&]/g<CR>",":%s/^/# /<CR>",":%s/$/;/<CR>",":%s/\\<\\w/\\u&/g<CR>",":%s/e/E/gi<CR>",":%s/A/x/i<CR>",":%s/\\v(\\w+) (\\w+)/\\2 \\1/<CR>",":%s#a#/#g<CR>",":%s/a//g<CR>",":%s/a/\\r/<CR>",":%s/\\n//<CR>",":2,3s/a/X/g<CR>",":s/a/X/<CR>j:s<CR>",":s/a/X/<CR>j&",":%s/a/X/<CR>u",":%s/eta/ETA/g<CR>",":%s/\\s\\+/_/g<CR>",":%s/t\\zsa/Z/g<CR>",":%s/\\U/x/g<CR>",":%s/[aeiou]/\\U&/g<CR>",
     ":g/a/d<CR>",":v/a/d<CR>",":g!/a/d<CR>",":g/e/s/e/E/g<CR>",":g/^/m0<CR>",":g/a/normal Ax<CR>",":g/eta/t$<CR>",":%normal A;<CR>",":2,3normal dw<CR>",":normal x<CR>",
     ":sort<CR>",":sort!<CR>",":sort u<CR>",":sort n<CR>",":sort i<CR>",":2,5sort<CR>",":%sort ui<CR>",
     "ma:'a,$d<CR>","jmajmb:'a,'bd<CR>","Vj:s/a/X/g<CR>","Vj:d<CR>","Vj<Esc>:'<,'>d<CR>","vj:normal Ax<CR>",
     "dap","yapP","cipnew<Esc>","dip","3J","3Ji<Esc>","xu.","2dwu.",
    ]
    for t in [T1, T2, T3, T4, T5]:
        for line, col in [(0,0),(0,3),(1,5),(2,2)]:
            tl = t.split('\n')
            if line >= len(tl): continue
            col = min(col, max(0, len(tl[line])-1))
            for k in simple:
                add(t, line, col, k)
    return cases


def more_cases():
    T1 = "foo bar.baz  qux\n    indented line(arg, [1, 2])\n\nlast_word end."
    T2 = "int main() {\n    if (x) {\n        call(a, \"str\", 'c');\n    }\n    return 0;\n}"
    T3 = "alpha beta gamma  \n  delta (epsilon)\n\nzeta Eta theta.\n)iota kappa\n\tlambda mu"
    T6 = "one two three\nfour five six\nseven eight nine\nten eleven twelve"
    keys = [
     "3ix<Esc>","2afoo <Esc>","3Aab<Esc>","2Ixy<Esc>","3ox<Esc>","2Oab<Esc>j.","ciwX<Esc>w.","ciwX<Esc>w3.","3cwX<Esc>","c2wX<Esc>.","A!<Esc>j.j.","I# <Esc>j.",
     "Rab<Esc>","3Rab<Esc>","Rabcdefghijklmnopqrstuvwxyz0123<Esc>","Rab<BS>c<Esc>","Ra<CR>b<Esc>","Rxy<Esc>j.","ifoo<C-o>0bar<Esc>","ifoo<C-o>$!<Esc>","ifoo<C-o>dwbar<Esc>",
     "yiwA <C-r>\"<Esc>","\"ayiwjA<C-r>a<Esc>","i<C-t><Esc>","A<C-t><C-t><Esc>","jA<C-d><Esc>","ji<C-d>x<Esc>","i<C-e><C-e><Esc>","ji<C-y><C-y><Esc>","ifoo<Esc>jA<C-a><Esc>","ia<C-v><Tab>b<Esc>",
     "J","2J","4J","jJ","jjJ","3jJ","gJ","jjgJ","$J",
     "~","$~","5~","10~","r1","5rx","20rx","$rx","X","5X","0X","D","$D","s<Esc>","$sZ<Esc>","5sZ<Esc>",
     "yiwP","yiwp","yiw3p","yiwgp","yiwgP","yyjp","yyjgp","yyjP","yyjgP","yy3p","yy2P","yiwjyy\"0P","yiwjyy\"1P","yyjdw\"-p",
     "\"ayy\"Ayy\"ap","\"ayw\"Ayy\"ap","\"ayy\"Ayw\"ap","\"ayw\"Ayw\"ap","\"Ayw\"ap",
     "<C-v>jly$p","<C-v>jly2p","<C-v>jlyjjP","<C-v>2jd$p","<C-v>jlyGo<Esc>p","<C-v>j$y$p","yyV<C-v>jlp",
     "wyiwviwp","wyiwvep","wyiwvjp","yyvjp","wyiwVp","wyiwVjp","yyVjP","wvey$vep",
     "viw","vawd","viwiwd","vaw2awd","vipd","vapd","vjipd","vi(d","va(d","vi(i(d","vi\"d","va\"d","vitd","vatd",
     "gUiw.","guawj.","g~~j.",">>j.","<<j.","vjdj.","Vjd.","vlld.","v2jd.","<C-v>jlld.","<C-v>jIX<Esc>j.","vjcX<Esc>j.",
     "/e/e<CR>","/e/e+1<CR>","/e/e-1<CR>","/e/s+1<CR>","/e/b-1<CR>","/e/+1<CR>","/e/-1<CR>","?e?e<CR>","/e<CR>d//e<CR>","/\\cFOO<CR>","/FOO\\c<CR>","/\\v(one|four)<CR>","/t\\{2}<CR>","/[^a-z ]<CR>","/^f<CR>","/e$<CR>","/\\<t<CR>","/o\\><CR>","/\\Vx.y<CR>","/e\\n<CR>","/\\_s\\+e<CR>","/a\\|z<CR>","3/e<CR>","/e<CR>3n","/e<CR>2N","*3n","#2n","*d''",
     ":set ic<CR>/FOO<CR>",":set ic scs<CR>/Foo<CR>",":set nows<CR>G/one<CR>",":set sw=2<CR>>>",":set noet<CR>>>",":set sw=8 noet ts=8<CR>>>",
     "%","50%","100%","d%","f(%","2%",
     "ma2jmb:'a,'bs/e/E/<CR>",":g/e/j<CR>",":g/o/normal dd<CR>",":g/^$/d<CR>",":v/e/normal Ax<CR>",":2,$g/e/s//E/<CR>",":%s/o/0/g|<CR>",":%s/\\w\\+/\\u\\0/g<CR>",":%s/\\w\\+/\\U\\0\\E!/<CR>",":%s/[a-z]\\+/\\L\\u&/g<CR>",":%s/e/&&/g<CR>",":%s/e/\\&/g<CR>",":%s/o/X/g 2<CR>",":%s/o/X/2<CR>",":s/o/X/g<CR>:%&<CR>",":%s/o/X/n<CR>",":%s/zz/X/e<CR>",
     ":2,3>><CR>",":%<<CR>",":.,+2j<CR>",":$-1,$d<CR>",":.;+1d<CR>",":/four/d<CR>",":/four/,/eight/d<CR>",":?three?d<CR>",":0put<CR>",":$put<CR>",":2y|$put<CR>",":1,2m$<CR>",":3m1<CR>",":1t0<CR>",":%t$<CR>",":2,3m0<CR>",":2,3co$<CR>",
     ":2,3sort!<CR>",":sort /\\w\\+ /<CR>",":sort n<CR>",":sort u<CR>",":%sort x<CR>",
     "qqddq@qu","qqA,<Esc>jq2@q","qwciwX<Esc>wq@w@@","qe0f dwjq3@e","qaq\"ayyj@a","qayypq@a",
     "<C-a>","w<C-a>","ww5<C-x>","$<C-a>","3<C-a>..","v$<C-a>","Vj<C-x>","Vjjg<C-a>",
     "gi","ifoo<Esc>jgix<Esc>","ifoo<Esc>jj`^x","ifoo<Esc>jj'^x","xjj`.x","xjj'.x","yiwjj`[x","yiwjj`]x","vj<Esc>gg`<x","vj<Esc>gg`>x","Vj<Esc>'>x",
     "mzjj`zx","mAjjj'Ax","jjmxgg:'x<CR>x","ma``x","Gjj``x","G''x","3Gx``x",
     "dap","dip","2dap","cipX<Esc>","yap","yipP","gqip","gqq","gwip","gqap",
     "=ip","==","=G","gg=G",
    ]
    cases = []
    for t in [T1, T2, T3, T6]:
        tl = t.split('\n')
        for line, col in [(0,0),(0,5),(1,3),(3,2)]:
            if line >= len(tl): continue
            col = min(col, max(0, len(tl[line])-1))
            for k in keys:
                if k.startswith('=') or 'gg=G' in k:
                    continue
                cases.append({"text": t, "line": line, "col": col, "keys": k})
    return cases


def search_match_cases():
    T6 = "one two three\nfour five six\nseven eight nine\nten eleven twelve"
    T1 = "foo bar.baz  qux\n    indented line(arg, [1, 2])\n\nlast_word end."
    keys = ["/e<CR>cgnX<Esc>..","/e<CR>dgn","/e<CR>gnd","/e<CR>gNd","*cgnZZ<Esc>.","/e<CR>ggdgn..","/ve<CR>cgnX<Esc>.", "vjipd","Vipipd","vipapd","vjapd"]
    cases=[]
    for t in [T6,T1]:
        for (l,c) in [(0,0),(1,4),(2,2)]:
            for k in keys:
                cases.append({"text":t,"line":l,"col":c,"keys":k})
    return cases


def misc_cases():
    T6 = "one two three\nfour five six\nseven eight nine\nten eleven twelve"
    T1 = "foo bar.baz  qux\n    indented line(arg, [1, 2])\n\nlast_word end."
    keys = ["v/e<CR>d","v?o<CR>d","vj/i<CR>y","d/i/e<CR>","y?t<CR>","c/v<CR>X<Esc>","d/e<CR>.","2d/e<CR>","ifoo<C-o>ddbar<Esc>","ifoo<C-o>2wbar<Esc>","Rab<CR>c<Esc>","Rab<CR>c<BS><BS><BS><Esc>","3ccX<Esc>","5SY<Esc>","ifoo<Esc>:normal .<CR>",":2,3normal .<CR>","qaAx<Esc>q:2,3normal @a<CR>",
     "A<Tab>x<Esc>","i<Tab><Esc>",">>..","3>>.","<<",">ip",">2j","gUap","g??","g?g?","g?iw","guiwgUl","v<Esc>gvd",":s/o/0/g<CR>u:redo<CR>",":undo<CR>","xu:redo<CR>",
     "d)","d(","c)X<Esc>","y}P","d}","d{","3}d{","v}d","V}d","dh","dk","dj","5dj","d3k","yj","yk","cjX<Esc>","ckX<Esc>","<j",">k","=j",
     "o<Esc>","O<Esc>","o<Esc>u","ix<Esc>ay<Esc>u","ix<Esc>ay<Esc>uu",
     "J.","3J.","gJu","r<Tab>","f<Tab>","wD.","w2D","ea!<Esc>w.","A;<Esc>+.","I//<Esc>j.",
     "*Nx","#nx","g*x","wv3e~","vee~","ve~.","ylvp","yl3vp","yw$p","ylP","yl2P",
     "diw.","daw.","2daw","d2aw","ciw<C-r>-<Esc>","\"_diwP","\"1pu.u.","dddd\"1p..","dddd\"2p",
    ]
    cases=[]
    for t in [T6,T1]:
        for (l,c) in [(0,0),(1,4),(2,2),(3,5)]:
            for k in keys:
                cases.append({"text":t,"line":l,"col":c,"keys":k})
    return cases


def audit_cases():
    L = "l1\nl2\nl3\nl4\nl5"
    M = "l1\nl2 x\nl3\nl4\nl5"
    A = "abc\ndef\nghi"
    E = "\nab c\nq"
    G = "a\na\na\nc\nd"
    R = "x ababc\nfoo\nbar ]a q"
    groups = [
        (L, 2, 0, [":d 2147483647<CR>", ":j 2147483647<CR>", ":.+4294967298d<CR>",
                   ":.-4294967298d<CR>", ":.+2147483647d<CR>", ":m 99<CR>", ":t 99<CR>",
                   ":m-5<CR>", ":m 6<CR>", ":m 5<CR>", ":t0<CR>", "46341d46341j",
                   "2d99999999j", ":s/l/X/<CR>j:@:<CR>j@:", ":s/l/X/<CR>j:1@:<CR>",
                   ":s/l/X/<CR>j:3@:<CR>j@:", ":s/l/X/<CR>:g/./@:<CR>",
                   ":s/l/X/<CR>j:silent @:<CR>", ":s/l/X/<CR>:norm 3@:<CR>",
                   ":s/l/X/<CR>j:@:<CR>j:@:<CR>"]),
        (M, 0, 0, ["jmadd'ax", "jmadd`ax", "jmaddu'ax", "jmaVjd'ax", "jmaggvjjd'ax",
                   "jmakJ'ax", "jma:2m4<CR>'ax", "jma:2d<CR>gg:'a<CR>x", "jmakdd'ax",
                   "jmajdd'ax", "jmacc<Esc>'ax"]),
        (A, 0, 0, ["qaqqa0xj@aq@a", "qa0xjq3@a", "qafzxq@a",
                   "qa/zzz<CR>xq@a", "qa0xj:s/q/r/<CR>q2@a", "qa0xj:foo<CR>xq@a",
                   "qa0xjq:2,3normal 3@a<CR>", "qaxjq:g/./normal 3@a<CR>",
                   ":g/./normal xfzx<CR>"]),
        (E, 0, 0, ["qa<C-a>Aq<Esc>jq2@a", "qaJAq<Esc>jq2@a", "qahAq<Esc>jq2@a",
                   "qa$lAq<Esc>jq2@a", "qaXAq<Esc>jq2@a"]),
        (G, 0, 0, [":g/a/+1d<CR>", ":g/a/.,+1d<CR>", ":g/c/-1,.d<CR>", ":g/a/+1j<CR>",
                   ":g/a/+1s/a/x/<CR>"]),
        ("a/b ab", 0, 0, [":s/x*/-/g<CR>", ":s/b*/-/g<CR>", ":s/\\<\\|\\>/|/g<CR>",
                          ":s/a\\zs/-/g<CR>", ":s/$/-/g<CR>", ":s/x*/-/<CR>"]),
        ("x*y\n\nab", 0, 0, [":%s/x*/-/g<CR>", ":%s/b*/-/g<CR>"]),
        (R, 0, 0, ["/\\%(ab\\)\\+c<CR>", "/\\v%(ab)+c<CR>", "/[^]a]q<CR>", "/[]a]<CR>",
                   ":%s/\\%(ab\\)\\+/Z/<CR>", ":%s/[^]a ]/-/g<CR>",
                   "/o\\_a\\+r<CR>", "/c\\_[a-z]\\+r<CR>"]),
        ("a1\n2g", 0, 0, ["/1\\_x\\+g<CR>"]),
        ("AB\nCd\nef", 0, 0, ["/B\\_u\\+d<CR>", "/d\\_l\\+f<CR>", "/B\\_a\\+f<CR>"]),
        ("x 9223372036854775807 y", 0, 0, ["<C-a>", "2<C-a>", "<C-x>"]),
        ("x -9223372036854775808 y", 0, 0, ["<C-x>", "<C-a>"]),
        ("x 99999999999999999999 y", 0, 0, ["<C-a>", "<C-x>"]),
        ("x 5 y", 0, 0, ["99999999<C-x>", "99999999<C-a>"]),
        ("x 0xffffffffffffffff y", 0, 0, ["<C-a>"]),
        ("alpha beta gamma\nsecond line", 0, 0, ["qa<C-Right>q0@ax", "qa<S-Right>q0@ax",
                                                "qa<S-Down>qgg@ax"]),
        ("ab", 0, 0, ["200ixy<Esc>", "3ia<CR>b<Esc>"]),
        ("one\ntwo\nthree", 0, 0, [":s/o/0/<CR>:1@:<CR>:silent @:<CR>:g/./@:<CR>@:"]),
    ]
    cases = []
    for text, line, col, keys in groups:
        for k in keys:
            cases.append({"text": text, "line": line, "col": col, "keys": k, "checkreg": False})
    return cases


SUITES = {
    "grid": grid_cases,
    "cmds": command_cases,
    "more": more_cases,
    "gn": search_match_cases,
    "misc": misc_cases,
    "audit": audit_cases,
}

if __name__ == "__main__":
    for name, generator in SUITES.items():
        cases = generator()
        with open(f"{name}.json", "w") as handle:
            json.dump(cases, handle)
        print(name, len(cases))
