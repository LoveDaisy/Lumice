[中文版](06-raypath-analysis_zh.md)

# Raypath Analysis

The Raypath Analysis window answers "which raypaths make the light in this region of the sky, and how much does each contribute". Unlike the main preview, it does not render a picture — it runs a dedicated, non-rendering pass that groups every outgoing ray by its full raypath and reports each group's share of the total energy.

> **The document must have shown a picture once; it need not match.** Analyze submits the document as it is on the panels — the same scene a Run would render — so it works on a freshly opened `.lmc` that carries its rendered picture, and on a document you have edited since the last run (the picture on screen is then of the previous configuration, and a line under the button says so; the list always describes the configured scene). What it does not do is analyse a scene you have never seen rendered: on a new document, a JSON import, or an `.lmc` saved without its picture, Analyze and the **Point** region choice stay disabled — with a tooltip saying to press Run once — until the first Run. Apart from that, the only thing that keeps the button disabled is a run in progress.

## 1. Where it is

The analysis tool lives in the **Analyze workspace**: click the **Analyze** cell of the Top Bar's two-cell **Render | Analyze** switch (the current mode's cell is highlighted; the other cell switches to it). The window then shows the Analyze layout instead of the render layout: the scene editors in a fixed left column, the **scene panorama** (the committed picture through the same lens, view and display state the Render workspace shows) as the large main view, the **share list** this chapter describes in the right column's upper half, and the **diagnostics region** (the selection's details, or an entry guide before any result) in the right column's lower half. Under the panorama sits its **operation bar** — the region choice, the radius, the analysis budget and the Analyze / Stop buttons, next to the picture they aim at. The two column splits are draggable; the main view fills its whole column — the depths all moved to their own windows (the class map: §7; the pose and trace faces: §8). The two workspaces share the one scene document and the same editors; the old floating "Raypath Analysis" window is retired — everything it did happens in the workspace now.

The main picture keeps its full place even before the first render: press **Run in the top bar**
without leaving Analyze. Analyze still waits for a rendered picture of this document. At a fixed
window size the picture and operation anchors do not jump as results or warnings arrive. The bar
uses one row when the full Point controls fit and two rows otherwise, with the same height in
Whole sky and Point. Counts, centre coordinates and old-picture warnings scroll in the fixed
status area above the bar; the search and bottom list actions do not move when the list becomes stale.
At unusually compressed UI scales the controls have their own vertical recovery scroll.

## 2. Choosing a region (ROI)

The operation bar offers two modes:

| Mode | What it analyzes |
|------|-------------------|
| **Whole sky** | Every ray that leaves the scene, in any direction. No spatial filtering. |
| **Point** | A cone around a direction, shown on the scene panorama as a marker. The choice waits for the document's first picture. |

**Why there is no "in frame" mode in the GUI**: an earlier build offered a third choice — only rays landing inside the picture as currently framed. It retired with the free-camera main view: a region defined as "whatever is framed right now" is silently re-defined by every camera move, which contradicts the sky-direction semantics the other two anchor. The CLI's `--roi frame` and the engine behind it keep working unchanged (see §7), so scripts that used it are not affected.

**The Point marker**: switching to Point mode places the marker at the centre of the current view by default (if there is a picture to place it on, and no earlier point to keep). Its sky direction stays fixed as the view or layout changes; the marker is projected onto the picture each frame and can go off-screen. Hover over the marker for a hand cursor and drag it to move the centre; the fixed status area above the operation bar updates its altitude/azimuth reading. To set another centre directly — even when the marker is off-screen — **Alt+left-click** on the picture (**Option+left-click** on macOS). No Pick button or arming step is needed. A click outside the picture or in a lens's non-sky area leaves the centre unchanged. The marker and its ring are drawn on the panorama itself, so windows over the picture cover them.

Drag elsewhere on the picture to orbit the camera. The wheel uses the same camera zoom as Render: positive notches narrow FOV by 5° each, bounded by 1° and the lens's maximum; full-sky lenses do not zoom. Zoom keeps Look At Lock. A press keeps its original action until release, even if you change Alt/Option or move off the picture; a direct click sets the point once, not continuously while held. Angular Distance picking takes priority over a new press. These view and point gestures neither modify the scene nor start a run: after moving the centre, press **Analyze** to update the result. Render's Alt/Option background-photo gestures remain separate.

