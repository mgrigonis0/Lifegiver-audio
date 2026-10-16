// lifegiver.strip.js - the whole Lifegiver device strip (jsui, Max 8 / ES5: no let, const, arrows, classes or
// template strings). Drawn after the approved mockup v2 (Listen . Tone . Dynamics . Stage . Moves . Out, 1116 x 169).
//
// in:  the Live parameters' values ("weight 2.5", "pan -0.3", "role 2", ...) and the engine's view messages (numbers):
//        takes  <n> <s1> <s2> <s3> <s4> <capture state 0..3> <kept s> <busy>
//        facts  <valid> <takes heard> <seconds> <low midi> <high midi> <harmonic 0..2> <sustained 0..2> <corr> <dots>
//               <reach -1|0..1> <stage dB> <compare 0/1> <takes in memory>
//        knobs  8 x (<word 0..4> <dots 1..3> <stage push> <adjustable> <reached> <asked>)
//        moves  n x (<kind> <value> <quality|-1> <reached> <asked>)
//        vars   <reach A..E (-1 = not yet)> <solo -1..4>
// out: "p <param> <value>" (a Live parameter: routed to it, it echoes back here and to the engine), and for the engine:
//        arm . removetake <i> . pickrole <r> . solo <v> . compare <0/1> . uiready

inlets = 1;
outlets = 1;
mgraphics.init();
mgraphics.relative_coords = 0;
mgraphics.autofill = 0;

var FONT = "Ableton Sans Medium", BOLD = "Ableton Sans Bold";
var C = {
    bg: [0.106, 0.106, 0.106], panel: [0.137, 0.137, 0.137], text: [0.84, 0.84, 0.84], bright: [0.93, 0.93, 0.93],
    dim: [0.55, 0.55, 0.55], dimmer: [0.33, 0.33, 0.33], track: [0.227, 0.227, 0.227], well: [0.102, 0.102, 0.102],
    btn: [0.2, 0.2, 0.2], floor: [0.11, 0.11, 0.11], line: [0.227, 0.227, 0.227],
    listen: [0.937, 0.353, 0.373], listenPale: [0.953, 0.655, 0.663], tone: [0.953, 0.604, 0.231], dyn: [0.29, 0.639, 0.961],
    stage: [0.31, 0.784, 0.753], stagePale: [0.561, 0.89, 0.863], moves: [0.655, 0.545, 0.98], movesPale: [0.788, 0.722, 1.0],
    out: [0.941, 0.541, 0.478], outPale: [0.965, 0.714, 0.671], range: [0.29, 0.639, 0.961], rangePale: [0.624, 0.816, 0.984],
    good: [0.498, 0.82, 0.659], warn: [0.953, 0.702, 0.42], ink: [0.06, 0.06, 0.07]
};
// layout (1x): the device is DEVICE_W x 169; panels side by side, 4 px apart. Everything below derives from SEC.
var Y = 4, H = 161, DEVICE_W = 756;
var SEC = { listen: [4, 72], knobs: [80, 140], stage: [224, 260], moves: [488, 180], out: [672, 80] };
var KR = 9.5;   // knob radius (the stage arc sits 3.5 px outside it)
function ix(sec) { return SEC[sec][0] + 6; }
function iw(sec) { return SEC[sec][1] - 12; }

var KN = ["weight", "warm", "bite", "air", "texture", "punch", "tail", "steady"];
var KL = ["Weight", "Warm", "Bite", "Air", "Texture", "Punch", "Tail", "Steady"];
var WORDS = [
    ["", "thin", "light", "full", "heavy"], ["", "lean", "even", "warm", "thick"], ["", "soft", "smooth", "clear", "edgy"],
    ["", "dull", "soft top", "open", "airy"], ["", "smooth", "fine", "grainy", "rough"], ["", "soft", "firm", "punchy", "hard"],
    ["", "short", "medium", "long", "held"], ["", "jumpy", "lively", "even", "steady"]
];
var ROLES = ["Lead", "Hook", "Support", "Rhythm", "Bass", "Bed", "FX"];
var VARS = ["A", "B", "C", "D", "E"], VNAMES = ["Polish", "Warm", "Bold", "Vintage", "Driven"];   // rising character
var MOVE = ["", "Lifting the lows", "Easing the low end", "Warming the low-mids", "Clearing low-mid mud",
    "Lifting presence", "Softening presence", "Lifting the air", "Darkening the top", "Tilting brighter", "Tilting darker",
    "Adding warm harmonics", "Adding edgy harmonics", "Steadying level", "Lowering what's between notes",
    "Sharpening attacks", "Softening attacks", "Lengthening sustain", "Shortening tails", "Clearing lows below",
    "Easing highs above", "Widening, lows kept mono", "Narrowing", "Stereo pan", "Level matched to the input", "Set back"];
