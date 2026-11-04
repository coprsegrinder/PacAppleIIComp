# Section 9 research: CRT filters (2026-11-04)

Nothing was downloaded, copied or integrated. The search tool returns summaries of pages, not the pages, so every fact below is **as the search results reported it**; "(not verified)" marks what they did not settle. Licences in particular must be read from each file's own header before anything is ever used.

## 1. Candidates

| Candidate | What it looks like | Licence (as reported) | Fit for this project |
|---|---|---|---|
| **crt-lottes** (Timothy Lottes; libretro port) | Scanlines, shadow-mask, mild curvature; styled after an RGB arcade monitor | Original header says **public domain**; the libretro file's own header was not checked; one Godot port lists conflicting licences | Closest to a clean licence, but it is an arcade look (not Apple II) |
| **crt-geom / CRT-Geom (cgwg, Themaister, DOLLS)** | Curvature, scanlines, dot mask, halation | **GPL v2 or later** (header) | GPL would force the whole program under GPL if its code is copied |
| **crt-easymode (EasyMode)** | Simple, fast scanlines, mask, curvature | **GPL** (header) | Same GPL problem |
| **crt-royale (TroggleMonkey)** | Very detailed mask and beam simulation | Not found in the results (my belief: GPL; not verified); very heavy, many passes | Too heavy and not Apple-like |
| **crt-guest-advanced (Guest.r)** | Very configurable, optional NTSC pass | Headers of Guest's related shaders say **GPL v2 or later**; the advanced file itself not found | GPL problem; large |
| **Flyguy's "Apple II-like artifact colors"** (Shadertoy; used with Lottes in a libretro preset) | Reproduces NTSC artifact colour of Apple II-type machines | Not found (Shadertoy default is CC BY-NC-SA, not verified) | Interesting idea, but our colours already come from the dots; non-commercial licence is a risk |
| **NTSC-CRT (LMP88959)** | Full composite signal simulation: colour fringes, dot crawl, bandwidth | Not found in results | Authentic for a *colour TV*; too much for the game's exact dots, and heavy on the CPU |
| **MAME HLSL / AppleWin / izapple2 CRT modes** | Reference emulator behaviour | GPL | Reference for the *ideas* only, never code |

The libretro slang-shader collection is **mixed-licence** (packagers list GPL-2, GPL-3, LGPL-3, MIT and public domain together), so each shader's own header decides.

## 2. What is authentic for an Apple II

* **Colour monitor / TV:** soft horizontal edges (limited bandwidth), visible scanline gaps (240-line field drawn on a 192-line picture, so each picture line is a bright line with a dark gap between), slight bloom around bright dots, mild curvature of a glass tube. NTSC colour fringes exist on a composite TV, but our "Original Colour" picture is already the clean artifact-colour result and the user wants gameplay to stay readable, so no extra fringing.
* **Apple Monitor II / III (green):** a high-contrast, very dark tube, P31 phosphor. P31 measurements conflict: about 2 ms decay by photometer, ~1.4 % after 50 ms in a grating test; a forum post says the green Monitor III had a slower phosphor with trails (not authoritative). I found **no Apple Monitor II scanline/gap spec** (not verified). My choice: a short, modest persistence (a fraction of one frame) as an option, off by default.
* The Apple II is a 15.7 kHz TV-rate device: scanline structure is strong, and the 280x192 picture on a 4:3 tube is 1.458:1 within a 4:3 glass (this is also what our Bezel shows).

## 3. Recommendation: write our own filter

No candidate has both a licence we can use and an Apple II character:
* The good-looking ones are **GPL**; copying their code would oblige this whole project (including the decompilation docs and the port) to be GPL. Public domain crt-lottes could be used, but it would still be an arcade look needing heavy rework.
* A custom HLSL pass in our existing Direct3D 11 renderer has **no dependencies, no licence question**, runs at any resolution, works on both Original Colour and Native Black & White, and can be written to stay HDR-ready (Section 10: it works on linear values, so a later HDR output only changes the final stage).
* The general techniques are textbook (scanline from a cosine/gaussian of the line position, horizontal gaussian blur for bandwidth, an inverse-barrel curvature mapping, a phosphor mask, temporal persistence) and are not copyrightable; I would write them from the maths, not from any listed file.

## 4. Proposed design (not built)

Independent options, each off or neutral by default, in a new "CRT Filter" page of the menu and the settings file:
1. **Scanlines** (strength 0..100 %, off default): darker gap between picture lines, drawn per *output* pixel from the exact picture line position so it stays correct at every whole scale; at scale 1 it is disabled (no room for a gap).
2. **Softness** (bandwidth blur along the line: off / slight / medium): applies to both colour modes; this is where the softness removed from Native B&W lives.
3. **Phosphor glow / bloom** (off / low / medium): light spill from bright dots.
4. **Curvature** (off / slight / medium): a gentle barrel mapping; the *viewport rectangle stays 280:192*, the picture is bent inside it (corners stay inside; I would add a faint edge fade). Off by default because it moves pixels and makes the maze edges less exact.
5. **Phosphor persistence** (off / short): a fraction of the previous frame, per colour.
6. **Brightness / contrast** (neutral default).
7. **Pixel transition** (how sharply a dot goes from dark to lit; shares the softness control, because they are the same thing on a real tube).
8. **Authentic colour blending**: for Original Colour only, a very light horizontal chroma smear (artifact-colour bleeding between neighbouring dots), off by default.

Rules I would hold: filter **off = picture byte-exact as today** (test); applied only to the game picture, never to the pause menu text, bars or frames; no effect darkens the picture's average by more than a few percent at default "on" strengths so Pac-Man and ghosts stay easy to follow; tests compare the filter against an independent CPU model of the same maths at several scales and in both colour modes; the game, clock and sound are untouched.

## 5. Decisions needed

1. Custom filter written by me (recommended), or one of the listed shaders (only crt-lottes is licence-free; the others are GPL)?
2. Which of the eight options above do you want (all, as independent switches, is my recommendation)?
3. Defaults: everything **off** (picture exactly as now) with a "CRT Preset" shortcut of Off / Subtle / Classic (scanlines + slight softness + slight bloom), or something else?
4. Curvature and persistence: include them (off by default) or leave them out?

## 6. Sources (as reported by the searches)
* https://docs.libretro.com/shader/crt/ (libretro CRT shader list)
* https://github.com/libretro/glsl-shaders/blob/master/crt/shaders/crt-easymode.glsl (header: GPL)
* https://github.com/mamedev/mame/blob/master/src/osd/modules/render/bgfx/shaders/chains/crt-geom/fs_crt-geom-deluxe.sc (crt-geom GPL v2+ header)
* https://godotshaders.com/shader/crtlottes-for-godot-4-x/ (Lottes header: public domain)
* https://archlinux.org/packages/extra/any/libretro-shaders-slang , https://freshports.org/games/libretro-shaders-slang (mixed licences)
* https://filthypants.blogspot.com/2017/06/new-ntsc-shaders.html (Flyguy artifact shader with Lottes; per-channel persistence)
* https://epub.ub.uni-muenchen.de/16122/ , https://webdocs.cs.ualberta.ca/~wfb/publications/abstract24.html (P31 persistence)
* https://applefritter.com/node/26038 (Monitor III phosphor, forum)
