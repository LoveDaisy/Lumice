[中文版](06-raypath-analysis_zh.md)

# Raypath Analysis

The Raypath Analysis window answers "which raypaths make the light in this region of the sky, and how much does each contribute". Unlike the main preview, it does not render a picture — it runs a dedicated, non-rendering pass that groups every outgoing ray by its full raypath and reports each group's share of the total energy.

> **The document must have shown a picture once; it need not match.** Analyze submits the document as it is on the panels — the same scene a Run would render — so it works on a freshly opened `.lmc` that carries its rendered picture, and on a document you have edited since the last run (the picture on screen is then of the previous configuration, and a line under the button says so; the list always describes the configured scene). What it does not do is analyse a scene you have never seen rendered: on a new document, a JSON import, or an `.lmc` saved without its picture, Analyze and the **In frame** / **Point** region choices stay disabled — with a tooltip saying to press Run once — until the first Run. Apart from that, the only thing that keeps the button disabled is a run in progress.

## 1. Where it is

Click the **Analysis** button (route icon) in the Top Bar, next to **Colors**. It opens an independent, non-modal "Raypath Analysis" window that you can leave open alongside the main preview — it does not replace or dock into any existing panel.

## 2. Choosing a region (ROI)

The **Region** row offers three modes:

| Mode | What it analyzes |
|------|-------------------|
| **Whole sky** | Every ray that leaves the scene, in any direction. No spatial filtering. |
| **In frame** | Only rays that land inside the picture as currently framed (same lens / view / visible / front settings the preview uses). The choice needs the document to have been rendered once (a background photograph alone does not count); with the mode already selected and no live preview, the frame is the document's own view at its simulation resolution. |
| **Point** | A cone around a direction, shown on the preview as a marker. Like In frame, the choice waits for the document's first picture. |

**The Point marker**: switching to Point mode places the marker at the centre of the current view by default (if there is a preview to place it on, and no earlier pick to keep). It moves with the view every frame — projected from the direction it represents, not fixed to a screen position — so rotating or panning the view never leaves it behind. The marker, its ring and the pick crosshair are drawn on the preview itself, so any window over the preview — the analysis window, an entry editor — covers them rather than being drawn through. Hover over the marker for a hand cursor and drag it to a new spot to set the centre directly; the altitude/azimuth reading next to **Pick on preview** updates as you drag. Press **Pick on preview** instead for a banner across the top of the window ("Click on the preview to set the centre — Esc to cancel") and a crosshair cursor: click anywhere on the preview to move the marker there, or press Esc to cancel.

A **Radius** slider is available as soon as there is a centre — even before you press Analyze — and drives the ROI ring drawn on the preview. Before a result exists, dragging it only changes that ring's size; once a result exists, it instead changes how far from the centre the report sums energy, still without re-running the analysis: the underlying pass always records the full cone split into fine angular rings, and the slider only decides, at display time, how many of those rings to add up. This means you can sweep the radius back and forth instantly after a single Analyze click.

## 3. Running the analysis

Above the Analyze button, **Infinite rays** and **Rays(M)** set how many rays the analysis traces — its own budget, independent of the document's **Rays** setting (it starts from that value the first time you open the window). Turn off **Infinite rays** and drag **Rays(M)** to trace a fixed total, in millions, across every wavelength; turn it on to trace until you press Stop. There is no separate early-stop setting for any region mode — the ray budget and the Stop button are the only two ways a run ends.

Press **Analyze**. A dedicated pass traces the scene and groups rays by raypath; while it runs you can press **Stop** to end it early with whatever has accumulated so far — pressing Stop never discards results, it just stops accumulating more. When it finishes (or is stopped), the result list below fills in.

**The analysis always runs on the CPU**, even if you have Metal or CUDA selected for rendering. This is deliberate, not a bug or a fallback you can turn off: the GPU trace path does not keep the per-ray bookkeeping this feature needs. There is no separate indicator for this in the window — Analyze simply may take longer than a GPU-accelerated render of the same scene would.