A **Radius** slider is available on the operation bar as soon as there is a centre — even before you press Analyze — and drives the ROI ring drawn on the panorama. Before a result exists, dragging it only changes that ring's size; once a result exists, it instead changes how far from the centre the report sums energy, still without re-running the analysis: the underlying pass always records the full cone split into fine angular rings, and the slider only decides, at display time, how many of those rings to add up. This means you can sweep the radius back and forth instantly after a single Analyze click.

## 3. Running the analysis

On the operation bar, **Infinite rays** and **Rays(M)** set how many rays the analysis traces — its own budget, independent of the document's **Rays** setting (it starts from that value the first time you open the window). Turn off **Infinite rays** and drag **Rays(M)** to trace a fixed total, in millions, across every wavelength; turn it on to trace until you press Stop. There is no separate early-stop setting for any region mode — the ray budget and the Stop button are the only two ways a run ends.

Press **Analyze**. A dedicated pass traces the scene and groups rays by raypath; while it runs you can press **Stop** to end it early with whatever has accumulated so far — pressing Stop never discards results, it just stops accumulating more. When it finishes (or is stopped), the result list below fills in.

**The analysis always runs on the CPU**, even if you have Metal or CUDA selected for rendering. This is deliberate, not a bug or a fallback you can turn off: the GPU trace path does not keep the per-ray bookkeeping this feature needs. There is no separate indicator for this in the window — Analyze simply may take longer than a GPU-accelerated render of the same scene would.

The analysis follows Settings' `sim.ray_allocation` (default `adaptive`, switchable to `proportional`), the same setting that governs the render (`scene.ray_allocation` in `configuration.md`). With it `adaptive` — the default — each Analyze deals rays across crystals by their measured per-ray energy variance rather than by their population share, starting the measurement fresh every time you press the button: the Energy shares it reports have the same expected values either way, but a rare raypath on a low-proportion crystal gets many more of the rays and so a far less noisy row (on a three-crystal scene with proportions 100 / 100 / 0.2, the rare row's noise at one million rays is about ten times lower). With it `proportional` the analysis deals by population share, the reproducible reference behaviour.

If the configuration has changed since your last run, Analyze still works and reports on the edited document — the picture on screen is the older one, and the fixed status area says so until you Run again.

## 4. Reading the result list

Each row is one distinct raypath — the full sequence of crystals and faces a group of rays went through, root to exit. A path through one crystal is written as its faces, `3-5` (entering face 3, exiting face 5). When a scattering layer holds more than one crystal, the crystal is named in front. With multiple scattering every layer is parenthesised and the layers are joined by arrows, root first: `(3-5) -> (1-3)`, or a chain where each of two layers names its crystal. **On screen** the crystal names are the numbers the crystal cards show: `#2(3-5)` is a chain of the card numbered **#2**, and the layer joiner draws as a right-arrow icon. Card numbers are stable — deleting or dragging a card never renumbers the others — and a result on show keeps naming the crystals it was computed from even if you edit the document afterwards (the list says when it is stale); after a re-analyze the rows re-attach to the same cards. The **text itself** — what the CLI prints, what Export CSV writes, what the C API hands out — is the interchange spelling, `C1(3-5)` and `C1(1-3) -> C4(3-5)` with the ASCII ` -> `, where the number counts the crystals in the order the scene was built. A filter created by **Exclude this raypath** carries the row's on-screen name (`Exclude #2(3-5)`), but the name is only a label; what the filter does is the face sequence and P / B / D it holds. The columns:

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
share recorded or exported before the change was computed under the old weighting. Part
of the dealt rays — about half, for randomly oriented crystals — are now rejected at entry,
so at the same ray count each row's **+/-** is somewhat larger than it used to be.

**Record limits.** The analysis keeps a large but fixed number of distinct raypaths — enough that a typical scene never notices — rather than growing without bound as more rays or scattering layers are added. If a scene does produce more distinct raypaths than fit, a grayed-out **other** row appears at the bottom of the list: it is the energy and ray count that did not fit in a named row, and it is what makes the Cumulative % column reach exactly 100 at the last row. It is never selectable and cannot be excluded, since it does not correspond to one raypath. The fixed status area notes when this happened ("record full (N hits)"); for the reference scenes shipped with this tool, and for most real configurations, it does not happen at all.

