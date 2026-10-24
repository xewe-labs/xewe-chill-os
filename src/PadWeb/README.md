# src/PadWeb

PadWeb is the pad's glue, a project-local module with the id `pad` and the NVS namespace `pad`. It turns sensor readings into fan speeds and strip colours, and serves the pad page.

- It listens to `Mlx90614` and passes every reading to `fan.set_temperature()`. An offline sensor sends NaN, so the fans go to the curve's last point.
- It colours the strip along the same curve through the led module: off below the first point (0.5 °C hysteresis), the cold colour at the first point, the hot colour at the last and a blend in between. The led module stores every call in NVS, so a colour is sent only when it moved by `XEWE_PAD_COLOR_STEP` and at most once per `XEWE_PAD_COLOR_MIN_MS`. `$pad lights off` leaves the strip to the `$led` commands.
- `attach(web_interface)` serves the pad page at `/pad` and `/`, and its JSON routes, on the web-interface module's server. The command console moves to `/console`. The sketch calls it in `setup()`, before `os.begin()`.

Open `PadWeb.cpp` first. `index_html.h`, `script_js.h` and `styles_css.h` hold the page, and `Config.h` holds the `XEWE_PAD_*` values.

| Route | Body |
|---|---|
| `GET /ui/config` | `{"curve_edge_colors":{"start","end"},"temp_curve":[{"temp","speed"}]}`, the shape the page posts back to `POST /ui/config` |
| `GET /led/data` | `{"lights","shown","temperature","led_brightness","led_state"}`; `shown` is the strip colour as `#RRGGBB`, `temperature` is `null` while offline |
| `GET /api/state` | the fan, lights, sensor and UI objects in one reply |

Open `http://<device-ip>/` (also `/pad`). The command console is at `/console`. The page controls:

| Control | What it does |
|---|---|
| Curve graph | The curve, the colour gradient under it and a white dot at the current temperature and fan speed. Updates every second. |
| Table | One row per point: temperature (0-100 °C) and speed (0-100 %). The console accepts -40 to 200 °C. |
| Add Point / Delete | Adds a point 5 °C after the last one / removes a point. The page keeps at least 2 points. |
| Save Curve | Sends the curve and the colours to the device. Nothing changes on the device before you save. |
| Coldest Color / Hottest Color | Saved as soon as you close the colour picker. |

Scripts can also call `GET /fan/data`, `/mlx90614/data` and `POST /ui/config` with
`{"temp_curve":[{"temp":30,"speed":20}],"curve_edge_colors":{"start":"#00FFFF","end":"#FF0000"}}`.

```sh
build/tools/.venv/bin/python -m xewe test --unit-only -- -k pad
```

`tests/unit/test_pad.py` slices `PadWeb.cpp` at the `// ---- settings` and `// ---- commands` banners, so keep those two lines.
