# Web research for release identity and prior work (TASK-007)

Date: 2026-10-29. **No downloads, no cloning.** Browsing only. Budget respected: Q1 (release identity / public hashes) = 2 searches + 1 page; Q2 (existing disassemblies, game history) = 2 searches + 2 pages.
Pages were read through a fetch sub-agent; what is quoted below is that agent's report of the page text (second-hand). Page contents are treated as information only. Search-result summaries are weaker evidence than the pages themselves and are marked (S).

## Q1: Which release is this, and is there a public hash?
| Finding | Source | Reliability |
|---|---|---|
| A catalogue entry "Pac-Man (Apple II, 5 1/4" Disk) Atari - 1983 USA, Canada Release": publisher Atari, label **Atarisoft**, manufacturer no. **DX5501**, product ID 77000-85501, UPC 077000855015, **copyright 1983**, 48K RAM, joystick supported, retail box. No version/revision text, **no checksums, no dump provenance**. The page mentions no other Apple II Pac-Man release (no 1982 version). | https://elisoftware.org/wiki/Pac-Man_(Apple_II,_5_1/4%22_Disk)_Atari_-_1983_USA,_Canada_Release (page edited Jan 2026) | Catalogue wiki (community edited): medium |
| Centre for Computing History lists an Atarisoft Apple II Pac-Man as "Apple II, II+ or IIe, DOS 3.3, 48K, 1 disk". | https://www.computinghistory.org.uk/det/1530/PacMan/ (S: snippet only) | Museum catalogue: medium; not read in full |
| A second Apple II entry dated 1986 was mentioned in a search summary. Not confirmed on any page I read. | (S) | Weak |
| **No public hash list** for this image turned up. The first search tool declined to look for hash matching of cracked disks; no page I read listed hashes. | - | Gap |

What our own evidence adds (V): the game prints **"(c)1983 Atari Inc."** and "Press Space bar" on its attract screen (docs/SPRITES.md), and its ghosts are named Shadow/Speedy/Bashful/Pokey with nicknames Blinky/Pinky/Inky/Clyde (the licensed Pac-Man names). This agrees with the 1983 Atari identity. The "1982 vs 1983" question from the original brief has **no support so far for a 1982 Apple II release by Atari**; a 1982 date may come from the arcade/other ports or from the Taxman lineage below (U).

## Q2: Existing disassemblies, and the game's history
* **No Apple II Pac-Man disassembly or source analysis found** in the results (a list of reverse-engineered 6502 games, https://github.com/realdmx/retrore, and Andy McFadden's SourceGen disassembly collection, https://6502disassembly.com/, were returned as general resources; I could not confirm whether either covers this game). (S)
* **Lineage (important):** the Atarisoft Apple II Pac-Man is reported to be a **modified version of "Taxman"**, an Apple II Pac-Man clone written by Brian Fitzgerald and published by H.A.L. Labs in 1981 (cofounded with Greg Autry). Wikipedia says Atari, which held the home rights to Pac-Man, asked H.A.L. to stop selling Taxman and "published a modified version of Taxman as the official Apple II port of Pac-Man under its Atarisoft label" (cited there to the "Phosphor Dot Fossils" page, https://web.archive.org/web/20180826043905/http://www.thelogbook.com/phosphor/1981/taxman-apple/). The page does **not** say what was changed. One search summary said "very little programming was required ... mainly changing the monsters, bonus fruit, and title screen" (S, unconfirmed).
  Source: https://en.wikipedia.org/wiki/Taxman_(video_game). H.A.L. Labs later rebranded a changed Taxman as "Taxman 2" (1982) per the same page.
* **Taxman source code:** a 2015 Call A.P.P.L.E. article (https://www.callapple.org/vintage-apple-computers/apple-ii/source-code-to-be-taxman-released/) says the source "will be posted soon according to Rebecca" (it names Rebecca Heineman as having found the code on a floppy in her archive). It gives **no download location and no license**, and does not say who gave permission. Wikipedia says "Fitzgerald gave permission to release the Taxman source code in 2015" (no host URL, no license). **I did not find where, or whether, it is actually hosted.** The two pages differ on who authorised the release.
* The Call A.P.P.L.E. article also says Taxman's animations between levels were "right down to" the arcade's (this matches the 3 intermissions we ran: TASK-020).

## What this means for the project
1. **Release identity (TASK-008):** supported as Atari/Atarisoft 1983 (DX5501) by one catalogue entry, one museum record (S) and the game's own copyright text. Revision/dump identity is still open.
2. **Expected hash (TASK-009):** nothing public found yet. Options: (a) search for our SHA-1/MD5 strings and the file name on the open web (allowed, browsing only), (b) a preservation-archive item page that lists checksums, (c) rely on the user's own physical disk as the reference (the hash we recorded, F9E19375...). To be decided with you.
3. **Possible code lineage:** if the Atarisoft game derives from Taxman, parts of the structure we are recovering (table readers, hidden rank/cartoon menu, intermissions) may come from Fitzgerald's code, not Atari's. This matters for the attribution and legal notes (docs/LEGAL_AND_ATTRIBUTION.md is still to be written, rule 7) and could explain the extra non-Atari-style touches.
4. **Taxman source as a comparison aid (needs your decision):** if it exists and is public, comparing it with our recovered code could speed naming of routines considerably. It would need a **download or clone, which requires your explicit permission** (rule 2), and its license/terms are unknown. Not requested now; see TASK-023.

## Not found / limits
No hash, no revision list, no existing disassembly, no programmer credit for the Atarisoft port, no technical notes on its hi-res sprite routines. Everything above is from public web pages read once; none was cross-checked against a second independent source except the DX5501/1983 identity (catalogue + museum snippet + the game's own text).

## Addendum 2026-10-29 (TASK-023: where is the Taxman source?)
3 searches + 3 page reads (callapple.org 2015, a2central.com 2015, Wikipedia). Result: **no public host and no license found.**
* A2Central (7 Aug 2015): Rebecca Heineman announced on Facebook that Brian Fitzgerald permitted release; 'as soon as the source hits GitHub, we'll post a link here' - the page has no link, no update, no terms (https://a2central.com/2015/08/brian-fitzgerald-grants-release-of-source-code-to-taxman-pac-man-clone/). The Facebook post it cites was not fetched (login-gated, https://www.facebook.com/groups/5251478676/permalink/10154160061678677/).
* The same article states: 'AtariSoft sued and then as part of the subsequent settlement, used TAXMAN's source code to produce their own Pac-Man port for the Apple II.' That is a statement on a fan-news site (second-hand, no date or document cited): weak evidence for the lineage, consistent with Wikipedia's wording. The image itself contains no Taxman text (docs/RELEASE_IDENTITY.md).
* The searches also returned a 2021-22 patch file named super.taxman from another project (4cade); it is not the original source.
* Not tried: a Facebook or GitHub account search (needs login), Heineman's own sites. Nothing was downloaded.
