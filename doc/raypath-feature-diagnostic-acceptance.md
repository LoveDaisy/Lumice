# Raypath Feature Diagnostic Acceptance Record

> Historical schema-1 fixed-detector acceptance, not the current product contract. Schema 2
> replaces that dispatcher and its unconditional visibility claims; see `raypath-cli-output.md`
> §7 and `feature-diagnostic-discovery-research.md` §19. The recorded inputs, hashes and physical
> observations below remain historical evidence and have not been regenerated.

This is a fixed-input product acceptance record. Image evidence is observational,
not a cross-machine golden and not a claim about every scene.

## Reproduction envelope

The product binary was built from revision
`9af04880618ea7d095a31f3d7d8b48203877a0be`. Every command below was run
from the repository root with `build/cmake_install/static/Lumice`. The stable
inputs are tracked so that reproducing a command never requires a scientific
output directory.

| Input | SHA-256 |
|---|---|
| `test/e2e/configs/raypath_feature_random_regular.json` | `60031d0534c8767cbaf5bcee9bc953adc3363640d4b04ef11aff5354ded30920` |
| `test/e2e/configs/raypath_feature_rhombic_plate.json` | `e46e3399f5f4a4ad6c5a80737206fc2d744147655f5bad1906a7403a9f2cccf0` |
| `test/e2e/configs/raypath_feature_plate_target.json` | `ba82129135ae3420f108aa25676077b5c6cc37b7c68f5a03f6b3767902662b9b` |
| `test/e2e/configs/raypath_feature_canonical_target.json` | `e478608db03d138422f690157ba3bb5f8421cc792ada042acf1d93817c113d33` |
| `test/e2e/configs/raypath_feature_random_315_render.json` | `155f05c5717459ae63fdc356266b8087c3ddaec235acdc1aca5adf0416202e82` |
| `test/e2e/configs/raypath_feature_random_315_focused_render.json` | `0778b5240fe6eac19519da5a1f917340b91948b0a98335dd3a6e20100a7428f1` |

The regular 3-1-5 scene is a unit regular prism with a uniform horizontal-axis
orientation, a D65 0.5-degree sun at altitude 15, and a `3-1-5` filter.
The overview is a 1024x512 equal-area dual fisheye at 5M rays. The focused
run has two 512x512 equal-area views (60 and 120 degrees) at 10M rays with
seed `20261002`. The target inputs fix the horizontal plate or randomized
prism/pyramid scene stated in their JSON.

## Command and output index

Set `OUT=/tmp/lumice-raypath-feature-acceptance` before running the commands.
Each recorded command exited 0. The retained measurement log contains
approximately 0.04 s, 0.01 s, and 0.00 s for the three report commands; the
5M PNG and raw NPY commands each took about 8.7 s; the two-view focused render
took 17.28 s. Exact wall-clock values for the remaining target commands were
not retained and are intentionally not reconstructed.

| Command | Events / seed | Recorded outputs |
|---|---|---|
| `Lumice raypath -f test/e2e/configs/raypath_feature_random_regular.json --crystal 1 --path 3-1-5 --report --events 8192 -o $OUT/report-random-315.json` | 8192 | `report-random-315.json` |
| `Lumice raypath -f test/e2e/configs/raypath_feature_random_regular.json --crystal 1 --path 3-5 --report --events 8192 -o $OUT/report-random-35.json` | 8192 | `report-random-35.json` |
| `Lumice raypath -f test/e2e/configs/raypath_feature_rhombic_plate.json --crystal 1 --path 1-3-4-2 --report --events 8192 -o $OUT/report-rhombic-1342.json` | 8192 | `report-rhombic-1342.json` |
| `Lumice raypath -f test/e2e/configs/raypath_feature_rhombic_plate.json --crystal 1 --path 1-3-5-2 --report --events 8192 -o $OUT/root-blue-1352.json` | 8192 | `root-blue-1352.json` |
| `Lumice raypath -f test/e2e/configs/raypath_feature_plate_target.json --crystal 1 --path 1-4-5-2 --target 20,120 --events 200k --grid 180 -o $OUT/target-plate-1452.json` | 200k | `target-plate-1452.json` |
| `Lumice raypath -f test/e2e/configs/raypath_feature_plate_target.json --crystal 1 --path 1-2 --target 20,120 --events 1k --grid 0 -o $OUT/target-plate-rank0.json` | 1k | `target-plate-rank0.json` |
| `Lumice raypath -f test/e2e/configs/raypath_feature_canonical_target.json --crystal 1 --path 1-3 --target '-45,0' --events 100k --grid 0 -o $OUT/target-prism-13-two-arcs.json` | 100k | `target-prism-13-two-arcs.json` |
| `Lumice raypath -f test/e2e/configs/raypath_feature_canonical_target.json --crystal 1 --path 3-5-6-7 --target '-51.133896651649145,180' --events 100k --grid 0 -o $OUT/target-prism-3567-open.json` | 100k | `target-prism-3567-open.json` |
| `Lumice raypath -f test/e2e/configs/raypath_feature_canonical_target.json --crystal 2 --path 13-15-26-28 --target '-72.3315778571731,0' --events 100k --grid 0 -o $OUT/target-pyramid-empty.json` | 100k | `target-pyramid-empty.json` |
| `Lumice render -f test/e2e/configs/raypath_feature_random_315_render.json -o $OUT/render-315 --format png --seed 20261002` | 5M / 20261002 | `render-315/img_01.png` |
| `Lumice render -f test/e2e/configs/raypath_feature_random_315_render.json -o $OUT/render-315-raw --format npy --raw-normalization normalized --seed 20261002` | 5M / 20261002 | `render-315-raw/img_01.npy`, `render-315-raw/img_01.json` |
| `Lumice -f test/e2e/configs/raypath_feature_random_315_focused_render.json -o $OUT/render-315-focused --seed 20261002` | 10M / 20261002 | `render-315-focused/img_01.jpg`, `render-315-focused/img_02.jpg` |

