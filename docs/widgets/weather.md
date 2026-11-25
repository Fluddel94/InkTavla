# Weather (Open-Meteo)

The forecast for one place: the course of today as a curve with weather
symbols and the next days. The band at the top uses the same data for its
weather picture and its sentence. Widget ID `weather`.

## Setting it up

On the settings page, page **Wetter / Weather**: search the place
([place search of Open-Meteo](https://open-meteo.com/en/docs/geocoding-api)),
pick it from the list and adjust the display name if you like; the
coordinates are under **Erweitert** / **Advanced**. The widget fits in
every column.

The weather is fetched even without a weather column, for the band (unless
its part of the sentence is switched off, see the setting `band` below).

## The column

Forecast from [Open-Meteo](https://open-meteo.com/) for one place, updated
every 30 minutes. The column shows the course of today from 0 to 24
o'clock as a temperature curve (the hourly values, each averaged with its
neighbours so that the line runs smoothly) and the next three days. At
four hours (5, 10, 15 and 20 o'clock) the curve shows the temperature of
that hour and above it a 30 px symbol of the weather around it (at night
with the moon; showers at night as rain of the same strength). The curve is
thin up to now and thicker from now on; a black dot marks now. In a column
of step L (367 px, band + 2) the labelled hours are 2, 6, 10, 14, 18 and 22
o'clock, and below follow tomorrow
in three parts (morning, midday, evening) and the two days after. The
forecast is fetched for six days (today and five); the band at the top uses
the same data for its sentence and picture.
Temperatures are in degrees Celsius. The column title is the place (`name`)
with the location pin and the credit "Open-Meteo" on the right.

The next three days are spread evenly below the chart: weekday, a 44 px
symbol and the high and low temperature at the bottom of the column. The
symbol of each day follows the daily weather code of
Open-Meteo. Rain is shown by its intensity: one stroke below the cloud for
drizzle and slight rain, two for moderate and three for heavy rain. Showers
have the sun behind the cloud, freezing drizzle and freezing rain a snow star
between two strokes. The band always shows the umbrella for rain; its
sentence uses the rain probability.

## Settings

| Setting | Type | Default | Description |
|---|---|---|---|
| `latitude` | −90 to 90 | none | Latitude of the place in decimal degrees, e.g. `48.137`. |
| `longitude` | −180 to 180 | none | Longitude of the place in decimal degrees, e.g. `11.575`. |
| `name` | text (≤ 32 bytes) | `""` | Name of the place, the title of the column (without a name: "Wetter"). |
| `band` | `true`, `false` | `true` | Mention the weather in the sentence of the band. `false` leaves it out; the weather picture stays. Without a weather column the weather is then not fetched and the band shows a placeholder (see [the band](../screen.md#the-band)). On the settings page: the switch at the top of the page *Wetter* ("Wetter im Satz des Bands nennen"). Only `false` is saved. |

Without `latitude` and `longitude` the column shows "Standort fehlt".

To find the coordinates of a place: open it in a map service such as
[OpenStreetMap](https://www.openstreetmap.org/) and right-click it
(**Adresse anzeigen** / **Show address** shows them on the left), or use the
[place search of Open-Meteo](https://open-meteo.com/en/docs/geocoding-api).
Two or three decimals are enough; the weather models work with a grid of
about 2 km.

## Data source and limits

Open-Meteo is free for non-commercial use; the data is licensed under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/), which is why the
source ("Open-Meteo") is named in the title of the weather column (the age
badge takes its place while the data is old). The values are shown rounded,
and the curve of the day is averaged from the hourly values.

- Temperatures are in degrees Celsius only; there is no setting for
  Fahrenheit.
- The forecast is updated every 30 minutes.