**Symmetry (P / B / D)**. The three checkboxes above the list decide which raypaths count as the same row. They name the same three elements as the filter editor's P, B and D (prism-face rotation, basal-face reflection, mirror symmetry), but here they merge only **physically equivalent** paths, so that one row is one physical class. With all three on (the default) on a regular crystal in random orientation, the six rotations and the mirror image of `3-5` are one row; turn D off and the mirror path `3-7` becomes its own row, turn P off and every rotation does. A checkbox asks; each crystal merges only under the symmetries its shape and its orientation really have — a Parry arc's locked roll keeps the rotations of `3-5` apart even with P on, a plate keeps `1-3` and `2-3` apart even with B on (the conditions are in [`../raypath/raypath-symmetry.md`](../raypath/raypath-symmetry.md) §2b, and the filter editor's `(i)` hints say which applies to a crystal). A filter's P / B / D is different: it relabels face numbers whatever the crystal, so on such a crystal it merges more than a row here does ([`../raypath/raypath-symmetry.md`](../raypath/raypath-symmetry.md) §1.1). This is a display-time choice: the analysis records every path unreduced, and toggling a checkbox regroups the result on hand at once — nothing re-runs, the totals do not change, and a selected row stays selected as long as its raypath is still a row (a row that merged into another is simply deselected). If you have pressed Run since the analysis, the result can no longer be regrouped; the window says which symmetry the list is showing, and the next Analyze applies the checkboxes.

**Search**. The box between the Symmetry row and the table narrows the list to the rows whose raypath text contains what you type, case-insensitively, matching either spelling of a row: the interchange text — the ASCII ` -> ` joiner, the `C1(` crystal prefix — so anything you copy out of the CSV or the CLI's output finds its row (`3-5`, `C2(`, `->` for every multi-layer path, `-> (1-3)`), or the on-screen one — `#2(` finds the row the card spelling draws. It is a view of the list, not a different list: hiding rows changes no number. The Energy column is still each row's share of the whole region, and the **Cumulative %** of a row that stays shown still reads "down to and including this row of the *whole* list", hidden rows counted — it is not re-summed over the rows you can see, so the last shown row can read well under 100. The **other** row is not a raypath and disappears whenever the box is non-empty. A selected row that the search hides is still the selection (Exclude keeps working on it), and clearing the box brings everything back. Export CSV writes the whole list, not the rows on screen. The text stays through Analyze and Revert — it is about how you are reading the result, not about which result — and is cleared when you open or create another document.

**Copying a row**. Right-click a row for two menu items. **Copy raypath** puts the raypath's text on the clipboard exactly as written above — the ASCII ` -> ` joiner, not the arrow the row draws — so it pastes into a filter, a search box or a note as the same text the CSV and the CLI print. **Copy row** puts the whole row there in the CSV's own form: `Raypath,Energy,Cumulative %,+/-`, comma-separated, the same formatting Export CSV writes for that row, so one row copied and a file exported agree byte for byte.

## 5. "Exclude this raypath" and "Export CSV"

Select a row and press **Exclude this raypath** to generate a filter that removes exactly the paths the row merged, no more and no less, and mark the document as modified. When the filter's own P / B / D under the list's bits would cover the same paths, the filter carries those bits and names the row's raypath; when it would cover more (a crystal whose shape or orientation lacks the symmetry, where the row is smaller than the filter's face-label family), the filter carries no symmetry and names each of the row's paths as its own alternative, so the **excluded** rows below show them one by one — press Run again to see the picture without it. This reuses the same filter mechanism as manually editing a crystal's filter in the crystal editor; it does not introduce a new kind of rule.

The button is disabled, with a tooltip explaining why, when:

- no row is selected;
- **the raypath crosses more than one crystal** (multiple scattering across layers) — a filter attaches to a single crystal, so there is no way to express "exclude this exact multi-crystal sequence" today. Only single-crystal, single-layer raypaths can be excluded this way;
- the crystal the raypath went through is no longer in the current document (e.g. you edited the crystal list after analyzing — run and analyze again);
- **an entry using that crystal already has an In filter** — Exclude can only add to an existing Out filter, so it stays disabled here; edit the In filter by hand, or change its action to Out, to continue.

