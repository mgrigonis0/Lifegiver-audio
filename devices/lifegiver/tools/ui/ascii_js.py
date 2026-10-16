#!/usr/bin/env python3
"""Make a Max JS file plain ASCII: non-ASCII characters in string literals become \\u escapes, in comments plain
look-alikes. Max reads a JS file's bytes as they are (CONVENTIONS "Max JS rules").   python3 ascii_js.py <file.js>"""
import sys

p = sys.argv[1]
t = open(p, encoding="utf-8").read()
out, i, n = [], 0, len(t)
while i < n:
    c = t[i]
    if t.startswith("//", i):
        j = t.find("\n", i)
        j = n if j < 0 else j
        seg = t[i:j]
        for a, b in [("·", "."), ("…", "..."), ("—", "-"), ("–", "-"), ("↔", "<->"), ("°", " deg"), ("×", "x"), ("→", "->")]:
            seg = seg.replace(a, b)
        out.append("".join(ch if ord(ch) < 128 else "?" for ch in seg))
        i = j
        continue
    if c in "\"'":
        j = i + 1
        while j < n and t[j] != c:
            if t[j] == "\\":
                j += 1
            j += 1
        out.append("".join(ch if ord(ch) < 128 else "\\u%04x" % ord(ch) for ch in t[i:j + 1]))
        i = j + 1
        continue
    out.append(c)
    i += 1
open(p, "w", encoding="ascii").write("".join(out))
