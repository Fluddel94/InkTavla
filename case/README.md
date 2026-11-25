# Case

Two 3D-printed cases for InkTavla on the Seeed TRMNL 7.5" (OG) DIY Kit.
Which one to choose, how to print and how to assemble:
**[docs/case.md](../docs/case.md)**.

| Folder | Contents |
|---|---|
| [`slim/`](slim/) | Slim (USB cable): case, lid, plungers as STL, two pictures |
| [`slim-battery/`](slim-battery/) | Slim+Battery (room for the kit's battery): the same parts |
| [`source/`](source/) | The source the STL files are made from, and the checks |

Everything in this folder is Copyright (C) 2026 Fluddel94 and licensed under
[CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/)
(full text in [LICENSE](LICENSE)), not under the GPL of the firmware. The font
in `source/fonts/` (Roboto Condensed) is under the SIL Open Font License 1.1
(`source/fonts/OFL.txt`).

## Making the STL files

The case is drawn in Python with CadQuery 2.8.0. Every STL file and both
pictures of a variant are made by one command:

```sh
python -m pip install -r case/source/requirements.txt
python case/source/inktavla_case.py slim
python case/source/inktavla_case.py slim-battery
```

This writes `case/slim/` and `case/slim-battery/`. Every STL starts with the
header `InkTavla case - CC BY-NC-SA 4.0 - github.com/Fluddel94/InkTavla`
(CI checks it, see [CONTRIBUTING.md](../CONTRIBUTING.md)). Another version of
CadQuery may split the surfaces into slightly different triangles; the
shape stays the same.

## Checking a change

After every change of the geometry, run the checks for both variants. They
need the assembled parts, which `--scene` writes to a folder outside the
repository:

```sh
python case/source/inktavla_case.py slim --scene /tmp/slim-scene
python case/source/check.py /tmp/slim-scene case/slim
python case/source/check_lid_swing.py /tmp/slim-scene slim
python case/source/check_panel_path.py
```

- `check.py`: every part is one closed solid, no part overlaps another part or
  the hardware (board, components, solder joints, battery, panel), and the
  print sizes. The case touches the panel along a face; that shows up as a few
  thousandths of a mm³ and is fine.
- `check_lid_swing.py`: the lid is put on back first and swung down in steps;
  only the snap tongues may touch the case (that is the snap).
- `check_panel_path.py`: a rigid panel can be swung into the pocket of the
  frame (2D, also with 0.3 mm reserve). Run it after every change of the
  pocket, the lip or the rim.
- `render_simple.py`: shaded pictures of STL files without a browser
  (`inktavla_case.py` uses it for the pictures).

Most dimensions are named constants at the top of `inktavla_case.py`, with a
comment on what they do and why they have their value.