The analysis follows the **Adaptive ray allocation** setting in Settings, the same switch that governs the render (`scene.ray_allocation` in `configuration.md`). With it on — the default — each Analyze deals rays across crystals by their measured per-ray energy variance rather than by their population share, starting the measurement fresh every time you press the button: the Energy shares it reports have the same expected values either way, but a rare raypath on a low-proportion crystal gets many more of the rays and so a far less noisy row (on a three-crystal scene with proportions 100 / 100 / 0.2, the rare row's noise at one million rays is about ten times lower). With it off the analysis deals by population share, the reproducible reference behaviour.

If the configuration has changed since your last run, Analyze still works and reports on the edited document — the picture on screen is the older one, and the line under the button says so until you Run again.

## 4. Reading the result list

Each row is one distinct raypath — the full sequence of crystals and faces a group of rays went through, root to exit. A path through one crystal is written as its faces, `3-5` (entering face 3, exiting face 5). When a scattering layer holds more than one crystal, the crystal is named in front: `C1(3-5)`. With multiple scattering every layer is parenthesised and the layers are joined by ` -> `, root first: `(3-5) -> (1-3)`, or `C1(1-3) -> C4(3-5)` where the layers hold several crystals. (On screen the list draws that joiner as a right-arrow icon; the text itself — what the CLI prints, what an exported filter is named, what the C API hands out — is the ASCII ` -> `.) The columns:

| Column | Meaning |
|--------|---------|
| **Raypath** | The chain's printable name, as above. |
| **Energy** | This raypath's share of the total energy in the current region, as a percentage. Rows are sorted by this, descending. |
| **Cumulative %** | The running total of the Energy column down to and including this row, so you can see at a glance how many rows account for most of the light. |
| **+/-** | The statistical noise of this row's Energy share, estimated from the number of rays counted for the raypath as 1/√N, so a thin tail entry reads as noisy rather than as a precise small number. (The count itself is not shown: under adaptive ray allocation it is how many rays the deal *sent* the crystal, which tracks neither the crystal's proportion nor the row's Energy.) A row that took over a slot vacated by a much smaller entry (see "Record limits" below) also shows, in parentheses, how much of its energy may actually belong to that other raypath. |

A raypath's raw energy is **not** the same as how visually prominent its arc looks on screen: a faint but wide-spread pattern (common with randomly-oriented crystals) can carry more total energy than a narrow bright arc, and can therefore outrank it in this list. The list answers "how much light", not "how eye-catching".

**Energy shares changed in 2026-09, and older numbers are not comparable.** Since then a
crystal intercepts light in proportion to the area it presents to the sun, instead of every
sampled orientation catching the same amount (`configuration.md`, the migration note under
`ray_allocation`). The analysis traces rays exactly the way the render does, so it inherits
the change: rows made by orientations that face the sun gain share, rows made by
orientations seen edge-on lose it, and in a layer that mixes oriented and randomly oriented
crystals the crystals' relative shares move too. Nothing in the analysis itself changed — a
share recorded or exported before the change was computed under the old weighting. About
half of the dealt rays are now rejected at entry, so at the same ray count each row's
**+/-** is somewhat larger than it used to be.

**Record limits.** The analysis keeps a large but fixed number of distinct raypaths — enough that a typical scene never notices — rather than growing without bound as more rays or scattering layers are added. If a scene does produce more distinct raypaths than fit, a grayed-out **other** row appears at the bottom of the list: it is the energy and ray count that did not fit in a named row, and it is what makes the Cumulative % column reach exactly 100 at the last row. It is never selectable and cannot be excluded, since it does not correspond to one raypath. The status line under the button notes when this happened ("record full (N hits)"); for the reference scenes shipped with this tool, and for most real configurations, it does not happen at all.

**Symmetry (P / B / D)**. The three checkboxes above the list decide which raypaths count as the same row — the same P, B and D symmetries the filter editor uses (prism-face rotation, basal-face reflection, mirror symmetry). With all three on (the default) the six rotations and the mirror image of `3-5` are one row; turn D off and the mirror path `3-7` becomes its own row, turn P off and every rotation does. This is a display-time choice: the analysis records every path unreduced, and toggling a checkbox regroups the result on hand at once — nothing re-runs, the totals do not change, and a selected row stays selected as long as its raypath is still a row (a row that merged into another is simply deselected). If you have pressed Run since the analysis, the result can no longer be regrouped; the window says which symmetry the list is showing, and the next Analyze applies the checkboxes.

**Search**. The box between the Symmetry row and the table narrows the list to the rows whose raypath text contains what you type, case-insensitively. It matches the text as written above — the ASCII ` -> ` joiner, the `C1(` crystal prefix — so anything you copy out of the CSV or the CLI's output finds its row: `3-5`, `C2(`, `->` (every multi-layer path), `-> (1-3)`. It is a view of the list, not a different list: hiding rows changes no number. The Energy column is still each row's share of the whole region, and the **Cumulative %** of a row that stays shown still reads "down to and including this row of the *whole* list", hidden rows counted — it is not re-summed over the rows you can see, so the last shown row can read well under 100. The **other** row is not a raypath and disappears whenever the box is non-empty. A selected row that the search hides is still the selection (Exclude keeps working on it), and clearing the box brings everything back. Export CSV writes the whole list, not the rows on screen. The text stays through Analyze and Revert — it is about how you are reading the result, not about which result — and is cleared when you open or create another document.

**Copying a row**. Right-click a row for two menu items. **Copy raypath** puts the raypath's text on the clipboard exactly as written above — the ASCII ` -> ` joiner, not the arrow the row draws — so it pastes into a filter, a search box or a note as the same text the CSV and the CLI print. **Copy row** puts the whole row there in the CSV's own form: `Raypath,Energy,Cumulative %,+/-`, comma-separated, the same formatting Export CSV writes for that row, so one row copied and a file exported agree byte for byte.

## 5. "Exclude this raypath" and "Export CSV"

Select a row and press **Exclude this raypath** to generate a filter that removes rays taking that exact path, under the same P / B / D symmetry the list is showing (so the filter removes exactly what the row merged, no more and no less), and mark the document as modified — press Run again to see the picture without it. This reuses the same filter mechanism as manually editing a crystal's filter in the crystal editor; it does not introduce a new kind of rule.

The button is disabled, with a tooltip explaining why, when:

- no row is selected;
- **the raypath crosses more than one crystal** (multiple scattering across layers) — a filter attaches to a single crystal, so there is no way to express "exclude this exact multi-crystal sequence" today. Only single-crystal, single-layer raypaths can be excluded this way;
- the crystal the raypath went through is no longer in the current document (e.g. you edited the crystal list after analyzing — run and analyze again);
- **an entry using that crystal already has an In filter** — Exclude can only add to an existing Out filter, so it stays disabled here; edit the In filter by hand, or change its action to Out, to continue.

If the crystal already has an **Out** filter instead, the button does not disable: pressing it appends this raypath to that filter as one more excluded alternative — the tooltip says which filter and how many other entries share it — rather than being refused. Excluding the same raypath twice does not add a duplicate.

**Excluded rows.** After the next Analyze the excluded raypath is gone from the result — that is the filter doing its job — but not from the list: it stays as a greyed row, marked **excluded**, under the result rows and above **other**, so what you have excluded so far is in front of you rather than in your memory. The rows are read off the document, every frame: one for each raypath of each Out filter on a crystal of the first scattering layer, in the same spelling the result rows use (`3-5`, or `C1(3-5)` when that layer holds several entries), whether the filter came from this button, from the crystal editor, or from a file you opened. Only raypaths are shown this way; a rule typed in the editor's other grammar (`entry:2 & len:3`) is not a raypath and has no row. And a raypath the result still lists is shown as the result row it is, never greyed — that means the filter did not take (the list was not re-analyzed, or is read under a symmetry the filter does not merge), and hiding the row would hide that. The **Energy** cell reads `was 12.34%`: the share the row had the moment you excluded it, kept for this session only and only under the same P / B / D as it was read with — an em dash otherwise (a filter from a file, from the editor, or a list under other symmetry bits). The greyed rows are not part of the total, have no cumulative share, and cannot be selected, so **Exclude this raypath** never targets one; the search box and the right-click copy reach them like any row (**Copy row** gives `raypath,excluded,<share>`, three fields). The **↺** button in a greyed row's last column is **Include again**: it removes that raypath from its filter — when it was the filter's only row, the crystal drops the filter altogether — marks the document as modified, and takes the row away; the raypath is back in the list after the next Analyze, and in the picture after the next Run. With the crystal editor open on that entry the row disappears from its filter list too. Export CSV does not write the greyed rows: the file is the result.

**Export CSV**, beside it, saves the list **as shown** — under the radius and the P / B / D symmetry currently on display, in the display order — to a `.csv` file through a save dialog. The file opens with `#`-prefixed lines that record the export time, the region, the cone centre / radius / rings summed for a Point result, the symmetry, the total ray count, the total energy the percentages are shares of, and the record-full hit count, so it describes itself without the window. Then one column-header row (`Raypath,Energy,Cumulative %,+/-`, matching the table above one for one) and the rows: the raypath name, its share of the total energy, the cumulative share, and the 1/√N noise together with the takeover bound the **+/-** column shows in parentheses (empty when none), and finally the **other** row when there is one. Numbers are written plain — no `%` sign, no thousands grouping — so a spreadsheet or script reads them directly. The button is disabled while there is no result.

## 6. From the command line

The same analysis is a CLI subcommand, `Lumice analyze`, and it writes the same CSV — byte for byte the file **Export CSV** saves for the same run, produced by one shared formatter rather than two. The config is the scene; the question is asked with options, never read from the config, which is what makes it scriptable and reproducible:

```text
Lumice analyze -f config.json                                          # whole sky, to stdout
Lumice analyze -f config.json --roi cone --center 43,0 --radius 2      # a 2° cone around alt 43°, az 0
Lumice analyze -f config.json --roi frame --render-id 1 --csv out.csv  # the rays inside render[] entry 1
Lumice analyze -f config.json --symmetry none --rays 5M --seed 7       # finest rows, own budget, reproducible
Lumice analyze -f config.json --symmetry none --chain-capacity 32768   # a record twice the default size
```

`--roi sky | frame | cone` is the panel's Whole sky / In frame / Point; `--center <alt>,<az>` names the cone's centre as the altitude and azimuth of the sky point (azimuth measured as the sun's, so `--center <sun_altitude>,0` is the sun); `--symmetry` is the P / B / D checkboxes (`PBD` by default, `none` for the finest rows); `--rays` is the window's Rays field (the scene's own `ray_num` by default, `"infinite"` included). Two things the window has no equivalent of: `--seed <N>` fixes the random seed so two runs of one question are the same run (a seeded run is single-threaded by contract), and a scene that runs forever is ended with Ctrl-C, which still writes the result accumulated so far. With `--csv` the file is rewritten atomically every second, so it is complete whenever a script reads it; without it the CSV is stdout's alone and progress goes to stderr. The CLI has no radius slider: every ring of the cone is summed, and the head's `cone_rings_summed` line says so. `Lumice analyze -h` is the full option list; see [`03-cli-quickstart.md`](03-cli-quickstart.md) §4.

**`--chain-capacity <N>`** sizes the record limit described in §4 for this one run: how many distinct unreduced raypaths are kept exact before the rest fall into the **other** row. The default, 16384, is the window's fixed size and holds every reference scene shipped here; a `--symmetry none` listing of *every* path through a single crystal can need more — a hexagonal prism's 8-face paths alone number about 29 000, so at the default they are what ends up in **other** (the head's `record_full_hits` line counts how often a path was turned away; 0 means the record was never full). Plain integer, no K/M/G suffix, 1 to 1048576. The cost is memory, roughly `workers × N × 220 bytes` on the trace side plus about as much again once for the listing: 32768 on 10 workers is ~140 MB, the maximum is several GB, and `--workers` above the automatic count multiplies the first term. It has no equivalent in the window, whose record stays at the default.

**Known noise in a `--symmetry none` listing.** On a convex crystal a ray cannot hit the same face twice in a row, so a raypath like `4-2-7-7-1-2-1` is not a physical path: such rows are numerical by-products of a ray grazing an edge or a vertex, and they are what a run's tail can look like — energy printed as `0.0000`, a single ray each (`+/-` reads 100). They do not affect the energy shares of the real rows, and the engine does not filter them (telling a true edge case from a rounding one at record time is an open question, not a rule it can apply). Drop them before doing statistics on the file — any row whose face sequence repeats a face consecutively:

```python
import csv
rows = [r for r in csv.reader(open("out.csv")) if r and not r[0].startswith("#")][1:]
def repeats(name):  # any layer whose face sequence repeats a face consecutively
    for layer in name.split(" -> "):
        f = layer.split("(")[-1].rstrip(")").split("-")
        if any(a == b for a, b in zip(f, f[1:])):
            return True
    return False
clean = [r for r in rows if r[0] != "other (not recorded)" and not repeats(r[0])]
```

## Further reading

- Full panel reference → [`../gui-guide.md`](../gui-guide.md)
- Filter syntax used by "Exclude this raypath" → [`../gui-guide.md`](../gui-guide.md) §"Filter Tab"
- Design record and mechanism detail → [`../raypath-analysis-panel.md`](../raypath-analysis-panel.md)
