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

The generated bitmap fonts are derived from these fonts and are distributed
under the same license; `OFL.txt` is copied next to them.