If the crystal already has an **Out** filter instead, the button does not disable: pressing it appends this raypath to that filter as one more excluded alternative — the tooltip says which filter and how many other entries share it — rather than being refused. Excluding the same raypath twice does not add a duplicate.

**Excluded rows.** After the next Analyze the excluded raypath is gone from the result — that is the filter doing its job — but not from the list: it stays as a greyed row, marked **excluded**, under the result rows and above **other**, so what you have excluded so far is in front of you rather than in your memory. The rows are read off the document, every frame: one for each raypath of each Out filter on a crystal of the first scattering layer, in the spelling the result rows draw (`3-5`, or `#1(3-5)` under the card numbering when that layer holds several entries), whether the filter came from this button, from the crystal editor, or from a file you opened. Only raypaths are shown this way; a rule typed in the editor's other grammar (`entry:2 & len:3`) is not a raypath and has no row. And a raypath the result still lists is shown as the result row it is, never greyed — that means the filter did not take (the list was not re-analyzed, or is read under a symmetry the filter does not merge), and hiding the row would hide that. The **Energy** cell reads `was 12.34%`: the share the row had the moment you excluded it, kept for this session only and only under the same P / B / D as it was read with — an em dash otherwise (a filter from a file, from the editor, or a list under other symmetry bits). The greyed rows are not part of the total, have no cumulative share, and cannot be selected, so **Exclude this raypath** never targets one; the search box and the right-click copy reach them like any row (**Copy row** gives `raypath,excluded,<share>`, three fields). The **↺** button in a greyed row's last column is **Include again**: it removes that raypath from its filter — when it was the filter's only row, the crystal drops the filter altogether — marks the document as modified, and takes the row away; the raypath is back in the list after the next Analyze, and in the picture after the next Run. With the crystal editor open on that entry the row disappears from its filter list too. Export CSV does not write the greyed rows: the file is the result.

**Export CSV**, beside it, saves the list **as shown** — under the radius and the P / B / D symmetry currently on display, in the display order — to a `.csv` file through a save dialog. The file opens with `#`-prefixed lines that record the export time, the region, the cone centre / radius / rings summed for a Point result, the symmetry, the total ray count, the total energy the percentages are shares of, and the record-full hit count, so it describes itself without the window. Then one column-header row (`Raypath,Energy,Cumulative %,+/-`, matching the table above one for one) and the rows: the raypath name, its share of the total energy, the cumulative share, and the 1/√N noise together with the takeover bound the **+/-** column shows in parentheses (empty when none), and finally the **other** row when there is one. Numbers are written plain — no `%` sign, no thousands grouping — so a spreadsheet or script reads them directly. The button is disabled while there is no result.

## 6. The class overview

Select a row and the **Selection details** card in the diagnostics region (the right column's
lower half) asks the engine about that
class while you keep working: the query runs in the background (a **Cancel survey** button is
there while it does), and the card shows one of four honest states — waiting, cancelled, the
engine's named refusal (a class crossing several crystals answers `multi_layer_unsupported`,
which is the answer, not an error), or the completed overview. Nothing is computed on the
interface thread, and nothing is invented: a refused or unevaluated part shows the engine's own
words.

The completed overview is organized by the class's routing arm, a few main conclusions first
with the details behind expanders:

