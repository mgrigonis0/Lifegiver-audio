#!/usr/bin/env python3
"""Generate the Lifegiver Max for Live device (never hand-edit the outputs).

Writes devices/lifegiver/Lifegiver/Lifegiver.maxpat (plain patcher) and Lifegiver.amxd (the same, as a Live audio
effect). Same generator design as wanderer-synth's tools/build_device.py.

Signal:   plugin~ -> lifegiver~ (left: inlet 0, right: inlet 1) -> plugout~
View:     one jsui (Lifegiver/lifegiver.strip.js) draws the whole strip after the approved mockup. It sends
          "p <name> <value>" -> [route p] -> [route <names>] -> the hidden Live parameters (live.dial), and the rest
          (arm, removetake, pickrole, solo, compare) straight to lifegiver~.
Controls: every Live parameter -> [prepend <name>] -> lifegiver~ and the view (automation, mapping and saving are Live's)
Engine:   lifegiver~ info -> [route getstate statelist param] -> the pattr / the parameters ("param <name> <v>": the role
          buttons' starting places) / the view (numbers only)
Load:     live.thisdevice -> parameters re-output, version printed, "init" (lifegiver~ pulls its saved state)
Saving:   [pattr state] is a hidden Live parameter (blob) holding the engine's state as a list of numbers:
          lifegiver~ "getstate" -> pattr bang -> [prepend statelist] -> lifegiver~;
          lifegiver~ "statelist <numbers>" -> [route] -> pattr (stored in the Live set)
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


def valueof(longname, short, ptype, lo, hi, init, unitstyle=1, exponent=1.0, units=None, enum=None, invisible=0,
            initial_enable=1):
    """initial_enable=0: no initial value at all (state parameters: Cycling '74's docs say an enabled initial value
    is recalled when a device is instantiated, saved or edited, which would wipe a stored state list on load)."""
    v = {"parameter_longname": longname, "parameter_shortname": short, "parameter_type": ptype,
         "parameter_mmin": lo, "parameter_mmax": hi,
         "parameter_initial_enable": 1 if initial_enable else 0, "parameter_unitstyle": unitstyle}
    if initial_enable:
        v["parameter_initial"] = [init]
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

# saving: a hidden blob parameter holding the engine's state as a list of numbers (source/engine/state.h).
# The list itself is stored in the set (not a dict name: build 15a saved only a name, which changes on reload).
# Parameter mode of a plain object (pattr) is an OBJECT attribute: Max reads it from saved_object_attributes (as Max
# itself writes it). A box-level parameter_enable only works for UI objects such as live.dial; on a pattr it is
# ignored, so Live had nothing to store (15a-15d: the set held an empty blob). Proven in Live with 15e.
pattr = obj("pattr state", (600, 300, 82, 22), ins=1, outs=3, parameter_enable=1, varname="state",
            saved_object_attributes={"parameter_enable": 1, "parameter_mappable": 0},
            saved_attribute_attributes=valueof("State", "State", 3, 0, 1, 0, invisible=1, initial_enable=0))
pstate = obj("prepend statelist", (600, 330, 100, 22))
wire(pattr, 0, pstate, 0); wire(pstate, 0, eng, 0)

# --------------------------------------------------------------------------------------------- the Live parameters
# Hidden in the device (the view draws them), but real Live parameters: automatable, saved, mappable.
# (message name, long name, short name, type 0 float / 2 enum, min, max, initial, unit style, exponent, enum)
PARAMS = [
    ("weight", "Weight", "Weight", 0, -10, 10, 0, 1, 1.0, None),
    ("warm", "Warm", "Warm", 0, -10, 10, 0, 1, 1.0, None),
    ("bite", "Bite", "Bite", 0, -10, 10, 0, 1, 1.0, None),
    ("air", "Air", "Air", 0, -10, 10, 0, 1, 1.0, None),
    ("texture", "Texture", "Texture", 0, -10, 10, 0, 1, 1.0, None),
    ("punch", "Punch", "Punch", 0, -10, 10, 0, 1, 1.0, None),
    ("tail", "Tail", "Tail", 0, -10, 10, 0, 1, 1.0, None),
    ("steady", "Steady", "Steady", 0, -10, 10, 0, 1, 1.0, None),
    ("role", "Role", "Role", 2, 0, 6, 0, 0, 1.0, ["Lead", "Hook", "Support", "Rhythm", "Bass", "Bed", "FX"]),
    ("pan", "Stage Pan", "Pan", 0, -1, 1, 0, 1, 1.0, None),
    ("depth", "Distance", "Distance", 0, 0, 1, 0.82, 1, 1.0, None),
    ("width", "Width", "Width", 0, 0, 1, 0.35, 1, 1.0, None),
    ("edge", "Edge", "Edge", 0, 0, 1, 0, 1, 1.0, None),   # 17b: unused (Edge removed); kept so old sets open
    ("lo", "Range Lo", "Range Lo", 0, 20, 20000, 60, 3, 3.0, None),
    ("hi", "Range Hi", "Range Hi", 0, 20, 20000, 20000, 3, 3.0, None),
    ("fit", "Identity Fit", "Id-Fit", 0, 0, 1, 0.5, 1, 1.0, None),
    ("amount", "Amount", "Amount", 0, 0, 100, 100, 5, 1.0, None),
    ("clean", "Clean", "Clean", 0, 0, 100, 40, 5, 1.0, None),
    ("vara", "A Polish", "A", 0, 0, 100, 0, 5, 1.0, None),
    ("varb", "B Warm", "B", 0, 0, 100, 0, 5, 1.0, None),
    ("varc", "C Bold", "C", 0, 0, 100, 0, 5, 1.0, None),
    ("vard", "D Vintage", "D", 0, 0, 100, 0, 5, 1.0, None),
    ("vare", "E Driven", "E", 0, 0, 100, 0, 5, 1.0, None),
    # 16c (appended: the older parameters keep their places)
    ("scale", "Scale", "Scale", 0, 0, 400, 100, 5, 1.0, None),
    ("dynamics", "Dynamics", "Dynamics", 2, 0, 1, 0, 0, 1.0, ["Dynamic", "Average"]),
    # 16d (appended)
    ("gain", "Gain", "Gain", 0, -12, 12, 0, 4, 1.0, None),
]
NAMES = [p[0] for p in PARAMS]

INFO = {
    "view": "Lifegiver listens to a sound and shapes it for its job in the mix. Press Arm and play (up to 4 takes). "
            "Pick a role, place the sound on the stage (drag: pan and distance; side handles: width; "
            "the strip: the range it owns), and turn the knobs for more or less of each quality. Moves shows what it "
            "does; A–E add characters, from subtle (A) to strong (E). Amount 0 % = the original, untouched; Scale "
            "multiplies the processing (0 = none, 100 % = as planned, up to 400 %); Dyn/Avg: the dynamic tools follow "
            "the sound or hold their average (less CPU). The output always matches the input's loudness; Gain (centre = 0 dB) "
            "is your own trim after everything.",
}

# the view: the whole strip, one jsui
DEVICE_W, DEV_H = 706, 169
view = box("jsui", (40, 520, 300, 60), ins=1, outs=1, pres=(0, 0, DEVICE_W, DEV_H), filename="lifegiver.strip.js",
           parameter_enable=0, border=0, annotation=INFO["view"], annotation_name="Lifegiver")

# view → parameters ("p <name> <value>") or the engine (everything else)
vroute = obj("route p", (40, 600, 60, 22), ins=2, outs=2)
wire(view, 0, vroute, 0)
wire(vroute, 1, eng, 0)
proute = obj("route " + " ".join(NAMES), (40, 640, 900, 22), ins=2, outs=len(NAMES) + 1)
wire(vroute, 0, proute, 0)

# engine → [route getstate statelist param] → the pattr, the parameters; the rest → the view
eroute = obj("route getstate statelist param", (200, 400, 180, 22), ins=2, outs=4)
wire(eng, 2, eroute, 0)
tget = obj("t b", (200, 430, 30, 22), ins=1, outs=1, outtypes=["bang"])
wire(eroute, 0, tget, 0); wire(tget, 0, pattr, 0)
wire(eroute, 1, pattr, 0)
wire(eroute, 2, proute, 0)
wire(eroute, 3, view, 0)

bang_at_init = []
for i, (name, longn, short, ptype, lo, hi, init, ustyle, expo, enum) in enumerate(PARAMS):
    px, py = 40 + (i % 12) * 75, 700 + (i // 12) * 110
    d = box("live.dial", (px, py, 44, 48), ins=1, outs=2, outtypes=["", "float"], varname=longn, parameter_enable=1,
            saved_attribute_attributes=valueof(longn, short, ptype, lo, hi, init, ustyle, exponent=expo, enum=enum))
    wire(proute, i, d, 0)
    pre = obj(f"prepend {name}", (px, py + 55, 70, 22))
    wire(d, 0, pre, 0)
    wire(pre, 0, eng, 0)
    wire(pre, 0, view, 0)
    bang_at_init.append(d)

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
        "dependency_cache": [{"name": "lifegiver.strip.js", "bootpath": ".", "patcherrelativepath": ".", "type": "TEXT", "implicit": 1},
                             {"name": "lifegiver~.mxo", "type": "iLaX"}],
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
        if t.strip() == "dict":
            continue   # an unnamed dict gets its own unique name: private to this device
        if b["box"]["maxclass"] == "newobj" and re.match(r"^(send|s|receive|r|buffer~|dict|value|v|table)\b", t):
            assert " ---" in t, f"shared name without ---: {t}"
    longnames = [b["box"]["saved_attribute_attributes"]["valueof"]["parameter_longname"]
                 for b in boxes if b["box"].get("parameter_enable") == 1]
    assert len(longnames) == len(set(longnames)), f"duplicate Live parameter names: {longnames}"
    # a plain object (newobj) is in parameter mode only through saved_object_attributes (box level is ignored)
    for b in boxes:
        bb = b["box"]
        if bb["maxclass"] == "newobj" and bb.get("parameter_enable") == 1:
            assert bb.get("saved_object_attributes", {}).get("parameter_enable") == 1, f"not in parameter mode: {bb['text']}"
    # state (blob) parameters must have no initial value: an enabled one is recalled on load and wipes the state
    for b in boxes:
        v = b["box"].get("saved_attribute_attributes", {}).get("valueof", {})
        if b["box"].get("parameter_enable") == 1 and v.get("parameter_type") == 3:
            assert v["parameter_initial_enable"] == 0 and "parameter_initial" not in v, v["parameter_longname"]
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
