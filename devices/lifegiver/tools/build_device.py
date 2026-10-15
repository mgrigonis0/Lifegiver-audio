#!/usr/bin/env python3
"""Generate the Lifegiver Max for Live device (never hand-edit the outputs).

Writes devices/lifegiver/Lifegiver/Lifegiver.maxpat (plain patcher) and Lifegiver.amxd (the same, as a Live audio
effect). Same generator design as wanderer-synth's tools/build_device.py.

Signal:   plugin~ -> lifegiver~ (left: inlet 0, right: inlet 1) -> plugout~
Controls: every Live control -> [prepend <name>] -> lifegiver~
Load:     live.thisdevice -> "init" (lifegiver~ pulls its saved state), controls re-output, version printed
Saving:   [pattr state] is a hidden Live parameter (blob) holding the engine's state dict inside the Live set:
          lifegiver~ "getstate" -> pattr bang -> "dictionary <name>" -> lifegiver~; saves go the other way
Latency:  the patcher's "latency" (Defined Latency, samples) is fixed; the engine is zero-latency (DESIGN §13)
"""
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "Lifegiver"
VERSION = re.search(r'VERSION = "([^"]+)"', (ROOT / "source/engine/version.h").read_text()).group(1)
LATENCY = 0

boxes, lines = [], []
_n = [0]


def box(maxclass, rect, text=None, ins=1, outs=1, outtypes=None, pres=None, **extra):
    _n[0] += 1
    bid = f"obj-{_n[0]}"
    b = {"id": bid, "maxclass": maxclass, "numinlets": ins, "numoutlets": outs, "patching_rect": [float(v) for v in rect]}
    if outs:
        b["outlettype"] = outtypes if outtypes is not None else [""] * outs
    if text is not None:
        b["text"] = text
    if pres is not None:
        b["presentation"] = 1
        b["presentation_rect"] = [float(v) for v in pres]
    b.update(extra)
    boxes.append({"box": b})
    return bid


def obj(text, rect, ins=1, outs=1, outtypes=None, **extra):
    return box("newobj", rect, text=text, ins=ins, outs=outs, outtypes=outtypes, **extra)


def msg(text, rect):
    return box("message", rect, text=text, ins=2, outs=1)


def wire(src, so, dst, di):
    lines.append({"patchline": {"source": [src, so], "destination": [dst, di]}})


def valueof(longname, short, ptype, lo, hi, init, unitstyle=1, exponent=1.0, units=None, enum=None, invisible=0):
    v = {"parameter_longname": longname, "parameter_shortname": short, "parameter_type": ptype,
         "parameter_mmin": lo, "parameter_mmax": hi, "parameter_initial": [init],
         "parameter_initial_enable": 1, "parameter_unitstyle": unitstyle}
    if exponent != 1.0:
        v["parameter_exponent"] = exponent
    if units:
        v["parameter_units"] = units
    if enum:
        v["parameter_enum"] = enum
    if invisible:
        v["parameter_invisible"] = invisible
    return {"valueof": v}


def rgba(c, a=1.0):
    return [c[0], c[1], c[2], a]


# --------------------------------------------------------------------------------------------- colours (Wanderer's)
C = {"listen": (0.937, 0.353, 0.373), "stage": (0.31, 0.784, 0.753), "tone": (0.953, 0.604, 0.231),
     "dyn": (0.29, 0.639, 0.961), "moves": (0.655, 0.545, 0.98), "out": (0.941, 0.541, 0.478)}
PANEL = (0.11, 0.114, 0.125)
TXT = (0.925, 0.925, 0.925)
DIM = (0.55, 0.55, 0.56)

INFO = {
    "amount": "How much of Lifegiver you hear: 0 % = the original, untouched; 100 % = the full plan. "
              "In this first build there is no plan yet, so the sound passes through unchanged.",
}


def info(name, title):
    t = INFO.get(name)
    return {"annotation": t, "annotation_name": title} if t else {}


# --------------------------------------------------------------------------------------------- core
plug_in = obj("plugin~", (40, 260, 60, 22), ins=2, outs=2, outtypes=["signal", "signal"])
eng = obj("lifegiver~ ---lfg", (40, 300, 140, 22), ins=2, outs=3, outtypes=["signal", "signal", ""])
plug_out = obj("plugout~", (40, 350, 60, 22), ins=2, outs=0)
wire(plug_in, 0, eng, 0); wire(plug_in, 1, eng, 1)
wire(eng, 0, plug_out, 0); wire(eng, 1, plug_out, 1)

