#!/usr/bin/env python3
"""Synthetic hi-hat loops for the probes: python3 make_hats.py <out dir>"""
import sys, wave
import numpy as np
sr = 48000
out = sys.argv[1]
def biq(x, b, a):
    y = np.zeros_like(x); x1 = x2 = y1 = y2 = 0.0
    for i, v in enumerate(x):
        o = (b[0]*v + b[1]*x1 + b[2]*x2 - a[1]*y1 - a[2]*y2) / a[0]; x2, x1 = x1, v; y2, y1 = y1, o; y[i] = o
    return y
def hp(x, f):
    w = 2*np.pi*f/sr; al = np.sin(w)/(2*0.707); c = np.cos(w); return biq(x, [(1+c)/2, -(1+c), (1+c)/2], [1+al, -2*c, 1-al])
def bp(x, f, q):
    w = 2*np.pi*f/sr; al = np.sin(w)/(2*q); c = np.cos(w); return biq(x, [al, 0, -al], [1+al, -2*c, 1-al])
def peak(x, f, q, db):
    A = 10**(db/40); w = 2*np.pi*f/sr; al = np.sin(w)/(2*q); c = np.cos(w)
    return biq(x, [1+al*A, -2*c, 1-al*A], [1+al/A, -2*c, 1-al/A])
def hits(seed):
    rng = np.random.default_rng(seed); n = 8*sr; step = int(sr*60/120/4); x = np.zeros(n)
    for k, s in enumerate(range(0, n, step)):
        L = int(sr*(0.25 if k % 8 == 6 else 0.05)); t = np.arange(min(L, n-s)); env = np.exp(-t/(L/5)); vel = 0.9 if k % 4 == 0 else 0.55
        x[s:s+len(t)] += vel*rng.standard_normal(len(t))*env
    return x
def write(name, y):
    y = 0.5*y/np.max(np.abs(y)); d = (np.stack([y, y], 1)*32767).astype('<i2')
    w = wave.open(f"{out}/{name}.wav", 'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr); w.writeframes(d.tobytes()); w.close()
x = hits(3); write("hat_harsh", hp(hp(x, 6000)*0.6 + bp(x, 4200, 25)*1.6 + bp(x, 7500, 30)*1.2 + bp(x, 3800, 1.2)*0.5, 300))
write("hat_even", hp(hp(hits(9), 5000), 5000))
write("hat_pierce", peak(hp(hp(hits(5), 3000), 3000), 4300, 1.4, 9))
