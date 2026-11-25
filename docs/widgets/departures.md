# Departures (MVG)

Live departures of one or two stations, with delays, cancellations and
disruption messages. Made and tested for Munich and its surroundings (the
area of the MVV); many stations elsewhere in Bavaria work too (see
[limits](#data-source-and-limits)).
Widget ID `mvg`.

## Setting it up

On the settings page, page **Abfahrten / Departures**:

1. Search the station and pick it. The display name follows the chosen
   station and can still be edited. If the search does not find a station,
   the link **Global-ID setzen** / **Set global ID** below it (**Ändern** /
   **Change**) opens a field for its global ID (e.g. `de:09162:10`); the
   display checks it with the MVG and shows its name.
2. Choose **one list**, **two directions** or **two stations**. The names of
   two directions are suggested as "Zentrum" / "Auswärts" ("Centre" /
   "Outbound" in English); with two stations the display name is the title
   of the first one.
3. Under every station, buttons for the means of transport that serve it,
   then for every line and direction whether and where it is shown. The
   example destinations come from the departures of the next 12 hours. At
   first all means of transport are on and all lines ticked; switching one
   off clears and hides its lines, switching it on again ticks them all
   (with two directions it hides its lines and restores the previous
   assignment). When all lines of a station are ticked, no line list is
   saved: the station then shows all its lines, also ones added later, and
   the limit of 16 lines does not apply.
4. Optional: the switch at the top of the page (**Ausfälle im Satz des
   Bands nennen**) takes cancellations out of the sentence of the band, and
   **Störungsmeldungen unter der Haltestelle** sets how many lines the
   disruption text may have.

The widget needs a column of at least step M (280 px), see
[the screen](../screen.md#layouts-and-columns).

## The column

The column shows one list (title: the station) or two sections (titles with
an arrow, or with the stop sign for two stations), 28 px per row: line
symbol, destination and time. Disruption messages of the lines shown follow
the station title (see [below](#disruption-messages)). A cancelled
departure among the next ones is also named in the sentence of the band
(see [the band](../screen.md#the-band)).

## Settings

The settings page writes these settings into the section `widgets.mvg` of
`config.json` ([configuration.md](../configuration.md)); they can also be
written by hand.

| Setting | Type | Default | Description |
|---|---|---|---|
| `station` | text | `""` | Station as MVG `globalId`, e.g. `"de:09162:10"` (München-Pasing). Empty: widget not set up. |
| `name` | text (≤ 64 bytes) | `""` | Name shown on the display. Empty: the name from the MVG API. |
| `transport_types` | list | all | Means of transport to request: `"SBAHN"`, `"UBAHN"`, `"TRAM"`, `"BUS"`, `"REGIONAL_BUS"`, `"BAHN"` (regional trains). Rail replacement buses come with the means of transport they replace. Ignored when `lines` is set. |
| `lines` | list | `[]` (all lines) | Only show these lines, at most 16. Each entry is `"line:direction:type"`, see [below](#line-entries). Not used with `sections`. |
| `direction` | `"all"`, `"H"`, `"R"` | `"all"` | Only departures with this MVG direction code (applies to all lines, in addition to `lines`). Not used with `sections`. |
| `sections` | list of 2 sections | none (one list) | Show the departures in two freely named sections instead of one list, for two directions or two stations, see [below](#two-directions-sections). |
| `time_format` | `"clock"`, `"minutes"` | `"clock"` | Departure as planned time with delay (`14:32 +3`; in a column of step L `14:32 +3 min`) or as minutes until departure (`4 min`, from 60 minutes on as clock time). |
| `band` | `true`, `false` | `true` | Mention a cancelled departure in the sentence of the band (see [the band](../screen.md#the-band)). `false` leaves it out; the notes on failed updates stay. On the settings page: the switch at the top of the page *Abfahrten* ("Ausfälle im Satz des Bands nennen"). Only `false` is saved. |
| `incidents` | `0`, `1`, `2` | `1` | Most lines of the disruption text under the station title. `0` hides only the text; the warning signs at the departures stay. Up to 0.4.x a switch: `false` still means `0`, `true` means `2`. On the settings page: page *Abfahrten*. |

#### Line entries

- **line:** as shown, spaces are ignored and case does not matter (`"RE 80"`
  = `"RE80"`).
- **direction:** `H` or `R` (MVG direction code of the line) or `B` for both.
  The codes are fixed per line and do not mean "towards the city centre";
  to name the directions, use [two sections](#two-directions-sections).
- **type:** `S` S-Bahn, `U` U-Bahn, `T` tram, `B` bus, `R` regional bus,
  `Z` regional train.

Examples: `"S3:B:S"` (S3, both directions), `"RE80:R:Z"` (RE 80, direction R).

If any regional train is selected, rail replacement buses for regional trains
are shown as well (also those without a line name), because the API does not
say which line they replace.

#### Two directions (`sections`)

Instead of one list, the column can show two sections with names of your
choice, for example towards the city and away from it:

```json
"mvg": {
  "station": "de:09162:10",
  "name": "Pasing",
  "sections": [
    {"name": "Zentrum", "lines": ["S3:H:S", "S4:H:S", "S5:H:S", "S6:H:S", "S8:H:S", "19:H:T"]},
    {"name": "Auswärts", "lines": ["S3:R:S", "S4:R:S", "S6:R:S", "S8:R:S", "14:H:T", "RB6:H:Z"]}
  ]
}
```

- Exactly two sections, each with a `name` (up to 24 bytes, shown in
  capitals) and its `lines` in the same format as above (at least one).
- Above the sections a title line shows the station (stop sign and
  `name`, or the name from the API). While the data is old, the age badge
  ("Stand 14:32") stands on its right.
- Lines that are in no section are not shown; `lines` and `direction` are
  ignored.
- The MVG direction code is fixed per line and does not say which way a
  line goes: at München-Pasing tram 14 "H" leaves the city while tram 19 "H"
  goes into it, and the same code can even lead to opposite destinations on
  one line. That is why every line is assigned with its direction.
- The column has room for 6 departures with two sections (8 as one list).
  They are shared evenly (3 + 3); rows a section cannot fill go to the
  other one.
- If anything about `sections` is invalid, the departures are shown as one
  list and the problem is reported.

The same response from the API is used for both sections, so two sections do
not cause more requests.

#### Two stations

A section can show a station of its own, for example the station at the
door and one a short walk away:

```json
"mvg": {
  "station": "de:09162:5",
  "sections": [
    {"name": "Ostbahnhof", "lines": ["S3:H:S", "S4:H:S"]},
    {"name": "Leuchtenbergring", "station": "de:09162:900"}
  ]
}
```

- `station` of a section: an MVG station ID as above; without it the section
  uses the station of the widget. If both sections have one, the widget's
  `station` may be left out.
- A section with a station of its own may leave out `lines`: then it shows
  all lines of that station.
- While the data is old, the first title shows the age badge on its right.
- The settings page writes the display name as the `name` of the first
  section.
- Both titles get the stop sign.
- Each station is requested once a minute, so two stations mean two
  requests a minute.

#### Disruption messages

Messages of the type "INCIDENT" that the API reports for the departures
that are drawn (not for the ones that do not fit on the display) appear
below the station title (two stations: below the title of the first one,
its rows follow the text), with a warning sign: "S2: Verspätung eines
vorausfahrenden Zuges.". The API only gives short texts without a heading.
Only the first message is shown (the one of the earliest departure drawn);
if there are more, the end of the text says how many: "+2" (in every
language). The
line in front ("S2: ") is left out when the departures drawn belong to one
line only; for several lines it names the lines of the message. Long
German words are hyphenated at the end of a line. The text has at most
the lines set with `incidents` (one by default, at most two), and gets
shorter, down to one line, so that every section keeps at least two
departures. If the text does not fit, its last line ends with "…" right
after the last letter that fits ("Wegen einer Stellwerksst…"), also in front
of "+1"; a word is not kept whole for the sake of the ending. The
affected departures keep a warning sign, right before the time and in the
same place in every row; a cancelled departure has the same sign and its
time struck through. Notes on early terminations stay in the row, fare notes
are not shown. `widgets.mvg.incidents: 0` hides the text only; the
warning signs and the cancellations stay.

## Data source and limits

- The departures come from the **unofficial** API of the MVG (the one its
  website uses). It is not affiliated with or endorsed by MVG, MVV or DB,
  has no documented terms and may change or stop working at any time.
- InkTavla is made and tested for the area of the MVV (Munich and the
  surrounding counties). The API knows stations in most of Bavaria and a
  few beyond (in a test on 2026-11-25: Augsburg, Nürnberg, Regensburg,
  Passau, Kempten, Hof, Lindau and Salzburg delivered departures), but not
  all: some stations deliver none (in that test Würzburg Hbf and
  Aschaffenburg Hbf; Ulm and Frankfurt were not found). Outside the MVV,
  fewer departures have live data.
  Try your station with the station search on the settings page; if its
  column stays empty ("Keine Abfahrten"), the API has no departures for it.
- The line symbols follow the style of Munich (MVV) everywhere: S-Bahn,
  U-Bahn and tram lines with a Munich number get the Munich symbol, all
  others a frame with their name.
- Each station is requested once a minute, at a second between 20 and 45
  chosen at random at every start, so that not all displays ask at once;
  new departures appear with the next minute.
- At most 8 departures fit in one list, 6 with two sections; at most 16
  lines can be chosen per station (no limit when all lines are shown).
- Platforms are not shown.
