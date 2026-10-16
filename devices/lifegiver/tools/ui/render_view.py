#!/usr/bin/env python3
"""Render the jsui view's recorded drawing (check_view.js output) to a PNG with cairo (what mgraphics is built on).
   python3 render_view.py <ops.json> <out.png> [scale]
Fonts differ from Live's (Ableton Sans → DejaVu Sans), so text widths are approximate; layout, colours and states are
as the view draws them."""
import ctypes
import json
import sys

cairo = ctypes.CDLL("libcairo.so.2")
D = ctypes.c_double
for fn, args in {
    "cairo_set_source_rgba": [D, D, D, D], "cairo_rectangle": [D, D, D, D], "cairo_move_to": [D, D], "cairo_line_to": [D, D],
    "cairo_arc": [D, D, D, D, D], "cairo_set_line_width": [D], "cairo_set_font_size": [D], "cairo_scale": [D, D],
    "cairo_curve_to": [D, D, D, D, D, D],
}.items():
    getattr(cairo, fn).argtypes = [ctypes.c_void_p] + args
cairo.cairo_image_surface_create.restype = ctypes.c_void_p
cairo.cairo_create.restype = ctypes.c_void_p
for fn in ("cairo_fill", "cairo_fill_preserve", "cairo_stroke", "cairo_stroke_preserve", "cairo_new_path", "cairo_close_path"):
    getattr(cairo, fn).argtypes = [ctypes.c_void_p]
cairo.cairo_select_font_face.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
cairo.cairo_show_text.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
cairo.cairo_set_line_cap.argtypes = [ctypes.c_void_p, ctypes.c_int]
cairo.cairo_surface_write_to_png.argtypes = [ctypes.c_void_p, ctypes.c_char_p]


def rounded(cr, x, y, w, h, ow, oh):
    r = max(0.0, min(ow / 2, oh / 2, w / 2, h / 2))
    if r <= 0:
        cairo.cairo_rectangle(cr, x, y, w, h)
        return
    import math
    cairo.cairo_new_path(cr)
    cairo.cairo_arc(cr, x + w - r, y + r, r, -math.pi / 2, 0)
    cairo.cairo_arc(cr, x + w - r, y + h - r, r, 0, math.pi / 2)
    cairo.cairo_arc(cr, x + r, y + h - r, r, math.pi / 2, math.pi)
    cairo.cairo_arc(cr, x + r, y + r, r, math.pi, 3 * math.pi / 2)
    cairo.cairo_close_path(cr)


def main():
    ops = json.load(open(sys.argv[1]))
    scale = float(sys.argv[3]) if len(sys.argv) > 3 else 2.0
    w, h = 1200, 169
    surf = cairo.cairo_image_surface_create(0, int(w * scale), int(h * scale))
    cr = cairo.cairo_create(surf)
    cairo.cairo_scale(cr, scale, scale)
    cairo.cairo_set_line_cap(cr, 1)   # round, like the mockup's strokes
    bold = False
    for op in ops:
        n, a = op[0], op[1:]
        if n == "set_source_rgba": cairo.cairo_set_source_rgba(cr, *map(float, a))
        elif n == "rectangle": cairo.cairo_rectangle(cr, *map(float, a))
        elif n == "rectangle_rounded": rounded(cr, *map(float, a))
        elif n == "fill": cairo.cairo_fill(cr)
        elif n == "fill_preserve": cairo.cairo_fill_preserve(cr)
        elif n == "stroke": cairo.cairo_stroke(cr)
        elif n == "stroke_preserve": cairo.cairo_stroke_preserve(cr)
        elif n == "set_line_width": cairo.cairo_set_line_width(cr, float(a[0]))
        elif n == "move_to": cairo.cairo_move_to(cr, float(a[0]), float(a[1]))
        elif n == "line_to": cairo.cairo_line_to(cr, float(a[0]), float(a[1]))
        elif n == "arc": cairo.cairo_arc(cr, *map(float, a))
        elif n == "new_path": cairo.cairo_new_path(cr)
        elif n == "close_path": cairo.cairo_close_path(cr)
        elif n == "select_font_face":
            bold = "Bold" in str(a[0])
            cairo.cairo_select_font_face(cr, b"DejaVu Sans Condensed", 0, 1 if bold else 0)
        elif n == "set_font_size": cairo.cairo_set_font_size(cr, float(a[0]) * 0.94)
        elif n == "show_text": cairo.cairo_show_text(cr, str(a[0]).encode("utf-8"))
    cairo.cairo_surface_write_to_png(surf, sys.argv[2].encode())


if __name__ == "__main__":
    main()
