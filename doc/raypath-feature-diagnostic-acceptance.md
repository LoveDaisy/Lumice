# Raypath Feature Diagnostic Acceptance Record

This record captures a fixed-input product acceptance run. Its image evidence is
observational, not a cross-machine visual golden or a claim about all scenes.

## Reproduction envelope

- Binary: `build/cmake_install/static/Lumice`.
- Report inputs: `test/e2e/configs/raypath_feature_random_regular.json` and
  `test/e2e/configs/raypath_feature_rhombic_plate.json`, with `--events 8192`.
- The 3-1-5 render used a regular prism (height and all face distances 1), a
  uniform horizontal-axis orientation, a D65 0.5 degree sun at altitude 15,
  and a `3-1-5` raypath filter. The overview was a 1024x512 equal-area dual
  fisheye at 5M rays; two 512x512 equal-area views (60 and 120 degrees) used
  10M rays and seed `20261002`.
- The fixed target checks used a horizontal plate with height 0.3 at sun
  altitude 20, plus a randomized prism/pyramid target scene at sun altitude
  15. They ran at 1k--200k events as appropriate to the target query.

Representative report commands are:

```bash
build/cmake_install/static/Lumice raypath -f test/e2e/configs/raypath_feature_random_regular.json --crystal 1 --path 3-1-5 --report --events 8192
build/cmake_install/static/Lumice raypath -f test/e2e/configs/raypath_feature_random_regular.json --crystal 1 --path 3-5 --report --events 8192
build/cmake_install/static/Lumice raypath -f test/e2e/configs/raypath_feature_rhombic_plate.json --crystal 1 --path 1-3-4-2 --report --events 8192
```

## Accepted observations

| Case | Product observation | Scope and interpretation |
|---|---|---|
| Random regular `3-1-5` | `solar_dispersion_edge` is confirmed at 21.612019265 and 22.371148713 degrees; `solar_caustic_candidate` remains a candidate. | The edge is a minimum-deviation dispersion result, not a confirmed Jacobian caustic. |
| Same path, antisolar side | `antisolar_tir_blue_band` is confirmed from 130.358885186 to 138.854666882 degrees. The production blue/red ratio is above one; without internal R it is below one. `exit_gate.visible` is false. | These are separate report features, so no primary verdict can hide either one. |
| Random regular `3-5` | One confirmed ordinary minimum-deviation edge has the same red/blue boundary values. | The numeric tolerance is `1e-5` degrees, from the analytic fixed reference. |
| Horizontal rhombic plate | Relative solar azimuths are -120 and +120 degrees while spherical separation is 117.599764152 degrees. The physical L2 members are exactly `1-3-4-2` and `1-3-8-2`; both contain the 694.36 nm and 430.02 nm samples with positive `A*T` and fixed-direction residual below `1e-12` rad. | Azimuth labels, spherical distance, colour samples, and fixed outgoing direction are distinct checks. |
| Fixed `1-4-5-2` target | One finite arc had 13 samples, positive entry measure, and `path_infeasible` endpoints. | This establishes the sampled target/fiber result only; it does not turn another path-class statistic into evidence for this path. |
| Open/multiple, cone, and rank 0 cases | Target queries demonstrated two arcs, one TIR-bounded open arc, an empty cone result, and a rank-0 `point_mass`. | Empty or complete target results do not claim full feature enumeration or physical impossibility. The report explicitly does not enumerate open/multiple components, general oriented kinks, cone-empty results, or rank-0 discovery. |

The recorded product JSON SHA-256 values were
`aa4fc0cc98441e0153e61795cb52064dbc3c5343b4e1c1cf489ce98b10aa040a`
(random 3-1-5 report),
`8d1fa0c786adfa2e8939fabaaf6b0ba62fa74824aef9c921d4a5d68c79170320`
(rhombic report), and
`5ed430a6946726af4fba96f95a911492125addaa8ca713a04bfe123229f61075`
(fixed target). The 5M overview and 10M focused renders took about 8.7 s and
17.3 s respectively on the acceptance machine.

## Probe and compatibility

Temporarily changing the production `solar_dispersion_edge` feature id made
`test_random_315_report_is_a_separate_document_with_both_feature_mechanisms`
fail with its expected missing-feature `KeyError`; restoring the production id
and rebuilding produced 12 passing report-CLI tests. This is a single-feature
red probe of the solar-side product assertion, not a limitation-text-only test.
The acceptance changed no CLI, JSON schema, or `--warm` contract; the existing
report-option rejection tests retain the target-only and legacy-option boundary.