var NOTE = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];

// the Live parameters (as last heard from them)
var P = { weight: 0, warm: 0, bite: 0, air: 0, texture: 0, punch: 0, tail: 0, steady: 0,
    pan: 0, depth: 0.82, width: 0.35, edge: 0, lo: 60, hi: 20000, role: 0, fit: 0.5, amount: 100, clean: 40,
    vara: 0, varb: 0, varc: 0, vard: 0, vare: 0 };
var LIMITS = { pan: [-1, 1], depth: [0, 1], width: [0, 1], edge: [0, 1], lo: [20, 20000], hi: [20, 20000], fit: [0, 1],
    amount: [0, 100], clean: [0, 100], vara: [0, 100], varb: [0, 100], varc: [0, 100], vard: [0, 100], vare: [0, 100] };
// what the engine said
var T = { n: 0, secs: [0, 0, 0, 0], cap: 0, kept: 0, busy: 0 };
var F = { valid: 0, heard: 0, sec: 0, lo: 0, hi: 0, harm: 0, sus: 0, corr: 1, dots: 1, reach: -1, stageDb: 0, compare: 0, live: 0 };
var K = [];
for (var i0 = 0; i0 < 8; i0++) K.push({ word: 0, dots: 1, push: 0, adj: 0, reached: 0, asked: 0 });
var MV = [];
var VR = [-1, -1, -1, -1, -1], SOLO = -1;
var drag = null;
var MPAGE = 0;   // which 5 Moves show (click the list's last line to page)

// ------------------------------------------------------------------------------------------------- drawing helpers
function col(c, a) { mgraphics.set_source_rgba(c[0], c[1], c[2], a === undefined ? 1 : a); }
function box(x, y, w, h, r, c, a) { col(c, a); if (r > 0) mgraphics.rectangle_rounded(x, y, w, h, r * 2, r * 2); else mgraphics.rectangle(x, y, w, h); mgraphics.fill(); }
function frame(x, y, w, h, r, c, a, lw) { col(c, a); mgraphics.set_line_width(lw || 1); mgraphics.rectangle_rounded(x + 0.5, y + 0.5, w - 1, h - 1, r * 2, r * 2); mgraphics.stroke(); }
function font(size, bold) { mgraphics.select_font_face(bold ? BOLD : FONT); mgraphics.set_font_size(size); }
function tw(s) { return mgraphics.text_measure(s)[0]; }
// text with alignment: 0 left, 1 centre, 2 right; clipped with "..." to maxW
function text(s, x, y, size, c, bold, align, maxW, a) {
    font(size, bold);
    s = "" + s;
    if (maxW && tw(s) > maxW) {
        while (s.length > 1 && tw(s + "\u2026") > maxW) s = s.substring(0, s.length - 1);
        s = s + "\u2026";
    }
    var w = tw(s);
    var xx = align === 1 ? x - w / 2 : (align === 2 ? x - w : x);
    col(c, a);
    mgraphics.move_to(xx, y);
    mgraphics.show_text(s);
}
function line(x1, y1, x2, y2, c, w, a) { col(c, a); mgraphics.set_line_width(w); mgraphics.move_to(x1, y1); mgraphics.line_to(x2, y2); mgraphics.stroke(); }
function rad(deg) { return (deg - 90) * Math.PI / 180; }   // 0 deg = up, clockwise
function arc(cx, cy, r, d1, d2, c, w, a) {
    if (Math.abs(d2 - d1) < 0.5) return;
    col(c, a);
    mgraphics.set_line_width(w);
    mgraphics.new_path();
    mgraphics.arc(cx, cy, r, rad(Math.min(d1, d2)), rad(Math.max(d1, d2)));
    mgraphics.stroke();
}
function dot(cx, cy, r, c, a) { col(c, a); mgraphics.new_path(); mgraphics.arc(cx, cy, r, 0, 2 * Math.PI); mgraphics.fill(); }
function ring(cx, cy, r, c, w, a) { col(c, a); mgraphics.set_line_width(w); mgraphics.new_path(); mgraphics.arc(cx, cy, r, 0, 2 * Math.PI); mgraphics.stroke(); }
function dots(x, y, n, c) { for (var k = 0; k < 3; k++) { if (k < n) dot(x + k * 5, y, 1.6, c); else ring(x + k * 5, y, 1.4, c, 0.8, 0.8); } }
function header(sec, title, c, right, rc) {
    var x = SEC[sec][0] + 6, w = SEC[sec][1] - 12;
    text(title, x, Y + 14, 9.5, c, true, 0);
    if (right) text(right, x + w, Y + 14, 7.5, rc || C.dim, false, 2, w - tw(title) - 8);
    box(x, Y + 18, w, 2, 0, c, 0.6);
}
function panel(sec) { box(SEC[sec][0], Y, SEC[sec][1], H, 4, C.panel); }
function clamp(v, a, b) { return v < a ? a : (v > b ? b : v); }
function sgn(v, d) { var r = Math.round(v * 10) / 10; if (Math.abs(r) < 0.05) return "0"; return (r > 0 ? "+" : "-") + Math.abs(r).toFixed(d === undefined ? 1 : d); }
function fmtHz(hz) { return hz < 1000 ? "" + Math.round(hz / 5) * 5 : (Math.round(hz / 100) / 10) + "k"; }
function noteName(m) { if (m <= 0) return ""; var r = Math.round(m); return NOTE[((r % 12) + 12) % 12] + (Math.floor(r / 12) - 1); }

