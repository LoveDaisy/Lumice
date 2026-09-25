[中文版](gui-guide_zh.md)

# GUI Guide

This document describes the Lumice GUI application — an interactive graphical interface for configuring and previewing ice halo simulations.

## Building and Running

```bash
# Build the GUI application
./scripts/build.sh -gj release

# Run
./build/cmake_install/static/LumiceGUI
```

The GUI requires a display server and a GPU with OpenGL 3.2 Core Profile support.

## UI Layout

The main window is split into six regions. The same numbering is used in the labels below and in the rest of this document.

![LumiceGUI default layout](figs/gui_screenshot_default.jpg)

| # | Region | Purpose |
|---|--------|---------|
| 1 | Top Bar | File operations (New / Open / Save), simulation Run / Stop / Revert, panel collapse toggles |
| 2 | Left Panel — Crystal Parameters | Scattering layers and the crystal cards inside each layer (geometry / axis / filter / proportion) |
| 3 | Right Panel — View Parameters | Scene (Sun + Simulation), View (Projection + Camera), Display (Resolution / EV / Aspect / Background), Overlay |
| 4 | Render Preview | The lens-projected halo image accumulated as rays land |
| 5 | Status Bar | Simulation state badge, ray count, current resolution / lens / FOV, file name, log toggle |
| 6 | Popup Editor | Modal editor opened from a crystal card; edits Crystal / Axis / Filter |

The left and right side panels can be collapsed independently — useful when you want a wider Render Preview during simulation.

Read-only text can be copied: right-click a line of the log panel, a row of the Raypath Analysis list, or the filter editor's live preview for a **Copy** menu, and the log panel's tool row and the Summary window each carry a button that copies the whole panel (**Copy**, **Copy as text**) as plain text.

## Top Bar

From left to right, the Top Bar exposes:

- **Left panel collapse**: `<` collapses the left panel; `>` expands it again (also bound to the `[` key).
- **Run / Stop**: a single fixed-width button that toggles between green **Run** and red **Stop** depending on the simulation state. Disabled controls during a running simulation are re-enabled once the run finishes.
- **New / Open**: project lifecycle actions (disabled while simulating).
- **Save**: opens a popup menu containing:
  - `Save` / `Save Copy` — write the project as a `.lmc` file
  - `Screenshot...` — export the current Render Preview as PNG. It first opens a small options
    window, prefilled with what the preview shows: per overlay family a **Line** and a **Label**
    box, and the display mode (Normal / Channel B-R). Untick a box to leave that line or label out
    of this one export; a family the preview is not showing is greyed and cannot be added here —
    turn it on in the Overlay panel instead. Nothing chosen here is kept: the panel and the preview
    are unchanged, and the next Screenshot starts again from what the screen shows. The display mode
    may be switched either way for the export, except under Print (as in the preview) and while the
    Colored raypath composite is switched on for a coloured run, where the other mode would need a
    different picture than the one uploaded for the screen (both greyed, with the reason on hover). **Export...**
    then asks for the file name.
  - `Dual Fisheye Equal Area...` / `Equirectangular...` — server-side off-screen exports (require a finished simulation)
  - `Config JSON...` — export the configuration in JSON form, for re-rendering with the CLI. The
    exported renderer describes what the Render Preview is currently showing: the display
    projection and FOV, the view angles, the visible hemisphere, the background colour, the horizon
    line switch and the aspect preset's canvas shape, with the manual EV baked into
    `intensity_factor` (the CLI has no display-time exposure). Three deliberate gaps: overlay
    annotations other than the horizon line are not exported (the GUI's screen-space grid, labels
    and sun circles have no equivalent in the config schema); the `Free` and `Match Background`
    aspect presets fall back to the simulation texture's own 2:1 shape, because neither names a
    ratio that a saved config could reproduce on another machine; and a view with the **Front**
    hemisphere clip enabled is refused with a warning rather than exported, since the config format
    has no way to express that clip and the CLI would otherwise render the un-clipped view.
  - `Include Texture in .lmc` — toggle for the next save. A screenshot has no overlay toggle of its own:
    it shows the overlay the screen shows, so a clean image comes from turning the families off in the
    Overlay panel.
