[中文版](07-config-summary_zh.md)

# Configuration Summary

The Summary window lays the current configuration out on one page — the crystals, their shapes and orientations, the sun, the light spectrum, the lens and view, the exposure — for a screenshot that someone else can read at a glance, without opening the app or a file. It shows the configuration only: the rendered picture is exported separately (Save → `Screenshot...`), and the page is not meant to be re-imported — for that, share the `.lmc` or the exported config JSON.

## 1. Where it is

Click the **Summary** button (document icon) in the Top Bar, next to **Colors** and **Analysis**. It opens an independent, non-modal "Summary" window beside the preview; click the button again, or the window's ×, to close it. Opening a different document — New, Open, Import, or Revert — closes it too, since the page describes one document.

The window has a fixed width; its height follows the content, up to the screen, and the content scrolls past that. It is read-only: nothing on it edits anything.

## 2. What is on the page

From the top:

- **Version** — the Lumice build that produced this picture. Two summaries that disagree are easiest to compare when both say which version drew them.
- **Sun** — altitude, disc diameter, the spectrum preset (or the custom spectrum's table).
- **Simulation** — ray count (in millions, as the slider shows it), maximum hits, infinite mode, ray allocation.
- **Render** — lens type, field of view, the camera's elevation / azimuth / roll, the visible hemisphere and the front clip, sim resolution, exposure offset (EV) and EV mode, tone (screen / print), and the background and paper colours.
- **Layer N** — the layer's multi-scatter probability and how many entries it has, then, indented, each entry:
  - **Crystal** — the pool number, the name you gave it, and the type, spelled exactly as the crystal card and the Colors window spell it;
  - **Enabled** and **Weight** — the card's toggle and proportion;
  - the shape rows the Crystal tab of the editor shows (Height for a prism; Prism H / Upper H / Lower H / Upper A / Lower A for a pyramid; Face 3–8), a randomized one printed as `centre ± spread distribution` and a synced one marked `· sync N`;
  - **Axis** — the preset the three distributions classify as (Column / Plate / Parry / Lowitz / Random / Custom), followed by **Zenith / Azimuth / Roll** in full, so a retuned preset (a Column at std 5 rather than 1) is visible;
  - **Filter** — as the card summarises it (`3-5-1 In PBD`, or `None`), plus the filter's name and, for a multi-row filter, one line per row.

The values are what the panels show, not what the simulator is asked for internally: a `linear` lens reads `linear` even though the preview always simulates a full-sky texture and reprojects it on the screen. Settings that describe your view of the app rather than the scene — overlay lines and their colours, the background photo and its path, panel layout, log levels, which windows are open — are not on the page, so the screenshot carries nothing about your machine.

Spelling is the document's own: `fisheye_equal_area`, `relative`, `full` — the same words a `.lmc` file and the Settings panel's read-only column use.

## 3. Taking the picture

There is no export button; use the operating system's screenshot tool and select the window — ⌘⇧4 then space-click the window on macOS, Win+Shift+S on Windows, or your desktop environment's equivalent on Linux. If the document is long enough for the page to scroll, take two screenshots or shrink the document first; the page prints every layer and entry, however many there are.

## 4. See also

- The exported config JSON, which describes the same picture in a form the CLI can render → Top Bar **Save** → `Config JSON...` ([gui-guide.md](../gui-guide.md))
- Save your current settings as personal defaults → the **Settings** panel, which lists the same settings rows with their factory values ([gui-guide.md](../gui-guide.md))