// ------------------------------------------------------------------------------------------------- paint
function paint() {
    box(0, 0, DEVICE_W, 169, 0, C.bg);
    paintListen();
    paintKnobs();
    paintStage();
    paintMoves();
    paintOut();
}

function armLabel() {
    if (T.cap === 1) return "Play\u2026";
    if (T.cap === 2) return T.kept.toFixed(1) + " s";
    return T.n >= 4 ? "Again" : "Arm";
}
function armRect() { return [ix("listen"), 26, iw("listen"), 20]; }
function takeRect(i) { var w = (iw("listen") - 2) / 2; return [ix("listen") + (i % 2) * (w + 2), 50 + Math.floor(i / 2) * 15, w, 13]; }

function paintListen() {
    panel("listen");
    header("listen", "LISTEN", C.listen, T.busy ? "\u2026" : "", C.listenPale);
    var r = armRect(), on = T.cap === 1 || T.cap === 2;
    box(r[0], r[1], r[2], r[3], 10, on ? C.listen : C.panel);
    frame(r[0], r[1], r[2], r[3], 10, C.listen);
    text(armLabel(), r[0] + r[2] / 2, r[1] + 14, 9.5, on ? [0.16, 0.06, 0.06] : C.listenPale, true, 1, r[2] - 4);
    for (var i = 0; i < 4; i++) {
        var tk = takeRect(i), has = i < T.n;
        box(tk[0], tk[1], tk[2], tk[3], 2, has ? [0.227, 0.145, 0.149] : [0.165, 0.165, 0.165]);
        text(has ? (i + 1) + "  x" : "\u2014", tk[0] + tk[2] / 2, tk[1] + 9.5, 7.5, has ? C.listenPale : [0.33, 0.33, 0.33], false, 1, tk[2] - 2);
    }
    var L = factLines(), x = ix("listen"), w = iw("listen");
    for (var k = 0; k < L.length; k++) text(L[k], x, 91 + k * 10.5, 7.5, C.dim, false, 0, w);
    if (F.valid && T.cap === 0) dots(x + 2, 91 + L.length * 10.5 - 3, F.dots, C.dim);
}

function factLines() {
    if (T.cap === 1) return ["Play the", "sound now."];
    if (T.cap === 2) return ["Keep playing.", "Stops by itself."];
    if (!F.valid) return ["No takes yet.", "Press Arm,", "then play."];
    var a = [];
    var pitch = F.lo > 0 ? (noteName(F.lo) === noteName(F.hi) ? noteName(F.lo) : noteName(F.lo) + "\u2013" + noteName(F.hi)) : "no pitch";
    a.push(F.live > 0 ? F.live + " take" + (F.live > 1 ? "s" : "") : "saved reading");
    a.push(pitch);
    a.push(["noisy", "part pitched", "harmonic"][F.harm]);
    a.push(["plucked", "medium decay", "sustained"][F.sus]);
    a.push("stereo " + F.corr.toFixed(1));
    if (F.live === 0) a.push("re-listen: refine");
    return a;
}

function knobCell(i) {
    var cw = iw("knobs") / 4;
    return { x: ix("knobs") + cw * (i % 4), y: 26 + Math.floor(i / 4) * 67, w: cw, h: 66 };
}

function paintKnob(cx, cy, val, stagePush, c, active) {
    var ua = clamp(val, -10, 10) / 10 * 135, sa = clamp(stagePush, -10, 10) / 10 * 135;
    arc(cx, cy, KR, -135, 135, C.track, 2.5, active ? 1 : 0.6);
    if (active) arc(cx, cy, KR, 0, ua, c, 2.5);
    if (active && Math.abs(sa) >= 0.5) arc(cx, cy, KR + 3.5, 0, sa, C.stage, 1.4);
    var a = rad(ua);
    line(cx, cy, cx + (KR - 2) * Math.cos(a), cy + (KR - 2) * Math.sin(a), C.bright, 1.5, active ? 1 : 0.4);
}

