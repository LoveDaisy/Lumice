[中文版](02-gui-quickstart_zh.md)

# GUI Quickstart

This chapter walks you through your first interactive simulation in the Lumice GUI: launch the app, load the bundled example, run, and read the preview. By the end you will know which panels to click and what the floating overlays mean.

> **Prerequisite**: a release build with the GUI enabled (`./scripts/build.sh -j release` produces `build/cmake_install/static/LumiceGUI`).

## 1. Launch

```bash
./build/cmake_install/static/LumiceGUI
```

On first launch the main window opens with no project loaded:

![LumiceGUI default empty state](../figs/gui_screenshot_default.jpg)

## 2. Tour of the main window

The annotated screenshot below labels the six regions you will use most often. The same numbering is used throughout this manual.

![Annotated GUI layout (English)](../figs/gui_screenshot_example_06.jpg)

| # | Region | Purpose |
|---|--------|---------|
| 1 | Top Bar | Open / save project, simulation Run/Stop/Continue, status badges |
| 2 | Left Panel | Crystal list, Light Source, Scattering, Render configuration cards |
| 3 | Crystal Preview | 3D preview of the currently selected crystal (wireframe / hidden line / x-ray / shaded) |
| 4 | Render Preview | Live halo image accumulated as rays land |
| 5 | Floating Lens Bar | Quick switch between lens projections (linear, equidistant fisheye, equal-area, …) |
| 6 | Status Bar | Current ray count, elapsed time, log severity counts |

> Region names are chosen for **what they do**, not what they look like — UI colours and pixel positions may drift between releases, but these six functions are stable.

## 3. Load the bundled example

`File ▶ Open` and choose `examples/config_example.json`. The Left Panel populates with the example's four crystals, light source, scattering layers, and one render entry:

![Example loaded](../figs/gui_screenshot_example_01.jpg)

The Crystal Preview shows the selected crystal in 3D. Click any crystal in the list to swap the preview.

## 4. Run a simulation and read the preview

Press the **Run** button in the Top Bar. The Render Preview accumulates rays in real time:

![Render preview with overlay grid](../figs/gui_screenshot_example_02.jpg)

While running:

- The Status Bar shows ray count and elapsed time.
- The Floating Lens Bar lets you switch lens projection without stopping the simulation — the same data is re-projected on the fly.
- The grid overlay (visible above) helps you read angles in the preview, and it travels with the document: its switches, colours and angle lists are saved into the `.lmc`, and an exported JSON carries them as `render[].grid`, which the CLI draws onto its output image too.

To stop early, press **Stop** in the Top Bar; partial results stay on screen.

Still too noisy once a run has finished — or after you stopped it? Press **Continue** next to
**Run**. It traces as many more rays as the Rays(M) field says (or, with Infinite rays on, keeps
going until you press Stop) and adds them to the picture on screen, so nothing already traced is
thrown away: the ray count carries on from where it was, and the brightness does not jump. **Run**,
by contrast, always starts over from zero. You can change Rays(M) between the two — it sets how
many rays the next Continue adds — and you can adjust exposure, overlays and the view freely.
Any other change (a crystal, the sun, a filter, …) means the picture no longer matches the
configuration, so Continue is disabled until you Run again or Revert the change; hover the button
to see why it is disabled.

### Is this spot bluer or redder? — the Channel B−R display

**Display ▶ Show As ▶ Channel B-R** turns the preview into a colour diagnostic: every pixel shows the blue channel minus the red channel of the normal picture (the displayed sRGB values, after gamma — the same subtraction you would do in an image editor's channel calculator), as grey. Mid grey means no difference; lighter than mid grey is bluer, darker is redder. Pure blue reads white, pure red reads black, and any neutral pixel — including empty sky and everything outside the lens circle — reads mid grey. Switch back to **Normal** to get the ordinary picture; nothing is re-simulated either way.

Three things to know before reading the numbers:

- **The value moves with EV.** B and R are the exposed, displayed channels, so raising the exposure makes the same halo read further from mid grey.
- **Clipping flattens it.** Once red or blue reaches full brightness it stops growing, so the difference is compressed in the brightest parts of the image.
- **So compare places at the same EV** — two readings taken at different exposures are not comparable.

Overlays (grid, circles, markers, labels, lens border) are drawn on top in their own colours. The background photo and the Colored raypath composite are hidden while the diagnostic is on (your settings for them are kept), and under **Mode ▶ Print** the option is greyed out: print has no separate red and blue to subtract. Screenshot exports what the screen shows, and an exported config carries `"display_mode": "channel_br"`, so the CLI renders the same grey image.

## 5. Author a new entry from scratch

Want to build a halo recipe yourself instead of opening the example? The shortest path is:

1. **File ▶ New** to start an empty project.
2. In the Left Panel **Crystal** card, click **Edit** (or **+ Add**) to open the Crystal Editor:

   ![Crystal editor dialog](../figs/gui_edit_crystal.jpg)

   Pick a preset (e.g. **Hexagonal Prism**), tweak `height` if you wish, and confirm. (Other shape fields like `face_distance` are accepted by the schema but are not yet active in the engine — see [`05-faq.md`](05-faq.md) §4.)
3. Open **Light Source** in the Left Panel and set `azimuth` (sun direction) and `altitude` (sun height above horizon).
4. Open **Render** and pick a lens, resolution, and field of view — the Floating Lens Bar can re-project later, so any sane default is fine.
5. Press **Run**.

> 📷 待补：Crystal Tab 整体截图（registered in `progress.md` placeholder list; will be folded into SUMMARY.md "待补充清单" at closeout).

## 6. Save and reload

`File ▶ Save As` writes a `.lmc` (a JSON document Lumice can also run from the CLI). Reopening it in the GUI restores **crystal / light / render** data, the lens projection and the overlay settings — but **not** pure viewing state such as the crystal preview style. See [`05-faq.md`](05-faq.md) "GUI vs JSON capabilities" for the full divergence list.

## Further reading

- Run the same `.lmc` headless from the CLI → [`03-cli-quickstart.md`](03-cli-quickstart.md)
- Reproduce classic halos with ready-made recipes → [`04-recipes.md`](04-recipes.md)
- Find out which raypaths make a halo, and exclude one → [`06-raypath-analysis.md`](06-raypath-analysis.md)
- Share your configuration as one screenshot → [`07-config-summary.md`](07-config-summary.md)
- Full panel reference → [`../gui-guide.md`](../gui-guide.md)
- All field names and types → [`../configuration.md`](../configuration.md)