The report/target JSON and scientific image outputs are intentionally temporary
acceptance evidence, not tracked reference assets. Their preserved digests are:

| Output | SHA-256 |
|---|---|
| `report-random-315.json` | `aa4fc0cc98441e0153e61795cb52064dbc3c5343b4e1c1cf489ce98b10aa040a` |
| `report-random-35.json` | `ac4af3c8c07ddc7db1ecfa181eb117003f18226aebac1349d488292ba6ecd381` |
| `report-rhombic-1342.json` | `8d1fa0c786adfa2e8939fabaaf6b0ba62fa74824aef9c921d4a5d68c79170320` |
| `root-blue-1352.json` | `de42256063ff8d4bb8c9059c5bbf6b974ae433a8e876d91092d896073310d996` |
| `target-plate-1452.json` | `5ed430a6946726af4fba96f95a911492125addaa8ca713a04bfe123229f61075` |
| `target-plate-rank0.json` | `d958dc3641950606af9f8f70e4301120f95223a54191bd0e2a6c96e78e0eb286` |
| `target-prism-13-two-arcs.json` | `40c4c5c9ec66c59a9300e8fb2ba91b8ac7a74a74f7952a322067a5b3e02f16b9` |
| `target-prism-3567-open.json` | `94d5b4a4b2d34d9bd865e7abb1868de33644fce25d16f712dd267a7433d3867e` |
| `target-pyramid-empty.json` | `4b8a1f0ee84fabb2453ca07e9c5b961121ad7be32bc65649fd43c966fc29449c` |
| `render-315/img_01.png` | `f4be532cb7f9183e15f115c0e6504dbdca40254af264786f38eec5f8c636e3f5` |
| `render-315-raw/img_01.npy` | `ac8c889dd851bdff26543e39ec08bd33993f1d3af57a1b3e0ae3f3e7b9c3f3f5` |
| `render-315-raw/img_01.json` | `69e5b38aae7fb04ae2bb72017fae8fac91a0bc07bc46842edbcc4095f6a6ab68` |
| `render-315-focused/img_01.jpg` | `37bb9e623efc3220f5151991a3d8ee321f935264b3573d0164106ab0fb8eda9c` |
| `render-315-focused/img_02.jpg` | `14dfca5322423620c2cfcfa8c3e0c4701ccfb41072459452cd5c7df7b4ec8d2c` |

## Accepted observations

| Case | Product observation | Scope and interpretation |
|---|---|---|
| Random regular `3-1-5` | `solar_dispersion_edge` is confirmed at 21.612019265 and 22.371148713 degrees; `solar_caustic_candidate` remains a candidate. | The edge is a minimum-deviation dispersion result, not a confirmed Jacobian caustic. |
| Same path, antisolar side | `antisolar_tir_blue_band` is confirmed from 130.358885186 to 138.854666882 degrees; production blue/red is above one and the no-internal-R comparison is below one. `exit_gate.visible` is false. | Separate features prevent a primary verdict from hiding either result. |
| Random regular `3-5` | One confirmed ordinary minimum-deviation edge has the same red/blue boundary values. | The numeric tolerance is `1e-5` degrees, from the analytic fixed reference. |
| Horizontal rhombic plate, white 1342 class | Relative solar azimuths are -120 and +120 degrees, spherical separation is 117.599764152 degrees, and the physical L2 members are `1-3-4-2` and `1-3-8-2`. Red and blue finite-crystal `A*T` are 0.000870121498 and 0.000891004053 (blue/red about 1.024). | This is the fixed rhombic-prism shape, 9-degree sun, ideal horizontal `Rz` family, two named wavelengths and 8192 samples; it separately checks azimuth labels, sphere distance, colour samples and fixed outgoing direction. |
| Horizontal rhombic plate, blue 1352 class | The physical L2 members are `1-3-5-2` and `1-3-7-2`. Each has red `A*T` 0.0001104536442463968 and blue `A*T` 0.0001803007775026744, or blue/red 1.6323660367462811; the same positions are at relative solar azimuths -120 and +120 degrees and 117.599764152-degree spherical separation. | This is a tint difference at one fixed sky position, not a spatial red/blue edge and not the aggregate L1-class ratio. It applies only to the fixed rhombic-prism shape, 9-degree sun, ideal horizontal `Rz` family, two wavelengths and 8192 samples. |
| Fixed target cases | The finite arc has 13 samples and infeasible endpoints; the other target cases show two arcs, a TIR-bounded open arc, an empty cone result and rank-0 `point_mass`. | Empty or complete target results do not establish full enumeration or physical impossibility. |

## Probe and compatibility

Two distinct red probes were executed and are not interchangeable:

- The earlier limitation-text probe changed the all-sky limitation text and made
  `test_report_states_the_feature_families_it_does_not_enumerate` fail.
- The later core probe temporarily changed the production
  `solar_dispersion_edge` id. It made
  `test_random_315_report_is_a_separate_document_with_both_feature_mechanisms`
  fail with the expected missing-feature `KeyError`; restoring the id and
  rebuilding left all 12 report-CLI tests passing.

The acceptance changed no CLI, JSON schema or `--warm` contract. Existing
report-option rejection tests retain the target-only and legacy-option boundary.