function paintKnobs() {
    panel("knobs");
    var x = ix("knobs"), w = iw("knobs");
    text("TONE", x, Y + 14, 9.5, C.tone, true, 0);
    var tws = tw("TONE");
    text(" \u00b7 ", x + tws, Y + 14, 9.5, [0.33, 0.33, 0.33], true, 0);
    text("DYNAMICS", x + tws + tw(" \u00b7 "), Y + 14, 9.5, C.dyn, true, 0);
    box(x, Y + 18, w / 2, 2, 0, C.tone, 0.6);
    box(x + w / 2, Y + 18, w / 2, 2, 0, C.dyn, 0.6);
    var ready = F.valid;
    for (var i = 0; i < 8; i++) {
        var c = knobCell(i), cx = c.x + c.w / 2, k = K[i], active = ready && k.adj;
        var alpha = ready ? (k.adj ? 1 : 0.45) : 0.3;
        text(KL[i], cx, c.y + 9, 8, [0.86, 0.86, 0.86], true, 1, c.w, alpha);
        paintKnob(cx, c.y + 27, P[KN[i]], k.push, i < 5 ? C.tone : C.dyn, active);
        text(P[KN[i]] === 0 ? "0" : sgn(P[KN[i]]), cx, c.y + 48, 8.5, C.bright, false, 1, c.w, alpha);
        // the reading in a word, how sure (dots); the stage's and the characters' ask is the teal arc
        if (!ready) continue;
        text(k.word ? WORDS[i][k.word] : "\u2014", cx, c.y + 57, 7, C.dim, false, 1, c.w, alpha);
        if (k.adj) dots(cx - 5, c.y + 62, k.dots, C.dim);
    }
}

// ---- the stage (the mockup's geometry in its 268 x 107 local units, drawn KX times as wide)
var SW = 166, SH = 104, RW = 40, SY = 26, KX = SW / 268, KY = SH / 107;
function stageX() { return ix("stage") + 42; }
function rangeX() { return stageX() + SW + 3; }
function sx(lx) { return stageX() + lx * KX; }
function sy(ly) { return SY + ly * KY; }
function geom() {
    var depth = P.depth, scale = 0.62 + 0.38 * depth, cy = 18 + depth * 70, half = 76 + 54 * depth;
    var cx = 134 + P.pan * half * 0.8;
    return { scale: scale, cy: cy, cx: cx, rx: (7 + P.width * 56) * scale, ry: 7 * scale };
}
function hzToY(hz) { return 103 - (Math.log(hz) / Math.LN10 - Math.log(20) / Math.LN10) / 3 * 99; }
function yToHz(y) { return Math.pow(10, Math.log(20) / Math.LN10 + (103 - y) / 99 * 3); }

function stagePath(g) {
    mgraphics.new_path();
    var N = 36;
    for (var i = 0; i <= N; i++) {
        var a = 2 * Math.PI * (i % N) / N;
        var k = 1 + ((i % N) % 2 === 0 ? 1 : -1) * P.edge * 0.3 + 0.03 * Math.sin(3 * a);
        var px = sx(g.cx + Math.cos(a) * g.rx * k), py = sy(g.cy + Math.sin(a) * g.ry * k);
        if (i === 0) mgraphics.move_to(px, py); else mgraphics.line_to(px, py);
    }
    mgraphics.close_path();
}

function stageWords() {
    var lvl = -(1 - P.depth) * 3;
    return (P.depth > 0.66 ? "front" : (P.depth < 0.33 ? "back, darker" : "middle")) +
        " \u00b7 " + (P.width < 0.3 ? "narrow" : (P.width > 0.7 ? "wide" : "medium")) +
        " \u00b7 " + fmtHz(P.lo) + "\u2013" + fmtHz(P.hi) +
        " \u00b7 " + (P.edge < 0.2 ? "smooth" : (P.edge > 0.6 ? "driven" : "some drive")) +
        " \u00b7 " + (lvl > -0.05 ? "0 dB" : lvl.toFixed(1) + " dB");
}

