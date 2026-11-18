# Fonts

- `Saira-ExpandedBold.ttf`: the BW-2001's key legends (P6l2), standing in for the PET 2001's Eurostile Bold Extended. Saira by Omnibus-Type, SIL Open Font License 1.1 ([Saira-OFL.txt](Saira-OFL.txt)). A static instance of Google Fonts' variable `Saira[wdth,wght].ttf` (github.com/google/fonts, `ofl/saira`), width 125 and weight 700, cut with fontTools: `fonttools varLib.instancer -o Saira-ExpandedBold.ttf 'Saira[wdth,wght].ttf' wdth=125 wght=700`. Embedded in `bw_ui` by `src/ui/CMakeLists.txt` (`pet_font.cpp.in`).

The other keys' legends use Roboto Medium, from Dear ImGui's `misc/fonts`.