- **Colors / Analysis / Summary**: three buttons that each open an independent, non-modal window
  beside the preview — the raypath colour classes, the Raypath Analysis tool, and the read-only
  **Summary**, one page of the current configuration (version, sun, simulation, render settings,
  then every layer with its entries' crystal, shape, axis and filter) laid out in two columns to
  fit one 1280 × 900 screen and be screenshotted and shared — each row under the label the panel
  itself uses, and only the rows the panel currently shows (Sky Color or Paper Color, never both).
  The Summary has no image export of its own: take the picture with your operating
  system's screenshot tool (⌘⇧4 on macOS, Win+Shift+S on Windows, or your desktop's equivalent on
  Linux); **Copy as text** on its version line puts the same page on the clipboard as tab-separated
  text. See [User Manual — Configuration Summary](user-manual/07-config-summary.md).
- **Settings**: the personal-defaults editor — what a *new* document starts from, plus the
  `Application preferences` rows at the top, which describe this machine rather than any document:
  the GPU backend and worker count a new document starts with, and **UI scale**, a multiplier
  (75% – 200%) applied on top of the display's own scaling to every piece of the interface —
  text, controls and panels alike. The display's scaling (Windows 125% / 150%, a Retina screen)
  is detected on its own and needs no setting; the multiplier is for making everything larger or
  smaller than that. Unlike its two neighbours it takes effect in the running window the moment it
  is changed; **Save as my defaults** keeps it for every later start (`app.ui_scale_multiplier` in
  `user_defaults.json`), and a value tried but not saved stays in force until the program exits —
  the `(this window: N%)` note beside the control shows what the window is actually using.
- **⚠ Revert**: at the right end of the bar, beside the right panel toggle. Appears only after parameters have been changed since the last simulation finished (status `Modified`); restores the configuration that produced the last result. Its slot is held while it is hidden, so nothing else on the bar moves when it appears.
- **Right panel collapse**: `<` / `>` mirror the left toggle (also bound to the `]` key).

## Left Panel — Crystal Parameters

### Layer & Crystal Cards

The left panel holds one or more **scattering layers**, each containing one or more **crystal cards**. Layers stack from top to bottom; rays exit the previous layer and enter the next with the layer's `Prob.` value (disabled when there is only one layer).

![Single vs. multi-scattering examples](figs/gui_scattering_combined.jpg)

Each crystal card reflects one entry in the scattering layer:

- **Thumbnail** (left): a small 3D preview of the crystal geometry; the cache repaints when geometry or axis settings change.
- **Crystal row**: type (`Prism` / `Pyramid`) + an `Edit` button that opens the Popup Editor on the Crystal tab.
- **Axis row**: a named preset describing the axis distribution (`Parry` / `Column` / `Lowitz` / `Plate` / `Random` / `Custom`) + `Edit` opens the Axis tab.
- **Filter row**: a one-line summary of the ray-path filter + `Edit` opens the Filter tab.
- **Proportion slider** (`prop.`): how this entry is weighted within the layer (0 – 100).