function paintStage() {
    panel("stage");
    var panTxt = Math.abs(P.pan) < 0.05 ? "C" : (P.pan < 0 ? "L" : "R") + Math.round(Math.abs(P.pan) * 100);
    header("stage", "STAGE", C.stage, ROLES[P.role] + " \u00b7 " + panTxt);
    var a = F.valid ? 1 : 0.3, x0 = ix("stage"), RX = rangeX();
    for (var r = 0; r < 7; r++) {
        var on = P.role === r;
        box(x0, 26 + r * 15, 39, 13, 2, on ? C.stage : [0.2, 0.2, 0.2], a);
        text(ROLES[r], x0 + 19.5, 26 + r * 15 + 9.5, 7.5, on ? [0.063, 0.125, 0.122] : [0.74, 0.74, 0.74], true, 1, 37, a);
    }
    // the stage, seen from above: back wall at the top, the listener at the bottom
    col(C.floor, a);
    mgraphics.new_path();
    mgraphics.move_to(sx(58), sy(4)); mgraphics.line_to(sx(210), sy(4)); mgraphics.line_to(sx(264), sy(103)); mgraphics.line_to(sx(4), sy(103));
    mgraphics.close_path();
    mgraphics.fill_preserve();
    col(C.line, a); mgraphics.set_line_width(1); mgraphics.stroke();
    line(sx(134), sy(4), sx(134), sy(103), [0.17, 0.17, 0.17], 1, a);
    line(sx(44), sy(30), sx(224), sy(30), [0.153, 0.153, 0.153], 1, a);
    line(sx(30), sy(56), sx(238), sy(56), [0.153, 0.153, 0.153], 1, a);
    line(sx(17), sy(80), sx(251), sy(80), [0.153, 0.153, 0.153], 1, a);
    text("BACK", sx(134), sy(11), 5.5, [0.37, 0.37, 0.37], false, 1, 0, a);
    var g = geom();
    stagePath(g);
    col(C.stage, (0.3 + 0.5 * P.depth) * a);
    mgraphics.fill_preserve();
    col(C.stagePale, a); mgraphics.set_line_width(1); mgraphics.stroke();
    dot(sx(g.cx), sy(g.cy), 1.4, [0.91, 1, 0.99], a);
    var top = g.cy - g.ry * (1 + P.edge * 0.3), edgeY = top - 5 - P.edge * 14;
    line(sx(g.cx), sy(top), sx(g.cx), sy(edgeY), C.stagePale, 0.7, a);
    col(C.floor, a);
    mgraphics.new_path();
    mgraphics.move_to(sx(g.cx), sy(edgeY) - 3.5); mgraphics.line_to(sx(g.cx) + 3.5, sy(edgeY));
    mgraphics.line_to(sx(g.cx), sy(edgeY) + 3.5); mgraphics.line_to(sx(g.cx) - 3.5, sy(edgeY)); mgraphics.close_path();
    mgraphics.fill_preserve(); col(C.stagePale, a); mgraphics.set_line_width(1); mgraphics.stroke();
    dot(sx(g.cx - g.rx), sy(g.cy), 3, C.floor, a); ring(sx(g.cx - g.rx), sy(g.cy), 3, C.stagePale, 1, a);
    dot(sx(g.cx + g.rx), sy(g.cy), 3, C.floor, a); ring(sx(g.cx + g.rx), sy(g.cy), 3, C.stagePale, 1, a);
    // the range it owns: lows at the bottom, highs at the top
    var bx = RX + 13;
    box(bx, sy(4), 9, 99 * KY, 2, C.floor, a);
    frame(bx, sy(4), 9, 99 * KY, 2, C.line, a);
    var rt = sy(hzToY(P.hi)), rb = sy(hzToY(P.lo));
    box(bx, rt, 9, Math.max(2, rb - rt), 2, C.range, 0.55 * a);
    text("10k", bx - 2, sy(hzToY(10000)) + 2, 5.5, [0.44, 0.44, 0.44], false, 2, 0, a);
    text("1k", bx - 2, sy(hzToY(1000)) + 2, 5.5, [0.44, 0.44, 0.44], false, 2, 0, a);
    text("100", bx - 2, sy(hzToY(100)) + 2, 5.5, [0.44, 0.44, 0.44], false, 2, 0, a);
    box(bx - 2, rt - 2, 13, 4, 1.5, C.floor, a); frame(bx - 2, rt - 2, 13, 4, 1.5, C.rangePale, a);
    box(bx - 2, rb - 2, 13, 4, 1.5, C.floor, a); frame(bx - 2, rb - 2, 13, 4, 1.5, C.rangePale, a);
    text(fmtHz(P.hi), bx + 12, rt + 2.5, 6, [0.81, 0.9, 0.98], false, 0, 0, a);
    text(fmtHz(P.lo), bx + 12, rb + 2.5, 6, [0.81, 0.9, 0.98], false, 0, 0, a);
    text(stageWords(), x0, SY + SH + 11, 7.5, [0.66, 0.66, 0.66], false, 0, iw("stage"), a);
    if (!F.valid) text("Listen first", SEC.stage[0] + SEC.stage[1] / 2, Y + 62, 11, C.listen, true, 1);
}