thisdev = obj("live.thisdevice", (40, 20, 90, 22), ins=1, outs=3, outtypes=["bang", "int", "int"])
tinit = obj("t b b b", (40, 50, 60, 22), ins=1, outs=3, outtypes=["bang", "bang", "bang"])
wire(thisdev, 0, tinit, 0)
initmsg = msg("init", (40, 80, 40, 22))
wire(tinit, 0, initmsg, 0); wire(initmsg, 0, eng, 0)
vmsg = msg(f"patch {VERSION}", (140, 80, 110, 22))
vprint = obj("print lifegiver", (140, 110, 90, 22), ins=1, outs=0)
wire(tinit, 1, vmsg, 0); wire(vmsg, 0, vprint, 0)

# saving: a hidden blob parameter holding the engine's dict
pattr = obj("pattr state", (600, 300, 82, 22), ins=1, outs=3, parameter_enable=1,
            saved_attribute_attributes=valueof("State", "State", 3, 0, 1, 0, invisible=1))
wire(pattr, 0, eng, 0)
route = obj("route getstate dictionary", (200, 400, 160, 22), ins=2, outs=3)
wire(eng, 2, route, 0)
tget = obj("t b", (200, 430, 30, 22), ins=1, outs=1, outtypes=["bang"])
wire(route, 0, tget, 0); wire(tget, 0, pattr, 0)
pdict = obj("prepend dictionary", (260, 460, 110, 22))
wire(route, 1, pdict, 0); wire(pdict, 0, pattr, 0)

# --------------------------------------------------------------------------------------------- layout (1x)
DEV_H = 169
PY, PH = 4, 161
IP = 6
CELL = 44
PANEL_W = 2 * IP + 120
PX = 6
DEVICE_W = PX + PANEL_W + 6
Y0 = PY + 4

box("panel", (970, 20, 40, 30), ins=1, outs=0, pres=(PX, PY, PANEL_W, PH), bgcolor=rgba(PANEL), border=0,
    rounded=6, background=1, ignoreclick=1, mode=0)


def header(title, color, x, w):
    box("live.comment", (1000, 40 + len(boxes), 80, 18), text=title, ins=1, outs=0,
        pres=(x - 3, Y0 - 3, w + 6, 14), textcolor=rgba(color), fontname="Ableton Sans Bold", fontsize=9.5, fontface=1)
    box("panel", (1100, 40 + len(boxes), 80, 2), ins=1, outs=0, pres=(x, Y0 + 11, w, 2),
        bgcolor=rgba(color, 0.55), border=0, rounded=1, ignoreclick=1)


def label(text, x, y, w, color=TXT, size=8.5):
    box("live.comment", (1200, 40 + len(boxes), 60, 18), text=text, ins=1, outs=0, pres=(x - 2, y - 2, w + 4, 13),
        textcolor=rgba(color), fontsize=size, textjustification=1)


bang_at_init = []


def to_engine(name, src, x, y):
    pre = obj(f"prepend {name}", (x, y, 100, 22))
    wire(src, 0, pre, 0)
    wire(pre, 0, eng, 0)


def dial(name, longn, short, lo, hi, init, ustyle, color, pres, at=(0, 0)):
    d = box("live.dial", (700 + at[0], 600 + at[1], 44, 48), ins=1, outs=2, outtypes=["", "float"], pres=pres,
            varname=longn, parameter_enable=1, **info(name, longn), fontsize=8.5,
            activedialcolor=rgba(color), activeneedlecolor=rgba(color), dialcolor=rgba((0.055, 0.055, 0.063)),
            textcolor=rgba(TXT), showname=1, shownumber=1,
            saved_attribute_attributes=valueof(longn, short, 0, lo, hi, init, ustyle))
    bang_at_init.append(d)
    to_engine(name, d, 700 + at[0], 655 + at[1])
    return d


x0 = PX + IP
header("LIFEGIVER", C["out"], x0, PANEL_W - 2 * IP)
label("first build: pass-through", x0, Y0 + 20, PANEL_W - 2 * IP, DIM, 8.0)
dial("amount", "Amount", "Amount", 0, 100, 100, 5, C["out"], (x0 + (PANEL_W - 2 * IP - CELL) / 2, Y0 + 44, CELL, 50))
label(f"build {VERSION}", x0, PY + PH - 16, PANEL_W - 2 * IP, DIM, 7.5)

for d in bang_at_init:
    wire(tinit, 2, d, 0)