The thumbnail can be drawn in four render styles. The style selector lives in the Popup Editor's preview pane, which both of that editor's shapes keep on screen the whole time — see the [Popup Editor](#popup-editor) section.

![Crystal preview styles: wireframe, hidden line, x-ray, shaded](figs/gui_crystal_styles_combined.jpg)

The bottom of each layer carries a `+ Crystal` button (add another entry) and the layer header carries a right-aligned `x` (delete the layer; disabled when only one layer remains). A bottom-of-panel `+ Layer` button adds a new scattering layer.

### Card Hover Actions

Hovering a crystal card reveals two small action buttons in its top-right corner:

- `D` — duplicate the entry into the same layer, directly below the original (deep copy of crystal / axis / filter / proportion).
- `×` — delete the entry. Coloured red and disabled when the layer would otherwise be empty.

The buttons fade in / out via alpha so the card layout stays stable; clicks are routed even on the very first hovered frame.

**Reordering** — drag a card by its thumbnail and drop it on another card of the same layer: the upper half of the target card places it above, the lower half below, and an accent line in the gap shows where it will land. Cards cannot be dragged into a different layer. A click on the thumbnail that does not turn into a drag opens the editor, like a click anywhere else on the card.

### Linked Entries

Two or more crystal cards that share the same crystal configuration and filter configuration form a **linked group**. Edits made through any member — crystal shape, axis distribution, or filter — are automatically visible on every other member in the group.

**fa-link badge** — A chain link icon appears below the hover action buttons on the card's right edge whenever the card belongs to a linked group (at least one other entry shares the same crystal and filter). Hovering the badge shows a tooltip listing the other entries in the group by layer and index. The badge is always visible (not hover-revealed) because it reflects a persistent state.

**"Link to..." — entering pick mode** — Open the popup editor for any card by clicking its `Edit` button. At the top of the editor you will see a sharing status line (`Not shared` or `Shared with N other entries`). Click **Link to...** to enter pick mode: the modal closes and a yellow hint bar appears at the top of the left panel:

> Pick mode: click an entry to share crystal/filter from Layer X / Entry Y (Esc to cancel)

Click any *other* card to link the current entry to it — the current entry adopts that card's crystal and filter. The popup editor reopens on the current entry so editing can continue from where it left off.

**Exiting pick mode without linking:** press `Esc`, or click any blank area in the panel (or switch panels), to cancel. The original crystal and filter are preserved.

**"Unlink"** — When the card belongs to a linked group, an **Unlink** button appears in the sharing status row of the popup editor. Clicking it forks the shared pool slots: the entry receives a private copy of its crystal and filter and leaves the group. Other members are unaffected.

**"D" (Duplicate) vs Link** — The `D` hover button (see [Card Hover Actions](#card-hover-actions)) creates a fully independent copy of the entry with its own crystal and filter. Unlike linking, the duplicate is born unlinked; subsequent edits to the original and the copy never affect each other.

**Edit propagation in a linked group:**
- Crystal and filter content edits in the popup editor are automatically reflected on all group members via the shared pool — no extra action required.
- Adding or removing a filter on one member also updates the filter reference for the entire group, so the fa-link badge remains visible and the group stays coherent.

**Co-shared highlight** — While the popup editor is open, other cards in the same linked group display an orange border. This makes it easy to see which entries will be affected by the current editing session.

## Right Panel — View Parameters

The right panel groups every parameter that influences how the simulated rays are rendered. Four collapsing sections are open by default.

### Scene

- **Sun**: `Altitude` (-90° to 90°), apparent `Diameter` (0.1° to 5°), and `Spectrum` (one of `D50` / `D55` / `D65` / `D75` / `A` / `E`).
- **Simulation**: `Infinite rays` checkbox (let the simulator keep accumulating until you stop it), `Rays(M)` count in millions when bounded (0.1 to 100 000 M, i.e. 1e5 to 1e11 rays, on a logarithmic slider so both ends of the range are reachable by hand; the box beside it takes a typed value), and `Max hits` per ray (1 – 20). `Adaptive ray allocation` — on by default, and changeable in Settings — deals each crystal's rays by its measured per-ray energy variance instead of its population share (`scene.ray_allocation` in `configuration.md`): the expected image is unchanged, but noise is made more even across crystals, so a faint halo's edge no longer looks rougher than the rest. The same switch governs the raypath analysis window's own runs (`06-raypath-analysis.md`): a rare raypath on a low-proportion crystal gets a far less noisy row. The choice is saved in the `.lmc` and written to an exported config; a document saved before the setting existed opens with it on.

### View

- **Projection**: `Lens Type` chooses among 10 lens projections — Linear, Rectangular, Fisheye (Equidistant / Equal Area / Stereographic / Orthographic), and Dual Fisheye (Equidistant / Equal Area / Stereographic / Orthographic). The combo presents them grouped so orthographic variants sit next to their siblings. `FOV` is clamped per lens; `Visible` (front / back / all) restricts which hemispheres of rays render. Under the Globe lens a `Back fade` slider appears beside them: it lets the far side of the sphere — its light and its grid lines — show through the near side, fading like fog with distance from the camera (the value is the fog length, 0–2; 0 = near side only; see `globe_back_fade` in `configuration.md`).
- **Camera**: `Elevation`, `Azimuth`, `Roll`. Disabled and forced to zero for full-sky lenses (the dual variants and the equirectangular export).
  The `Look At` button beside `Reset` points the camera at a named direction in one click: the six reference points the Overlay panel can draw (Zenith, Nadir, Sun, Subsun, Anthelion, Antisolar), and a **Horizon** series of four level bearings relative to the sun — `Toward sun`, `Sun +90°`, `Away from sun`, `Sun -90°` (elevation 0; the number is what the `Azimuth` slider will read, since the sun sits at azimuth 0). A preset writes only `Elevation` and `Azimuth`; `Roll` and `FOV` are left as you framed them.

The lens choice changes the geometry of the projected image dramatically:

![Five lens projections](figs/gui_lens_projections_combined.jpg)

### Display

- **Rendering**: `Resolution` (512 / 1024 / 2048 / 4096; highlighted in brown to flag that changing it re-runs the simulation), and `EV` (-6 to +6 stops of exposure offset).
- **Adaptive Brightness**: always on — no toggle. The P99.5 brightness of the current scene (filter-independent F1 anchor) is mapped to a fixed target level; the applied offset is shown as `+N.NN EV auto` next to the manual EV slider. Filter switches do not jump the EV. See [`doc/adaptive-brightness.md`](adaptive-brightness.md) for the full algorithm and additivity invariant.
- **Aspect Ratio**: a `Preset` combo (Free, 16:9, 3:2, 4:3, 1:1, 2:1, Match Background) plus a `Portrait` ↔ `Landscape` flip button. When the requested aspect cannot be honoured (window too small) a warning row reports the achieved versus requested ratio.
- **Background**: `Load Bg...`, `Clear`, `Show` checkbox, and an `Alpha` slider for compositing the loaded background under the rendered halo.

### Overlay

The auxiliary lines drawn on top of the Render Preview, as one table — every overlay answers the same questions, so they are read as rows rather than as stacked blocks. The columns are: a colour swatch, the name, `Line` and `Label` checkboxes (toggle the projected line and the viewport-edge label independently — line-only, label-only, both, or neither), an `Alpha` cell you drag (click it to type an exact value), and a trailing `⋯` fold for the one field a row has that the others do not.

Three rows: `Horizon`, `Grid` and `Lens Border`. None of the three owns a field the others lack, so none offers a fold — the fold column is there for the two rows of the `Angular Distance` section below, which share these columns.

`Lens Border` outlines the lens's own image circle — the edge of the region the projection is defined on. Outside it the preview is pure black and therefore indistinguishable from the background, which is the whole reason for the line: under a fisheye the valid area is usually a circle that does not fill the display, and a halo that does not span the whole sky leaves the user no way to see where the lens ends. It is off by default, draws no text label (empty `Label` cell) and has no fold, because it owns no field of its own: the circle is derived from the lens, the FOV and the viewport.

It applies to the seven fisheye-family lenses that have such a boundary — `fisheye_equal_area`, `fisheye_equidistant`, `fisheye_orthographic` and all four `dual_fisheye_*` variants — and draws nothing for `linear`, `fisheye_stereographic`, `rectangular` or `globe`. `linear` and `rectangular` have no bounded image circle at all; `globe` does have one (the sphere's silhouette) and is left out as a product call rather than for want of a boundary. Single-lens stereographic is excluded because its image always fills the display; the dual-fisheye variants are all included because their black region comes from a hard circular clip that applies regardless of the projection formula. When the FOV puts the boundary circle off screen, nothing is drawn — that is correct behaviour, not a missing line. The border is drawn independently of the `Visible` hemisphere setting: it is the lens's frame, so it stays a whole circle even where the sky it bounds is being hidden.

Under the table sit two independent sections, each a collapsing header that is closed by default and does not add a row to the table: `Angular Distance`, then `Reference Points` (the generalisation of the zenith/nadir markers to a family of six named sky points, one click away rather than six more rows). Their open/closed state is saved with the document.

`Angular Distance from...` holds the two families of iso-angular rings as two rows over the same columns as the main table — swatch, name, `Line`, `Label`, `Alpha`, fold. The header already says the rows are measured **from** something; the name column just says what. `Sun` is the classic halo rings centred on the sun, 22° / 46° by default (persisted as `grid.angular_dist` / `angular_dist_line` / `angular_dist_label`). `Lens Center` is the same kind of ring centred on the camera's **optical axis** — the view centre — so it stays put on the canvas while the view is turned and the sun rings slide past it (`grid.view_dist` / `view_dist_line` / `view_dist_label`, shaped exactly like the `angular_dist` three), defaulting to 22° / 46° / 90° — the first two double as common halo radii for framing against the lens centre, the third marks the front-hemisphere boundary. Each row's `⋯` fold opens its own angle editor — same shape for both (presets, a custom-angle input, the current list with per-entry delete) but its own preset table per family: `Sun` offers 9° / 22° / 28° / 46° (the halo radii), `Lens Center` offers 22° / 46° / 90° (matching its own default) — each editing only its own list, and offered whether or not that row's rings are currently drawn. The section header carries no buttons of its own — nothing here is family-wide: two line switches, two label switches and two alphas are all per row, so there is nothing for a header button to act on (`Reference Points` below has family-wide fields, and keeps them in a row of its own table rather than on its header). Reaching either row's switches or editor therefore starts with unfolding the section.

`Reference Points` holds the six named sky points — `Zenith`, `Nadir`, `Sun`, `Subsun`, `Anthelion`, `Antisolar` — as six rows over the same columns, each with its own swatch, `Line` (the ring) and `Label` (the name beside it); their `Alpha` and fold cells are empty, because the family shares one alpha and one radius. Those two live in an **`All`** row at the top of the table, which is the family as a row: no swatch; `Line` and `Label` are derived from the six — ticked when all six are on, empty when none is, a filled square when they disagree — and a click follows the usual select-all rule (everything on unless all six already were, in which case everything off), each column independently of the other; `Alpha` edits the family's shared opacity (persisted as `overlay_markers_alpha`, 0–1) in place; and the row's `⋯` fold holds the one field left with nowhere else to go, the ring radius (`overlay_markers_radius_px`, 2–20 px). The six entries remain the only thing saved — the `All` row is a view of them, not a field.

## Render Preview

The central area shows the live, lens-projected halo image. While idle the area shows a disabled `Render Preview` placeholder; once a simulation starts producing rays, the texture updates each frame. Overlay labels for horizon / grid / sun circles / compass are rendered on top of the texture, projected to match the active lens.

![Render Preview with halo](figs/gui_screenshot_example_04.jpg)

## Popup Editor

`Edit` buttons on a crystal card open a single modal editor holding the same three sections — Crystal, Axis and Filter — plus a crystal preview that redraws every frame, so geometry and axis edits are visible immediately. A sharing status row at the top shows whether the card belongs to a linked group and provides **Link to...** and **Unlink** actions; see [Linked Entries](#linked-entries).

The selector on the button row picks between two shapes, and the choice is saved with the document:

- **Compact** — the preview on top, the three sections stacked below it behind a tab bar. Narrow, and it keeps most of the halo preview behind the modal visible; you click a tab to move between sections. This is the default.
- **Expanded** — the preview and Crystal in a left column, Axis over Filter in a right column, no tab bar. All three sections are visible at once, at the cost of covering nearly all of the preview.

In both layouts the window's width is fixed, but its height is yours: drag the bottom edge to make it shorter or taller. The preview and the button row keep their size and the section area gives or takes the difference, so the OK / Cancel row stays in view at any height. On a screen too short for the whole window it opens already fitted to the screen. A height you drag is kept until the app is closed.

A document saved by an older version that used the earlier side-by-side tab layout opens as **Expanded**, its closest successor; that layout no longer exists.

Since v15 the modal can be detached as its own OS window via ImGui multi-viewport — drag the title bar outside the host window to float it.

![Popup Editor — Crystal, Axis, Filter](figs/gui_edit_modal_combined.jpg)

### Crystal Tab

Geometry of the crystal:

- **Type**: `Prism` (hexagonal prism) or `Pyramid` (hexagonal pyramid with truncated upper / lower wedges).
- **Shape parameters**: `height` for prisms; `prism_h`, `upper_h`, `lower_h`, and the wedge angles `upper_alpha` / `lower_alpha` for pyramids (the defaults map to Miller indices `{1, 0, -1, 1}`). That four-index label `{h, k, i, l}` and the three integers a config file's `upper_indices` / `lower_indices` carry are the same face written two ways: the third of the four is derived, `i = -(h + k)`, so a JSON document holds only `(h, k, l)` and rejects a four-element array. The rules, and what the wedge angle is measured from, are in [configuration.md](configuration.md#11-reading-the-miller-index-fallback-warning).
- **Face distance**: six values, one for each prism face, allowing irregular hexagonal cross-sections.
- **Tab order**: in the shape-parameter table and the Face Distance table, `Tab` / `Shift+Tab` move between the input boxes by column — down every Value box (both tables), then down every Spread box whose row has Rand on — wrapping from the last back to the first. Click into any box to start; the sliders, Sync cells and Rand checkboxes are not stops.

### Axis Tab

Three independent angular distributions controlling crystal orientation: `zenith`, `azimuth`, `roll`. Each distribution has:

- A type radio: `Gauss`, `Uniform`, `Zigzag`, `Laplacian`, or the legacy `GaussLegacy`.
- A `mean` (the centre angle) and a `std` whose meaning depends on the type — standard deviation for Gauss, full range for Uniform, amplitude for Zigzag, and scale for Laplacian.

### Filter Tab

Ray-path filtering for the crystal. The tab has two parts:

1. **Shared controls** (apply to the whole filter): an **Action** radio, plus the **`P` / `B` / `D`** symmetry checkboxes — `P` prism-face reflection, `B` basal-face reflection, `D` a further symmetry that is only offered when the crystal's axis configuration makes it applicable.
2. **A sum-of-products (SoP) row editor** where you type the predicate. A one-line summary of the resulting filter is shown on the corresponding crystal card.

#### Filter input mini-DSL

The predicate is a **sum-of-products**: an OR of rows, each row an AND of factors.

- **Each row is one OR term.** Add rows with `+ Add OR row`; a filter matches a ray if **any** row matches.
- **Within a row, `&` is AND.** `3-5 & entry:2` matches rays that satisfy both factors.
- **A blank row states nothing, so it is dropped.** It is not a wildcard: a row that matched every ray would widen the whole OR to everything (making your other rows moot), or — under **Exclude** — hide every ray and render a black frame. A filter whose rows are *all* blank is simply no filter, the same as leaving the crystal's filter unset. This holds whether the blank row was typed here or came from a `.lmc` file, so loading a config with an empty row applies the rows that say something and ignores the one that does not.

A factor is either a **raypath** or an **entry-exit** token:

| Factor | Syntax | Meaning |
|--------|--------|---------|
| Raypath | `3-5`, `1-3` | a face path |
| Raypath OR-alternatives | `1-3;3-5` | `;` = OR between raypaths; **distributes over `&`** (see below) |
| Entry face(s) | `entry:2`, `entry:1,2` | enter through face 2 (comma = face 1 **or** 2) |
| Exit face(s) | `exit:4` | exit through face 4 |
| Length (exact) | `len:3` | ray length **is exactly** 3 |
| Length (at most) | `len:<=5` | ray length **≤** 5 |
| Length (range) | `len:2-3` | ray length in **[2, 3]** |

**`,` is not a raypath connector.** Only `-` joins faces on the same path, and `;` separates alternate paths; a raypath token containing `,` (e.g. `3-5,1-2`) is rejected, with a message naming both. It used to be accepted as a second spelling of `-`, which meant `3-5,1-2` — typed to mean *two* paths — silently became the single four-face path `3-5-1-2` and rendered almost nothing. The `entry:1,2` comma in the table above is different, deliberate syntax: an OR-list on one entry/exit token, not a raypath. A `.lmc` written before this was enforced still loads — the `,` in its raypath tokens is rewritten to `-` on load, keeping the path it had, and the file is normalized the next time you save.

`entry:` / `exit:` / `len:` tokens in the **same row** merge into a single entry-exit factor (so `entry:2 & exit:4 & len:<=5` is one entry-exit predicate, not three). Repeating a token in one row is an error — use the comma list for multiple faces (`entry:2,3`, not `entry:2 & entry:3`).

**Examples**

| Intent | Type |
|--------|------|
| enter face 2 **and** exit face 4 **and** length ≤ 5 | `entry:2 & exit:4 & len:<=5` |
| raypath `1-3` **or** raypath `3-5` | `1-3;3-5` (one row) or two rows `1-3` / `3-5` |
| (`1-3` or `3-5`) **and** enter face 2 | `1-3;3-5 & entry:2` → `(1-3 & entry:2) OR (3-5 & entry:2)` |

`;` (and the `entry:1,2` comma list) is display sugar only — it fans out to separate OR terms when the filter is applied, so the underlying model stays a plain sum-of-products. A **live preview** below the rows shows the expanded predicate as you type, and the **ⓘ icon** next to the hint lists the full token syntax.

**A hand-written config can still ask for a wildcard.** The dropped-blank-row rule above is about a row that carries no predicate at all. A core config file (the CLI format, or one this GUI exported) can ask for a filter that admits every ray by writing `{"type": "none"}`, or by leaving the `type` key off the filter entirely; both are kept as a match-all row and can sit beside real alternatives in a composition. The two are different things arriving by different routes: an empty editor row is an unfinished thought, a `none` filter is a request.

**One shape is refused instead: `{"type": "raypath", "raypath": []}`.** An empty face list is not a wildcard — the simulator compares each ray's recorded path against the listed one, an empty list has nothing any ray can equal, and the filter therefore matches **no ray at all**. Under **Include** that renders a black frame. The editor has no row that says "no ray", and its nearest row (an empty one) says the opposite, so a filter of this shape is dropped on import with a warning naming it, rather than shown as something it is not. Any entry that referenced it opens with no filter; re-save the document and the filter is gone from it. Write the ray paths you want, or `{"type": "none"}` if you meant every ray.

For the filter architecture behind this editor (physical gate semantics, the `ComplexFilterParam` sum-of-products, the 1:1 crystal binding), see [`filter-architecture.md`](filter-architecture.md).

## Status Bar

From left to right:

- **Simulation state**: `Ready` (green), `Simulating...` (yellow), `Done` (blue), `Modified` (orange).
- **Rays accumulated** (when non-zero): scaled to `x10^3 / x10^6 / x10^9`.
- **Resolution / lens / FOV**: e.g. `1024x512 Fisheye Equal Area FOV:180`.
- **File name** with `*` indicator when the project has unsaved changes.
- **Log toggle** (right-aligned): `Log [>]` opens / closes the log panel along the bottom of the window.

## File Operations

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+S | Save project |
| Ctrl+Shift+S | Save Copy (as a new file) |
| `[` | Toggle left panel collapse |
| `]` | Toggle right panel collapse |

Run, Stop, New, and Open are not bound to keys — use the Top Bar buttons. Export operations (Screenshot, Dual Fisheye Equal Area, Equirectangular, Config JSON) are reachable through the Save menu popup.

### Project File Format (`.lmc`)

Lumice uses a binary project file format (`.lmc`) that stores:

- **Configuration**: all crystal, scene, render, and filter settings as semantic JSON
- **Preview texture**: optionally, the most recent render result. From format v5 this is the
  render's **unexposed linear XYZ energy** (the same data the live preview uploads), behind a
  small header that also carries the frame's exposure measurements. A reopened document
  therefore renders through the same shader path as a live run — the EV slider, the exposure
  mode, the background and print mode all keep working on it, and there is no exposure-time
  clipping baked into the pixels. From format v6 the energy is stored as **float16 with one
  global scale** (zlib-deflated), and the live preview's own GPU texture is the same float16 —
  quantized by the same function before upload — so the file still holds exactly the pixels the
  screen was sampling, at half the bytes. v5 files (float32) open unchanged and are written back
  as v6 on the next save. Files written by v4 and earlier embed an 8-bit PNG instead (exposure
  baked in) and still open, but keep their baked exposure until the next run.

The format uses a 44-byte header with magic number `LMC\0`, version field, flag bits (whether a texture is present, and which encoding it uses), and offset / size pointers to the JSON and texture payloads. Values are stored as human-readable semantic types (e.g. `"prism"` instead of enum indices) for forward compatibility. The texture section's own 32-byte header has no spare bytes left as of v6 (its last field became the float16 scale); adding a field there means growing the header and bumping the format version together.

A project file with a texture is still larger than the old PNG bake, but half of what v5 was.
Measured on three real scenes at the default 1024 simulation resolution (a 2048×1024 source
texture), saved from the GUI:

| Scene | Non-zero texels | v5 (float32) | v6 (float16) |
|---|---|---|---|
| 3568/31568 raypath halos, 5 M rays | 9% | 2.5 MB | 1.4 MB |
| 98°/120°/144° halos, 100 M rays | 65% | 17.1 MB | 8.5 MB |
| Lens flare (5-line spectrum), 20 M rays | 79% | 20.6 MB | 9.9 MB |

The old PNG bake was 0.2–5 MB; the texture is the all-sky source at 2048×1024, so the size depends
on how much of the sky the scene lights, not on the lens or the view. Halving the bit width is
also the last lever there is: Monte-Carlo noise is spatially white, so the compressor only ever
recovers the black texels.

Untick "Include Texture in .lmc" in the Save menu if the file size matters more than the embedded preview.

### Unsaved Changes

When the project has unsaved changes (`*` indicator in the status bar), the application warns before:

- creating a new project
- opening another project
- closing the application

The warning popup offers `Save`, `Don't Save`, and `Cancel`.

## Simulation Workflow

1. **Configure**: set up scattering layers and crystal cards in the left panel, then scene / view / display / overlay in the right panel.
2. **Run**: click `Run` in the Top Bar. The current configuration is serialised and submitted to the simulation core; parameter widgets are disabled during the run.
3. **Monitor**: the Status Bar reports the running state and accumulated ray count; the Render Preview updates continuously.
4. **View**: results stay in the Render Preview after the run finishes (state `Done`).
5. **Stop**: click `Stop` to halt early. Results accumulated up to that point remain visible.
6. **Revert**: if you have changed parameters after the run finished (state `Modified`), `Revert` restores the configuration that produced the last result.

The simulation state — `Ready`, `Simulating`, `Done`, `Modified` — is shown in the status bar and gates which Top Bar actions are enabled.

## Related Documentation

- [Configuration Guide](configuration.md) — detailed configuration reference (JSON format)
- [Architecture Document](architecture.md) — system architecture and GUI module design
- [Developer Guide](developer-guide.md) — GUI testing and development
- [User Manual — GUI Quickstart](user-manual/02-gui-quickstart.md) — step-by-step tour for first-time users
- [User Manual — Raypath Analysis](user-manual/06-raypath-analysis.md) — which raypaths make a halo, and how to exclude one
- [User Manual — Configuration Summary](user-manual/07-config-summary.md) — one page of the configuration, for sharing a screenshot