function moveText(m) {
    var s = MOVE[m[0]] || "";
    if (m[0] === 19) s += " " + fmtHz(m[1]);
    if (m[0] === 20) s += " " + fmtHz(m[1]);
    if (m[0] === 13) s += m[1] >= 250 ? ", slow" : (m[1] <= 60 ? ", fast" : "");
    if (m[0] === 23) s += m[1] < 0 ? " L" + Math.round(-m[1] * 100) : " R" + Math.round(m[1] * 100);
    return s;
}
function moveResult(m) {
    if (m[2] >= 0) return sgn(m[3]) + "/" + sgn(m[4]);
    if (m[0] === 19 || m[0] === 20) return "range";
    if (m[0] === 21) return "mono-safe";
    if (m[0] === 22) return "narrower";
    if (m[0] === 23) return "pan";
    if (m[0] === 24 || m[0] === 25) return sgn(m[1]) + " dB";
    return "";
}
function movePages() { return Math.max(1, Math.ceil(MV.length / 5)); }
function varCell(v) { var cw = (iw("moves") - 8) / 5; return { x: ix("moves") + v * (cw + 2), w: cw }; }

function paintMoves() {
    panel("moves");
    var right = F.reach >= 0 && F.valid ? (F.reach >= 0.9 ? "hits goal" : Math.round(F.reach * 100) + " %") : "";
    header("moves", "MOVES", C.moves, right, F.reach >= 0.9 ? C.good : C.warn);
    var x = ix("moves"), w = iw("moves"), a = F.valid ? 1 : 0.3;
    if (MPAGE >= movePages()) MPAGE = 0;
    for (var i = 0; i < 5; i++) {
        var j = MPAGE * 5 + i;
        if (j >= MV.length) break;
        var y = 26 + i * 12, res = moveResult(MV[j]);
        font(7.5, false);
        var rw = tw(res);
        text(moveText(MV[j]), x, y + 8.5, 7.5, [0.86, 0.86, 0.86], false, 0, w - rw - 4, a);
        text(res, x + w, y + 8.5, 7.5, C.moves, false, 2, 0, a);
        box(x, y + 11, w, 1, 0, [0.17, 0.17, 0.17], a);
    }
    if (!F.valid) text("After Listen.", x, 35, 7.5, C.dim, false, 0, w);
    else if (!MV.length) text(T.busy ? "Planning\u2026" : "Nothing to do.", x, 35, 7.5, C.dim, false, 0, w);
    if (movePages() > 1) text((MPAGE + 1) + "/" + movePages() + "  more \u203a", x + w, 95, 7, C.movesPale, false, 2, w);
    for (var v = 0; v < 5; v++) {
        var c = varCell(v), pct = P["var" + "abcde".charAt(v)], solo = SOLO === v;
        box(c.x, 100, c.w, 14, 2, pct > 0 ? C.moves : [0.2, 0.2, 0.2], a);
        if (solo) frame(c.x, 100, c.w, 14, 2, C.movesPale, a, 1.5);
        text(VARS[v], c.x + c.w / 2, 110.5, 8.5, pct > 0 ? [0.106, 0.082, 0.188] : [0.74, 0.74, 0.74], true, 1, c.w, a);
        box(c.x, 117, c.w, 5, 2.5, C.well, a);
        if (pct > 0) box(c.x, 117, c.w * pct / 100, 5, 2.5, C.moves, a);
        text(VNAMES[v], c.x + c.w / 2, 131, 6.5, [0.6, 0.6, 0.6], false, 1, c.w + 1, a);
        text(solo ? "solo" : Math.round(pct) + "%", c.x + c.w / 2, 141, 6.5, [0.6, 0.6, 0.6], false, 1, c.w, a);
        if (VR[v] >= 0) text(VR[v] >= 0.9 ? "hits" : Math.round(VR[v] * 100) + "%", c.x + c.w / 2, 151, 6.5, VR[v] >= 0.9 ? C.good : C.warn, false, 1, c.w, a);
    }
    text("A subtle \u2192 E strong", x + w / 2, 161, 6, [0.42, 0.42, 0.42], false, 1, w, a);
}

function outKnob(i) { var x = ix("out"), w = iw("out"); return { cx: x + w * (i ? 0.75 : 0.25), cy: 74, name: i ? "Clean" : "Amount", p: i ? "clean" : "amount" }; }
function fitRect() { return [ix("out"), 32, iw("out"), 14]; }
function compareRect() { return [ix("out"), 112, iw("out"), 17]; }

