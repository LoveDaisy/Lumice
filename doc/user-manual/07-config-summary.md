[中文版](07-config-summary_zh.md)

# Configuration Summary

The Summary window lays the current configuration out on one page — the crystals, their shapes and orientations, the sun, the light spectrum, the lens and view, the exposure — for a screenshot that someone else can read at a glance, without opening the app or a file. It shows the configuration only: the rendered picture is exported separately (Save → `Screenshot...`), and the page is not meant to be re-imported — for that, share the `.lmc` or the exported config JSON.

## 1. Where it is

Click the **Summary** button (document icon) in the Top Bar, next to **Colors** and **Analysis**. It opens an independent, non-modal "Summary" window beside the preview; click the button again, or the window's ×, to close it. Opening a different document — New, Open, Import, or Revert — closes it too, since the page describes one document.

The window is a fixed 1200 px wide and laid out in two columns — the settings groups on the left, the layers and their entries on the right — so that a typical document fits on one 1280 × 900 screen without scrolling; its height follows the content up to that budget, and only a very long document scrolls past it. It is read-only: nothing on it edits anything.

## 2. What is on the page

From the top:

- **Version** — the Lumice build that produced this picture. Two summaries that disagree are easiest to compare when both say which version drew them.
- **Sun** — Altitude, Diameter, Spectrum (or the custom spectrum's table).
- **Simulation** — Rays(M) (in millions, as the slider shows it), Max hits, Infinite rays.
- **Render** — Lens Type, FOV, the camera's Elevation / Azimuth / Roll, Visible and Front, Resolution, EV and EV Anchor, Mode (screen / print), and the ground colour — Sky Color under Screen, Paper Color under Print.
- **Settings** — the few settings that have no control on the main panels and are edited only in the Settings panel (today: ray allocation). They are set apart so that, reading the page beside the app, you know where to look for them.
- **Layer N** — a heading with the layer's multi-scatter probability and its entry count, then two tables, one entry per row, both numbered by the entry's position in the layer (`#`) so a row in one can be paired with its row in the other:
  - **Crystals** — `#`, **Enabled** and **Weight** (the card's toggle and proportion); **Crystal**, the pool number, the name you gave it and the type, spelled exactly as the crystal card and the Colors window spell it; **Zenith / Azimuth / Roll**, the three orientation distributions in full, the zenith led by the preset they classify as (`Plate · G 0(1)`: Column / Plate / Parry / Lowitz / Random / Custom), so a retuned preset (a Column at std 5 rather than 1) is visible; and **Filter** as the card summarises it (`3-5-1 In PBD`, or `None`), followed by the filter's name — a multi-row filter shows its first row and `(+N more)`, as on the card; the Filter Editor is where the rows are read in full.
  - **Shape** — `#`, then the columns of the editor's Crystal tab: Height for a prism, Prism H / Upper H / Lower H / Upper A / Lower A for a pyramid, then Face 3–8. The columns cover both types, and a column that does not apply to a row is simply empty.

  Under the tables, a one-line legend spells out the distribution letters.

**Distribution notation.** Wherever a value is drawn from a distribution — the three axis angles, and any shape scalar you randomized — the cell reads `letter centre(spread)`: the letter names the distribution (`G` Gauss, `U` Uniform, `Z` Zigzag, `L` Laplacian, `G*` Gauss (legacy)), and the two numbers are exactly the two on the panel's own controls — the mean and the Std / Range / Amplitude / Scale box of an axis, the centre and spread of a shape scalar — with no conversion: `U 0.900(0.100)` is a uniform whose Range box says 0.100, i.e. the interval [0.85, 0.95], not ±0.100. Two shortcuts: a shape scalar that is not randomized is just its number (`1.000`), and a full-circle uniform axis — mean 0, range 360, the Random preset's azimuth and roll — is folded to the bare `U`. A synced shape scalar carries `· sync N` after its value.

The labels are the panels' own words — `Rays(M)`, `EV Anchor`, `Sky Color` — so a reader holding the screenshot up beside the app finds each row under the same name; the one exception is `Visible`, which the View panel edits as three unlabelled radio buttons under the *Visibility* heading. The page also prints only what the panels currently show or enable: under a full-sky lens there are no FOV / Elevation / Azimuth / Roll / Visible / Front rows, just as those sliders are greyed on the panel; with Infinite rays on there is no Rays(M) row; and the Render group carries whichever of Sky Color and Paper Color the current Mode uses, never both.

The values are what the panels show, not what the simulator is asked for internally: a `linear` lens reads `linear` even though the preview always simulates a full-sky texture and reprojects it on the screen. Settings that describe your view of the app rather than the scene — overlay lines and their colours, the background photo and its path, panel layout, log levels, which windows are open — are not on the page, so the screenshot carries nothing about your machine.

Value spelling is the document's own: `fisheye_equal_area`, `relative`, `full` — the same words a `.lmc` file and the Settings panel's read-only column use.

## 3. Taking the picture

There is no image export button; use the operating system's screenshot tool and select the window — ⌘⇧4 then space-click the window on macOS, Win+Shift+S on Windows, or your desktop environment's equivalent on Linux. A document of a few layers fits the window with room to spare; if one is long enough for the page to scroll, take two screenshots or shrink the document first — the page prints every layer and entry, however many there are.

For the page as text rather than as a picture, **Copy as text** at the right end of the version line puts it on the clipboard: the version line, then each group as its title and one `label<TAB>value` line per field, then each layer as its heading and its two tables with tab-separated cells, header row first, and the distribution legend last. It is the same page model the window draws, so a row on screen is a line on the clipboard.

## 4. See also

- The exported config JSON, which describes the same picture in a form the CLI can render → Top Bar **Save** → `Config JSON...` ([gui-guide.md](../gui-guide.md))
- Save your current settings as personal defaults → the **Settings** panel, which lists the same settings rows with their factory values ([gui-guide.md](../gui-guide.md))
