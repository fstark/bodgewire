# Fonts

- `Saira-ExpandedBold.ttf`: the BW-2001's key legends (P6l2), standing in for the PET 2001's Eurostile Bold Extended. Saira by Omnibus-Type, SIL Open Font License 1.1 ([Saira-OFL.txt](Saira-OFL.txt)). A static instance of Google Fonts' variable `Saira[wdth,wght].ttf` (github.com/google/fonts, `ofl/saira`), width 125 and weight 700, cut with fontTools: `fonttools varLib.instancer -o Saira-ExpandedBold.ttf 'Saira[wdth,wght].ttf' wdth=125 wght=700`. Embedded in `bw_ui` by `src/ui/CMakeLists.txt` (`pet_font.cpp.in`).

- `Audiowide-Regular.ttf`: the launcher's title, BODGEWIRE in neon cyan (the user's pick, 2026-11-22). Audiowide by Brian J. Bonislawsky (Astigmatic), SIL Open Font License 1.1 ([Audiowide-OFL.txt](Audiowide-OFL.txt)), from Google Fonts (github.com/google/fonts, `ofl/audiowide`), unchanged. Embedded in `bw_ui` by `src/ui/CMakeLists.txt` (`title_font.cpp.in`).

- `AtkinsonHyperlegible-Regular.ttf`, `AtkinsonHyperlegible-Bold.ttf`: the reading font of the binder's pages and tabs and the Brief's note, at 16 px (the user's pick, 2026-11-27: players found the pages' text too small). Atkinson Hyperlegible by the Braille Institute, made for readers with low vision, SIL Open Font License 1.1 ([AtkinsonHyperlegible-OFL.txt](AtkinsonHyperlegible-OFL.txt)), from Google Fonts (github.com/google/fonts, `ofl/atkinsonhyperlegible`), unchanged. Embedded in `bw_ui` by `src/ui/CMakeLists.txt` (`page_fonts_data.cpp.in`).

- `AtkinsonHyperlegibleMono-Regular.ttf`: the pages' code spans, the Give up page's Fault and a memory chip's contents, where characters line up in columns. Atkinson Hyperlegible Mono, SIL Open Font License 1.1 ([AtkinsonHyperlegibleMono-OFL.txt](AtkinsonHyperlegibleMono-OFL.txt)). A static instance of Google Fonts' variable `AtkinsonHyperlegibleMono[wght].ttf` (`ofl/atkinsonhyperlegiblemono`), whose default weight is 200: ImGui draws a variable font at its default. Cut with fontTools: `fonttools varLib.instancer -o AtkinsonHyperlegibleMono-Regular.ttf 'AtkinsonHyperlegibleMono[wght].ttf' wght=400`. Embedded as above.

The other keys' legends use Roboto Medium, from Dear ImGui's `misc/fonts`.