function paintOut() {
    panel("out");
    header("out", "OUT", C.out);
    var x = ix("out"), w = iw("out");
    text("Identity", x, 34, 7, [0.74, 0.74, 0.74], false, 0);
    text("Fit", x + w, 34, 7, [0.74, 0.74, 0.74], false, 2);
    box(x, 38, w, 5, 2.5, C.well);
    box(x + P.fit * w - 2, 36, 4, 9, 2, C.out);
    for (var i = 0; i < 2; i++) {
        var k = outKnob(i), v = P[k.p], ang = -135 + 270 * v / 100;
        text(k.name, k.cx, 60, 7.5, [0.86, 0.86, 0.86], true, 1, w / 2 + 2);
        arc(k.cx, k.cy + 4, KR, -135, 135, C.track, 2.5);
        arc(k.cx, k.cy + 4, KR, -135, ang, C.out, 2.5);
        var aa = rad(ang);
        line(k.cx, k.cy + 4, k.cx + (KR - 2) * Math.cos(aa), k.cy + 4 + (KR - 2) * Math.sin(aa), C.bright, 1.5);
        text(Math.round(v) + "%", k.cx, k.cy + 24, 8, C.bright, false, 1);
    }
    var on = F.compare, r = compareRect();
    box(r[0], r[1], r[2], r[3], 8.5, on ? C.out : C.panel);
    frame(r[0], r[1], r[2], r[3], 8.5, C.out);
    text(on ? "Before" : "Compare", x + w / 2, r[1] + 12, 8.5, on ? [0.165, 0.078, 0.067] : C.outPale, true, 1, w - 4);
    text("level-matched", x + w / 2, 143, 6.5, C.dim, false, 1, w);
}

// ------------------------------------------------------------------------------------------------- input
function inRect(x, y, r) { return x >= r[0] && x < r[0] + r[2] && y >= r[1] && y < r[1] + r[3]; }
function setP(name, v) {
    var lim = LIMITS[name];
    if (lim) v = clamp(v, lim[0], lim[1]);
    P[name] = v;
    outlet(0, "p", name, v);
    mgraphics.redraw();
}
function outKnobHit(x, y, o) { var ok = outKnob(o); return Math.abs(x - ok.cx) < iw("out") / 4 && y >= 50 && y < 106; }

function onclick(x, y, but, cmd, shift, capslock, option, ctrl) {
    drag = null;
    // Listen
    if (inRect(x, y, armRect())) { outlet(0, "arm"); return; }
    for (var i = 0; i < 4; i++) if (inRect(x, y, takeRect(i)) && i < T.n) { outlet(0, "removetake", i); return; }
    // knobs (vertical drag, like Live's; shift = fine)
    for (var k = 0; k < 8; k++) {
        var c = knobCell(k);
        if (inRect(x, y, [c.x, c.y, c.w, c.h])) { if (F.valid) drag = { kind: "knob", name: KN[k], y0: y, v0: P[KN[k]], lo: -10, hi: 10, per: 0.12 }; return; }
    }
    if (F.valid) {
        // roles
        for (var r = 0; r < 7; r++) if (inRect(x, y, [ix("stage"), 26 + r * 15, 39, 13])) { outlet(0, "pickrole", r); return; }
        // stage handles, then the shape itself (hit areas in screen pixels)
        var RX = rangeX(), g = geom(), ly = (y - SY) / KY;
        var top = g.cy - g.ry * (1 + P.edge * 0.3), edgeY = top - 5 - P.edge * 14;
        if (Math.abs(x - sx(g.cx)) <= 5 && Math.abs(y - sy(edgeY)) <= 5) { drag = { kind: "edge" }; return; }
        if (Math.abs(y - sy(g.cy)) <= 5 && (Math.abs(x - sx(g.cx - g.rx)) <= 5 || Math.abs(x - sx(g.cx + g.rx)) <= 5)) { drag = { kind: "width" }; return; }
        if (x >= stageX() && x < stageX() + SW && y >= SY && y < SY + SH) { drag = { kind: "move" }; ondrag(x, y, 1); return; }
        if (x >= RX && x < RX + RW && y >= SY && y < SY + SH) {
            var dh = Math.abs(ly - hzToY(P.hi)), dl = Math.abs(ly - hzToY(P.lo));
            drag = { kind: dh < dl ? "hi" : "lo" };
            ondrag(x, y, 1);
            return;
        }
        // Moves: the list pages through when there are more than fit
        if (inRect(x, y, [ix("moves"), 26, iw("moves"), 72]) && movePages() > 1) { MPAGE = (MPAGE + 1) % movePages(); mgraphics.redraw(); return; }
        // characters: the letter auditions it alone (Solo), the bar sets its strength
        for (var v = 0; v < 5; v++) {
            var vc = varCell(v);
            if (inRect(x, y, [vc.x, 100, vc.w, 14])) { outlet(0, "solo", v); return; }
            if (inRect(x, y, [vc.x, 114, vc.w, 11])) { drag = { kind: "bar", name: "var" + "abcde".charAt(v), x0: vc.x, w: vc.w, scale: 100, step: 5 }; ondrag(x, y, 1); return; }
        }
    }
    // out
    var fr = fitRect();
    if (inRect(x, y, fr)) { drag = { kind: "bar", name: "fit", x0: fr[0], w: fr[2], scale: 1, step: 0 }; ondrag(x, y, 1); return; }
    for (var o = 0; o < 2; o++) {
        if (outKnobHit(x, y, o)) { var ok = outKnob(o); drag = { kind: "knob", name: ok.p, y0: y, v0: P[ok.p], lo: 0, hi: 100, per: 0.6 }; return; }
    }
    if (inRect(x, y, compareRect())) { outlet(0, "compare", F.compare ? 0 : 1); return; }
}

