# Font generator

Converts TrueType fonts into the 1-bit bitmap fonts compiled into the
firmware (`lib/ui/src/ui/fonts/`). Glyphs are rasterised by FreeType without
anti-aliasing, as they appear on the 1-bit e-paper panel.

```sh
pip install -r tools/fontgen/requirements.txt
python tools/fontgen/fontgen.py
```

The output is deterministic. Commit the generated files together with any
change to the script or to the font list in it (`FONTS`, `CHARSET`).

## Fonts

| File | Font | Source | License |
|---|---|---|---|
| `fonts/RobotoCondensed-Variable.ttf` | Roboto Condensed v3.008 (variable weight) | [google/fonts](https://github.com/google/fonts/tree/main/ofl/robotocondensed), upstream [googlefonts/roboto-classic](https://github.com/googlefonts/roboto-classic) | SIL Open Font License 1.1 ([`fonts/OFL.txt`](fonts/OFL.txt)) |
| `fonts/Fraunces-Variable.ttf` | Fraunces v1.000 (variable: optical size, weight, softness, wonky) | [google/fonts](https://github.com/google/fonts/tree/main/ofl/fraunces), upstream [undercasetype/Fraunces](https://github.com/undercasetype/Fraunces) | SIL Open Font License 1.1, no Reserved Font Name ([`fonts/OFL-Fraunces.txt`](fonts/OFL-Fraunces.txt)) |

Fraunces is set like in the "Briefing" design: optical size = pixel size,
softness 50, no "wonky" letters; the weight is given per size in `FONTS`.

The generated bitmap fonts are derived from these fonts and are distributed
under the same license; the license files are copied next to them.
