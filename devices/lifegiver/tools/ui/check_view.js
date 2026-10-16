// Offline check of the jsui view (Node): ES5 only (Max 8's JS engine), loads without errors, paints every state,
// answers clicks and drags with the right messages, and records the drawing so render_view.py can turn it into a PNG.
//   node check_view.js <view.js> <out dir>
"use strict";
const fs = require("fs");
const vm = require("vm");
const path = require("path");

const file = process.argv[2], outDir = process.argv[3] || ".";
const src = fs.readFileSync(file, "utf8");
let failures = 0;
function expect(ok, what) { console.log(`  [${ok ? " ok " : "FAIL"}] ${what}`); if (!ok) failures++; }

// ---- ES5: strip strings and comments, then look for later syntax
function stripCode(s) {
    let out = "", i = 0;
    while (i < s.length) {
        const c = s[i], n = s[i + 1];
        if (c === "/" && n === "/") { while (i < s.length && s[i] !== "\n") i++; continue; }
        if (c === "/" && n === "*") { i += 2; while (i < s.length && !(s[i] === "*" && s[i + 1] === "/")) i++; i += 2; continue; }
        if (c === '"' || c === "'") { const q = c; out += q; i++; while (i < s.length && s[i] !== q) { if (s[i] === "\\") i++; i++; } out += q; i++; continue; }
        out += c; i++;
    }
    return out;
}
const code = stripCode(src);
const banned = [[/\blet\s/, "let"], [/\bconst\s/, "const"], [/=>/, "arrow functions"], [/`/, "template strings"], [/\bclass\s/, "classes"],
    [/\.\.\./, "spread"], [/\bof\s/, "for…of"], [/function\s*\w*\s*\([^)]*=/, "default parameters"]];
const found = banned.filter(b => b[0].test(code)).map(b => b[1]);
expect(found.length === 0, "ES5 only (no " + (found.join(", ") || "later syntax") + ")");
expect(!/[^\x00-\x7f]/.test(src), "plain ASCII (non-ASCII text is written as \\u escapes: Max reads the file's bytes as they are)");

// ---- a mock Max: mgraphics records every call
function makeMax() {
    const ops = [], sent = [];
    const mg = { size: [1116, 169], relative_coords: 0, autofill: 0, _font: 10 };
    const rec = name => (...a) => { ops.push([name, ...a]); };
    for (const f of ["init", "set_source_rgba", "rectangle", "rectangle_rounded", "fill", "fill_preserve", "stroke", "stroke_preserve",
        "set_line_width", "move_to", "line_to", "arc", "close_path", "new_path", "show_text", "select_font_face", "redraw", "set_line_cap"])
        mg[f] = rec(f);
    mg.set_font_size = s => { mg._font = s; ops.push(["set_font_size", s]); };
    mg.text_measure = s => [String(s).length * mg._font * 0.52, mg._font];
    const ctx = {
        mgraphics: mg, inlets: 1, outlets: 1, post: () => {}, messagename: "",
        outlet: (i, ...a) => sent.push(a.join(" ")),
        arrayfromargs: args => Array.prototype.slice.call(args),
        Math, String, Object, Array,
    };
    vm.createContext(ctx);
    vm.runInContext(src, ctx, { filename: path.basename(file) });
    return { ctx, ops, sent, mg };
}
function msg(m, name, ...args) { m.ctx.messagename = name; m.ctx.anything(...args); }
function paintOps(m) { m.ops.length = 0; m.ctx.paint(); return m.ops.slice(); }

let m;
try { m = makeMax(); expect(true, "loads"); } catch (e) { expect(false, "loads: " + e.message); process.exit(1); }

// state 1: a fresh device (Listen first)
let ok = true;
try { fs.writeFileSync(path.join(outDir, "view_fresh.json"), JSON.stringify(paintOps(m))); } catch (e) { ok = false; console.log(e); }
expect(ok, "paints a fresh device (Listen first)");

// state 2: listening
msg(m, "takes", 0, 0, 0, 0, 0, 2, 2.3, 0);
expect(paintOps(m).some(o => o[0] === "show_text" && /Listening/.test(o[1])), "paints Listening… with the seconds kept");

// state 3: a reading, moves and variations (as the engine sends them)
msg(m, "takes", 2, 4, 4, 0, 0, 0, 0, 0);
msg(m, "facts", 1, 2, 7.7, 57, 76, 2, 2, 0.51, 3, 0.82, -0.5, 0, 2);
const kv = [];
for (let q = 0; q < 8; q++) kv.push(1 + (q % 4), 1 + (q % 3), [0, 0, 1.7, 1.7, 0, 1, -1, 0.6][q], q === 0 ? 0 : 1, 0, 0);
msg(m, "knobs", ...kv);
msg(m, "moves", 5, 0.9, 2, 1.5, 1.7, 13, 141, 7, 0.6, 0.6, 15, 0.14, 5, 0.9, 1.0, 19, 62, -1, 0, 0, 22, 0.77, -1, 0, 0, 24, 0.6, -1, 0, 0);
msg(m, "vars", 0.8, 0.63, 0.73, 0.54, 0.88, -1);
for (const [k, v] of [["bite", 2], ["steady", -1], ["role", 0], ["depth", 0.82], ["width", 0.35], ["edge", 0.25], ["lo", 110], ["hi", 9000]]) msg(m, k, v);
const ready = paintOps(m);
fs.writeFileSync(path.join(outDir, "view_ready.json"), JSON.stringify(ready));
expect(ready.some(o => o[0] === "show_text" && /Lifting presence/.test(o[1])), "paints the Moves from numbers (no text from the engine)");
expect(ready.some(o => o[0] === "show_text" && /A3/.test(o[1])), "paints the facts (notes A3 … E5 from MIDI numbers)");
expect(!ready.some(o => o[0] === "show_text" && /undefined|NaN/.test(o[1])), "no 'undefined' or 'NaN' anywhere");

// interactions
function click(x, y) { m.sent.length = 0; m.ctx.onclick(x, y, 1, 0, 0, 0, 0, 0); return m.sent.slice(); }
function dragTo(x, y) { m.ctx.ondrag(x, y, 1, 0, 0, 0, 0, 0); return m.sent.slice(); }
const X = m.ctx;
const ar = X.armRect(), t0 = X.takeRect(0), kc = X.knobCell(2), vc = X.varCell(0), fr = X.fitRect(), cr = X.compareRect();
expect(click(ar[0] + 20, ar[1] + 8).join() === "arm", "Arm sends arm");
expect(click(t0[0] + 10, t0[1] + 5).join() === "removetake 0", "Take 1 x removes take 1");
click(kc.x + kc.w / 2, kc.y + 30);
const kd = dragTo(kc.x + kc.w / 2, kc.y + 30 - 25);
expect(kd.length > 0 && /^p bite 5/.test(kd[kd.length - 1]), "dragging the Bite knob up 25 px sets Bite +5 (Live parameter)");
m.ctx.ondrag(0, 0, 0);
expect(click(X.ix("stage") + 25, 27 + 5 * 15 + 6).join() === "pickrole 5", "the Bed button picks the role (its stage preset comes from the engine)");
const sv = click(X.stageX() + 134, 27 + 18 + 0.2 * 70);
expect(sv.some(s => /^p depth 0\.2/.test(s)) && sv.some(s => /^p pan 0$/.test(s)), "clicking the stage floor moves the sound there (depth, pan)");
m.ctx.ondrag(0, 0, 0);
expect(click(vc.x + 10, 108).join() === "solo 0", "the A button auditions A alone");
const bar = click(vc.x + vc.w / 2, 122);
expect(bar.some(s => /^p vara 50/.test(s)), "the A bar sets A's strength (halfway = 50 %)");
m.ctx.ondrag(0, 0, 0);
expect(click(cr[0] + 20, cr[1] + 8).join() === "compare 1", "Compare toggles");
m.ctx.ondblclick(kc.x + kc.w / 2, kc.y + 30);
expect(m.sent.some(s => s === "p bite 0"), "double-click resets a knob to 0");
const fit = click(fr[0] + fr[2] - 0.5, 42);
expect(fit.some(s => /^p fit 0\.99/.test(s)), "Identity <-> Fit bar: the right end is Fit");
m.ctx.ondrag(0, 0, 0);
// Moves: 7 moves = 2 pages; a click on the list shows the next page
const before = paintOps(m).filter(o => o[0] === "show_text").map(o => o[1]).join("|");
click(X.ix("moves") + 20, 40);
const after = paintOps(m).filter(o => o[0] === "show_text").map(o => o[1]).join("|");
expect(/page 1 of 2/.test(before) && /page 2 of 2/.test(after) && /Level matched/.test(after), "Moves pages: a click shows the rest");
expect(!paintOps(m).some(o => (o[0] === "move_to" || o[0] === "rectangle_rounded" || o[0] === "rectangle") && o[1] > 1116.01), "nothing drawn beyond the device width (1116 px)");

console.log(failures ? `\n${failures} FAILED` : "\nview: all passed");
process.exit(failures ? 1 : 0);