- **A direction-invariant class** (every pose sends its light back to the sun's own direction)
  shows the certified **source image** at the sun, the **weight tint** reading (e.g.
  "blue-tinted") with the exact measurement caliber spelled out, and the statement that
  wavelength moves no edge here — the colour question is the tint, not a shift.
- **A full-dimensional class** shows the **support** (the deviation range the class covers), the
  **sharp edge** with its closed-form certificate when the geometry certifies one, the
  brightness peak as a **finite-resolution probe reading** (distinct from the geometric edge),
  and the **chromatic dispersion** block: the inner edge at 650 nm and at 450 nm, how far apart
  they sit and which way (for the classic two-face halo, red inner and blue outer), followed by
  every other onset the table carried, and the basis — the engine's own geometry string and the
  certificate's formula, apex angle and cross-check — readable in place. The **weight colour
  reading**, the **weight tint** and the **reference profile** (the class's brightness along the
  solar radial line at the document's reference wavelength, drawn with empty bins as gaps, not
  zeros) sit behind their own expanders. A class without a sharp structure renders exactly that:
  a support and a probe reading, no fabricated "brightest point".
- **A family-restricted class** (a directed crystal) shows the **freezing ladder**: which
  orientation dimension each level froze, at what centre and width, how many candidate images
  each level proposed and how many the survival filter killed.
- **A refused class** says so in the engine's words — the delta axis could not partition this
  geometry, which is a statement about the partition, not that the class carries no light — and
  keeps the independent facts readable beside it: the **independent witness** (the declared
  source's own positive events on this class) and the **internal-reflection share**.

The header always names the reference wavelength and where it came from, the sun the positions
read, and whether the survey stopped inside its budget (a partial document says so). Selecting
another class re-asks; the card never mixes two answers.

## 7. The raypath view (the class reference map)

Selecting a share-list row opens **Raypath view**, a movable, non-modal window following the
selected physical class. The left pane is its scalar sky map; the right pane keeps the quality
action, wavelength, shared presets, sky-point controls and **Pose & trace** entry above an
independently scrolling identity/caliber/legend report. Closing a window only hides it: results
and in-flight work survive. Clicking the selected row opens it again.

**One camera, two pictures.** The main picture and the map use the same lens, FOV, orientation
and display clips. Drag bare sky to orbit that camera; the wheel applies the main picture's
5°-per-notch FOV change. Both pictures respond, but neither action requests another map. Each
canvas keeps its own aspect ratio and physical pixel size: a wider canvas sees more sky rather
than stretching a second camera. Fixed all-sky lenses cannot orbit/zoom; globe keeps its
outside-in grab, zero effective roll and independent far-side hint.

**Fit** means *Fit sampled map*: a shared preset, not a certificate of the class's complete
support. In composition it fits the existing reference; explicitly select Detail to fit that
source instead. Zero/no-band or unfittable sources disable Fit. **Full sky** selects the shared
rectangular, full-visible, zero-pose preset. Both affect the main picture, unlock Look At while
keeping its target, and perform no map calculation. First arrivals and class changes never
auto-Fit or launch an implicit second refinement.

**Quality is explicit.** First selection produces one full-sky **Reference**. Zooming only
reprojects its original cells; it does not create finer statistical bands. Narrow a zoomable
lens to the desired region and click **Refine** once to calculate a **Detail**. The same fixed
slot shows **Cancel** while computing, or **Retry** after cancellation/error. Refine captures
that click's map-canvas domain, grid and quality; moving/resizing while it runs does not chase
the camera, reissue, or pull the camera back when it arrives. Retry repeats the captured target,
not a new current view. Full-sky-equivalent targets ask you to narrow the lens first. Globe's
action covers its **near side only**; the far hint has its own actual source precision.

Only the current reference and **latest** detail are retained. Compatible detail cells replace,
never add to, the reference; outside detail the reference remains at its original precision.
Returning to an unreplaced detail domain needs no calculation. A new detail replaces the old
one: returning to an old-only domain is reference resolution again, not a hidden high-resolution
patch cache. Obtaining a new region therefore costs **one quality click plus the original
query wait**; a fixed all-sky lens also requires switching to a zoomable lens. Changing
wavelength/class/capture invalidates old sources and establishes a new reference.

**Read the source, not the colour.** The report names Reference/Detail original Δδ×Δaz, completed
members K/N, λ, n and bounds; the centre line says covered/outside and which source is shown.
The cursor reads the actual direction's original binary64 value, count and status from that
same source cell. Source-fixed colour scales and nearest-cell display do not change the
quantities. Dashed boundaries mark detail coverage; stripes mean **unknown**, not zero, and
singular cells stay distinct. Globe gives separate near/pick and far-hint readings; a clipped
near side cannot turn a far hint into a point pick.

Composition also requires the same captured scene/class, frame, wavelength, sampling and
**ordered completed member prefix**. Two matching K/N partial sources may compose, but the whole
picture remains **partial**. Different prefixes never combine, in either direction: default
composition keeps the reference; choose **Detail only** explicitly to inspect it, with unknown
outside its bounds. Refusals do not overwrite a valid frozen picture with a zero image. A rank-0
arm is a separate sun-position mass marker with original-cell m/Ω; changing screen area does
not change it, and a mixed class retains its band arm.

**Pointing.** Use **Alt/Option+click** to set a sky point, or drag its marker. A bare camera press
never becomes a point pick at release. Ownership lasts until release, including modifier
changes; explicit pickers have priority. Coordinates and **Set sky point** work without a
finite map. These actions do not move the scene ROI, calculate a map, or open Pose & trace
implicitly. The report scroll never zooms either camera. **Overlays** still consumes the
survey's own located objects and dispersion circles, with one grouped tooltip and a legend
of what was actually drawn; geometry positions are not whole-circle contribution claims.

## 8. The point inspector (the Pose & trace window, point detail)

Where §7 answered "what does this class look like over the whole sky", the faces fed by the
**sky point** answer "what is happening at this one direction" — and they keep the spaces
apart: the map is directions, the **sun direction globe** is the crystal's poses (the
directions the sun sits in *the crystal's own frame* — a projection of the pose space, not the
whole of it), the **Raypath 3D** region is one concrete light path through one crystal, and the
**factors readout** is the current pose's per-interface numbers.

Open **Pose & trace** beside the sky-point controls in **Raypath view** to inspect the
**3D ray and pose**. This button stays above the scrolling report, including before a point is
set, while it computes, and when no drawable sample exists; the window explains the current
empty, waiting or refused state. Select a class and pick a direction on the map (or type
coordinates and press **Set sky point**), then click **Pose & trace**. Picking or setting a sky
point does **not** open the window automatically. Close it with its title-bar close button and
use the same entry to reopen it.

**Point detail** (in the diagnostics region, under the class overview) remains another opener:
clicking its title opens the **same floating, non-modal window**, with the same current selection,
point document and pose cursor. Opening or reopening it starts no query and does not reset the
cursor. The block itself keeps the answer and its evidence: once a class is
selected and a sky point is set, the engine is asked about that exact direction in the
background (a couple-of-seconds tier; the point queues behind the survey and map on the same
worker, and the block says so while it waits, with a **Cancel point** button). The answer opens
with the engine's own echo — the target labels and deviation, the member face sequence, the
crystal, the wavelength and its source, the refractive index, the sampling tier — then the
component list as evidence rows (kind, end statuses, coverage, integral). A **point mass**
answer (a direction-invariant path — the sun's own image) is presented as the mass it is:
position, target separation, the orbit integral and its measure support — no fiber, no
playback, no forced curve. A refusal names itself; a multi-layer chain names
`multi_layer_unsupported` and points at the existing retreat (keep only this chain in the
filters and re-run).

In the window, the component selector is the globe region's combo — the one place that moves
between components. The window's two draggable splits (the 3D region | the right column, the
globe | the factor readouts) keep their ratios while the window is closed and reopened in the
same session.

Waiting, cancelled, refused and sample changes retain the same canvas and control slots. Missing
components or samples disable the corresponding controls instead of inventing data. Descriptions,
partial-document notes, legends and factor readouts scroll in each graph's report below it; they do
not squeeze the graph when they grow. Resize and divider changes still resize the graph normally.

**The sun direction globe** (the window's upper right) draws the selected component's fiber:
each valid sample is one pose where the sun sits somewhere in the crystal (the `u` arrow), the
polyline walks them in order, and the highlighted marker is the current sample. The deviation
grid (toggleable) colours the sphere by where each sun direction deflected the light — drawn
only when the document carries the grid, absent otherwise, never painted as zero. Under the
disk: the sample slider and **Play**. Dragging the slider or playing moves through the poses
that already came back in the document; the sky point itself never moves — playing changes the
pose you are looking at, not the direction you asked about — and with the window closed the
playing pauses (the cursor holds where it was and resumes on reopen).

**Raypath 3D** (the window's large left region) draws the current sample's real path: the
crystal's wireframe (its analysis shape — distributions flattened to the values the engine
used) with the entry leg, every internal reflection and the exit leg from the engine's
representative-ray recovery, colour-coded with a legend, contacts marked. **World** and
**Crystal** are the same physical path in two frames (drag orbits, wheel zooms). While the path
for a fresh pose is computing the region says so; a pose with no legal path names why (the
corridor's refusal and its measured area); a rank-0 point mass has no path and says that
instead of drawing one.

**Contribution curve** (the window's lower right) draws the selected component's five named
factors — ρ (the declared pose density, `rad^-3`), A/a² (the entry area over the crystal
length unit squared), T (the total transmission), K = 1/(J+ε), and their product ρ·A/a²·T·K —
along the component's own SO(3) **arclength** (the horizontal axis, in rad; a closed loop
starts at its seed, an arc starts at its backward far end with a **seed** mark mid-curve). One
shared logarithmic vertical axis carries all five curves at their actual magnitudes — nothing
is normalized per curve, so the arms stay comparable and the product visibly rides on its
factors. Points the engine could not produce stay gaps (a curve never bridges an unavailable
stretch or a different component), exact zeros are marked on the zero row under the plot, and
an arm that is zero or unavailable throughout says so in the legend. The **cursor** is the same
selection everywhere: click or drag on the plot to move the current pose (the slider, the globe
marker and the 3D path follow; playback stops), and hovering reads out the pose under the hand.
The report under the curve carries the component's identity and the engine's integral echo verbatim — its
completeness pair (`available/total`) is the honesty mark, an incomplete sum never masquerades
as a total, and the caliber line states the convention: these curves read the declared pose
density in the **angle-Lebesgue** measure. On fixed-axis (plate) scenes ρ — and with it the
product and the integral — reads identically zero in this convention (the declared measure
lives on a spin orbit), while A/a², T and K stay informative per their own availability; that
is the declared measure's honest answer, not missing data. The readouts are not an attribution
chart: the arms do not sum to 100 %, and a drawable curve is not a completeness claim about
the class.

**Current sample factors** (the scrollable list under the curve) reads the current pose's own
numbers — the orientation angles, ρ, the entry measures, the total transmission, the Jacobian
with its named-unavailable state, K, their product, and each interface's transmittance
labelled as conditional on this path. The class-level totals stay in the main window's
diagnostics; these are the per-pose, per-path numbers of the sample the globe's marker names.

## 9. From the command line

The same analysis is a CLI subcommand, `Lumice analyze`, and it writes the same CSV — byte for byte the file **Export CSV** saves for the same run, produced by one shared formatter rather than two. The config is the scene; the question is asked with options, never read from the config, which is what makes it scriptable and reproducible:

```text
Lumice analyze -f config.json                                          # whole sky, to stdout
Lumice analyze -f config.json --roi cone --center 43,0 --radius 2      # a 2° cone around alt 43°, az 0
Lumice analyze -f config.json --roi frame --render-id 1 --csv out.csv  # the rays inside render[] entry 1
Lumice analyze -f config.json --symmetry none --rays 5M --seed 7       # finest rows, own budget, replayable on one machine
Lumice analyze -f config.json --symmetry none --chain-capacity 32768   # a record twice the default size
```

`--roi sky | frame | cone` selects the region: the GUI's Whole sky and Point are `sky` and `cone`; the third choice, `frame` — the region the GUI retired with its free-camera main view, as §2 records — stays CLI-only with the same engine support it always had. `--center <alt>,<az>` names the cone's centre as the altitude and azimuth of the sky point (azimuth measured as the sun's, so `--center <sun_altitude>,0` is the sun); `--symmetry` is the P / B / D checkboxes (`PBD` by default, `none` for the finest rows); `--rays` is the window's Rays field (the scene's own `ray_num` by default, `"infinite"` included). Two things the window has no equivalent of: `--seed <N>` fixes the random seed so two runs of one question are the same run on one machine (a seeded run is single-threaded by contract; across platforms, standard libraries, ISA levels or trace backends the same seed is statistically equivalent, not identical — see [`../configuration.md`](../configuration.md) "Seed (`--seed`) and Reproducibility"), and a scene that runs forever is ended with Ctrl-C, which still writes the result accumulated so far. With `--csv` the file is rewritten atomically every second, so it is complete whenever a script reads it; without it the CSV is stdout's alone and progress goes to stderr. The CLI has no radius slider: every ring of the cone is summed, and the head's `cone_rings_summed` line says so. `Lumice analyze -h` is the full option list; see [`03-cli-quickstart.md`](03-cli-quickstart.md) §4.

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

## 10. Path feature reports from the CLI

The GUI analysis list still ranks complete raypaths in an ROI. It does not yet display the
new path-feature document or make its records clickable in the preview.

```bash
Lumice raypath -f config.json --crystal 1 --path 3-1-5 --report --budget-ms 15000
```

Schema 3 uses the actual product shape/orientation/solar disc/spectrum, physical L2 members and
one shared numerical route. Its evidence comes in two layers: a **geometry layer** that
enumerates the path's structural objects on the orientation-reduced sphere (deviation-map
critical sets, gate boundaries, transmission-weight kinks, restricted-family images) with exact
certificates, and a **demoted Monte-Carlo layer** (`mc_evidence`) that may corroborate or
contradict an object but never promotes or erases one. The `features` buckets (`actual` /
`candidate` / `unfinished`) are derived from object states, and the kinds the enumerator
actually produces are reported in the document's coverage row. There is no fixed path-name
detector matrix. An internal TIR condition is not automatically an observed blue edge. A
field-value colour band is defined by two levels at one observation scale, not by two different
smoothing widths.

This is conditional path optics under the configured source, not scene-allocation/filter weighting
or illumination inherited from earlier scatterings.

The default numerical deadline is 15 seconds; `--budget-ms` allows up to two minutes. Optical
and field-work caps are separate. `--events` controls outer samples, not image pixels; without
it the prefix adapts to the member/spectral work. No projection or 512² image is imposed.
With no `--wavelength`, the scene spectrum is used. Supplying it explicitly selects a diagnostic
spectrum, recorded as an override. An unfinished/partial report can still contain valid local
actuals and candidates; empty arrays never establish physical absence. Multi-crystal chains are
explicitly unsupported, but reflections within one crystal are allowed.

`--target`, `--grid`, and `--warm` remain a separate, unchanged target-fiber workflow and cannot
be combined with `--report`. The full definitions, limitations and the version-2 to version-3
migration are in
[the report field reference](../raypath/raypath-cli-output.md#7-target-free-path-feature-report-schema-3).

## Known limits of this first version

- The diagnostic map and point queries run at a **reference wavelength, nominal geometry and a
  point sun** — they are deliberately not the D65 scene picture, and every caliber line says so.
  The dispersion pair is the engine's fixed 650/450 nm comparison.
- Each query face carries its own budget (survey 15 s, point 2 s, the map its own default); a
  budget-stopped answer keeps the facts it established and says it is partial — it never pads
  with zeros or invents the missing part.
- Direction-invariant (rank-0) classes answer with the **point mass** arm — no fiber, no
  playback, no curve forced where the physics has none. Near-pole and exact-antisolar targets
  keep their existing named refusals.
- Deferred by the confirmed first-version scope, not regressions: per-wavelength colour map
  families, smart Fit for detached bright patches,   full-sky curves (Parry/Lowitz), full-spectrum / solar-disk images, energy cross-checks,
  backward pose dragging, interpolation between arbitrary pose samples, and multi-layer
  inversion (the existing keep-one-chain retreat applies).
- The map shares the main picture’s effective lens; the sun-direction globe is a projection of the
  crystal's pose space, not the whole of it.

## Further reading

- Full panel reference → [`../gui-guide.md`](../gui/gui-guide.md)
- Filter syntax used by "Exclude this raypath" → [`../gui-guide.md`](../gui/gui-guide.md) §"Filter Tab"
- Design record and mechanism detail → [`../raypath/raypath-analysis-panel.md`](../raypath/raypath-analysis-panel.md)
- Why the feature is split the way it is, and where it is heading (Chinese) → [`../raypath/raypath-analysis.md`](../raypath/raypath-analysis.md)
- Full CLI report fields and boundaries → [`../raypath/raypath-cli-output.md`](../raypath/raypath-cli-output.md) §7