patcher = {
    "patcher": {
        "fileversion": 1,
        "appversion": {"major": 8, "minor": 6, "revision": 4, "architecture": "x64", "modernui": 1},
        "classnamespace": "box",
        "rect": [60.0, 60.0, 1400.0, 900.0],
        "openrect": [0.0, 0.0, float(DEVICE_W), float(DEV_H)],
        "bglocked": 0, "openinpresentation": 1,
        "default_fontsize": 10.0, "default_fontface": 0, "default_fontname": "Arial Bold",
        "gridonopen": 1, "gridsize": [8.0, 8.0], "gridsnaponopen": 1, "objectsnaponopen": 1,
        "statusbarvisible": 2, "toolbarvisible": 1, "boxanimatetime": 200, "enablehscroll": 1,
        "enablevscroll": 1, "devicewidth": float(DEVICE_W), "description": "Lifegiver",
        "digest": "", "tags": "", "style": "", "subpatcher_template": "",
        "boxes": boxes, "lines": lines,
        "dependency_cache": [{"name": "lifegiver~.mxo", "type": "iLaX"}],
        "latency": LATENCY, "is_mpe": 0, "minimum_live_version": "", "minimum_max_version": "",
        "platform_compatibility": 0,
        "project": {
            "version": 1, "creationdate": 3590052493, "modificationdate": 3590052493,
            "viewrect": [0.0, 0.0, 300.0, 500.0], "autoorganize": 1, "hideprojectwindow": 1,
            "showdependencies": 1, "autolocalize": 0, "contents": {"patchers": {}},
            "layout": {}, "searchpath": {}, "detailsvisible": 0,
            "amxdtype": int.from_bytes(b"aaaa", "big"), "readonly": 0, "devpathtype": 0, "devpath": ".",
            "sortmode": 0, "viewmode": 0, "includepackages": 0,
        },
        "autosave": 0,
    }
}


def amxd(patch_json: bytes, devtype: bytes = b"aaaa") -> bytes:
    """Unfrozen device container: ampf(type) + meta + ptch(JSON, NUL-terminated), little-endian lengths.
    Type "aaaa" = audio effect ("iiii" = instrument, "mmmm" = MIDI effect)."""
    body = patch_json + b"\x00"
    return (b"ampf" + struct.pack("<I", 4) + devtype +
            b"meta" + struct.pack("<I", 4) + struct.pack("<I", 0) +
            b"ptch" + struct.pack("<I", len(body)) + body)


def check():
    ids = {b["box"]["id"] for b in boxes}
    byid = {b["box"]["id"]: b["box"] for b in boxes}
    for l in lines:
        s, d = l["patchline"]["source"], l["patchline"]["destination"]
        assert s[0] in ids and d[0] in ids, l
        assert s[1] < byid[s[0]]["numoutlets"], (byid[s[0]].get("text"), s)
        assert d[1] < byid[d[0]]["numinlets"], (byid[d[0]].get("text"), d)
    for b in boxes:
        r = b["box"].get("presentation_rect")
        if r:
            assert r[0] >= 0 and r[1] >= 0 and r[0] + r[2] <= DEVICE_W + 0.01 and r[1] + r[3] <= DEV_H + 0.01, (b["box"].get("text"), r)
    # every name shared inside a device starts with --- (CONVENTIONS: duplicates must never crosstalk)
    for b in boxes:
        t = b["box"].get("text", "")
        if b["box"]["maxclass"] == "newobj" and re.match(r"^(send|s|receive|r|buffer~|dict|value|v|table)\b", t):
            assert " ---" in t, f"shared name without ---: {t}"
    longnames = [b["box"]["saved_attribute_attributes"]["valueof"]["parameter_longname"]
                 for b in boxes if b["box"].get("parameter_enable") == 1]
    assert len(longnames) == len(set(longnames)), f"duplicate Live parameter names: {longnames}"
    assert patcher["patcher"]["project"]["amxdtype"] == 1633771873


def main():
    OUT.mkdir(exist_ok=True)
    boxes.sort(key=lambda b: 0 if b["box"].get("background") == 1 else 1)   # background panels drawn first
    check()
    text = json.dumps(patcher, indent="\t").encode("utf-8")
    (OUT / "Lifegiver.maxpat").write_bytes(text)
    (OUT / "Lifegiver.amxd").write_bytes(amxd(text))
    nparams = sum(1 for b in boxes if b["box"].get("parameter_enable") == 1)
    print(f"lifegiver patch {VERSION}: {len(boxes)} boxes, {len(lines)} connections, {nparams} Live parameters, "
          f"device {DEVICE_W}x{DEV_H}, latency {LATENCY}")


if __name__ == "__main__":
    main()
