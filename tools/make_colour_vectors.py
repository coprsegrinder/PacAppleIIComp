"""TASK-028a: expected colour-class histograms for the C++ video tests, from the verified Python colour model (tools/colour_check.py: predicted()).
Pages: the maze walls in both modes and the first 12 attract strings (as in make_hires_vectors.py). Prints counts per class: 0 violet, 1 green, 2 blue, 3 orange, 4 white,
-1 unlit (black). Development tool; original data from scratch/. Usage: python -I tools/make_colour_vectors.py"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
import colour_check, make_hires_vectors as mh, wall_model, text_model


def histogram(page):
    lit, pal = colour_check.pixels(page)
    cls = colour_check.predicted(lit, pal)
    return {c: int((cls == c).sum()) for c in (-1, 0, 1, 2, 3, 4)}


if __name__ == '__main__':
    body = mh.body
    print('wall normal', histogram(wall_model.wall_page(mh.table(0x5F69), 1)))
    print('wall forced', histogram(wall_model.wall_page(mh.table(0x5DBF), 0)))
    recs, _ = mh.records(0x434F, 12)
    page = bytearray([0x80]) * 0x2000
    for r in recs:
        text_model.draw_string(page, body, r)
    print('text 12    ', histogram(bytes(page)))