function ondrag(x, y, but, cmd, shift, capslock, option, ctrl) {
    if (!drag) return;
    if (!but) { drag = null; return; }
    var d = drag;
    if (d.kind === "knob") {
        var v = d.v0 + (d.y0 - y) * d.per * (shift ? 0.25 : 1);
        v = d.per < 0.5 ? Math.round(v * 10) / 10 : Math.round(v);
        setP(d.name, clamp(v, d.lo, d.hi));
        return;
    }
    if (d.kind === "bar") {
        var f = clamp((x - d.x0) / d.w, 0, 1) * d.scale;
        if (d.step) f = Math.round(f / d.step) * d.step;
        setP(d.name, f);
        return;
    }
    var lx = (x - stageX()) / KX, ly = (y - SY) / KY, g = geom();
    if (d.kind === "move") {
        var depth = clamp((ly - 18) / 70, 0, 1), half = 76 + 54 * depth;
        P.depth = depth;
        setP("depth", depth);
        setP("pan", clamp((lx - 134) / (half * 0.8), -1, 1));
    } else if (d.kind === "width") setP("width", clamp((Math.abs(lx - g.cx) / g.scale - 7) / 56, 0, 1));
    else if (d.kind === "edge") setP("edge", clamp((g.cy - g.ry - 5 - ly) / 18, 0, 1));
    else if (d.kind === "hi") setP("hi", Math.max(clamp(yToHz(ly), 20, 20000), P.lo * 2));
    else if (d.kind === "lo") setP("lo", Math.min(clamp(yToHz(ly), 20, 20000), P.hi / 2));
}

function ondblclick(x, y) {
    for (var k = 0; k < 8; k++) { var c = knobCell(k); if (inRect(x, y, [c.x, c.y, c.w, c.h]) && F.valid) { setP(KN[k], 0); return; } }
    for (var o = 0; o < 2; o++) if (outKnobHit(x, y, o)) { setP(outKnob(o).p, o ? 40 : 100); return; }
}
// ------------------------------------------------------------------------------------------------- messages
function anything() {
    var a = arrayfromargs(arguments), m = messagename;
    if (P.hasOwnProperty(m)) { if (a.length) P[m] = m === "role" ? Math.round(a[0]) : a[0]; mgraphics.redraw(); return; }
    if (m === "takes") {
        T.n = a[0] | 0;
        for (var i = 0; i < 4; i++) T.secs[i] = a[1 + i] || 0;
        T.cap = a[5] | 0; T.kept = a[6] || 0; T.busy = a[7] | 0;
    } else if (m === "facts") {
        F.valid = a[0] | 0; F.heard = a[1] | 0; F.sec = a[2]; F.lo = a[3]; F.hi = a[4]; F.harm = clamp(a[5] | 0, 0, 2);
        F.sus = clamp(a[6] | 0, 0, 2); F.corr = a[7]; F.dots = clamp(a[8] | 0, 1, 3); F.reach = a[9]; F.stageDb = a[10];
        F.compare = a[11] | 0; F.live = a[12] | 0;
    } else if (m === "knobs") {
        for (var k = 0; k < 8 && 6 * k + 5 < a.length; k++)
            K[k] = { word: clamp(a[6 * k] | 0, 0, 4), dots: clamp(a[6 * k + 1] | 0, 1, 3), push: a[6 * k + 2], adj: a[6 * k + 3] | 0, reached: a[6 * k + 4], asked: a[6 * k + 5] };
    } else if (m === "moves") {
        MV = [];
        MPAGE = 0;
        for (var j = 0; j + 4 < a.length; j += 5) MV.push([a[j] | 0, a[j + 1], a[j + 2] | 0, a[j + 3], a[j + 4]]);
    } else if (m === "vars") {
        for (var v = 0; v < 5; v++) VR[v] = a.length > v ? a[v] : -1;
        SOLO = a.length > 5 ? (a[5] | 0) : -1;
    } else return;
    mgraphics.redraw();
}
function bang() { mgraphics.redraw(); }
function loadbang() { outlet(0, "uiready"); }
