# Provenance — the warm fixtures

The two survey documents are byte-exact products of this tree's own CLI, generated from the two
scene configs beside them (from the repo root):

    ./build/Release/static/bin/Lumice raypath survey \
        -f test/fixtures/raypath-warm/warm_random_scene.json --crystal 1 --path 3-5 \
        -o test/fixtures/raypath-warm/warm_random_survey.json
    ./build/Release/static/bin/Lumice raypath survey \
        -f test/fixtures/raypath-warm/warm_sun33_scene.json --crystal 1 --path 3-5 \
        -o test/fixtures/raypath-warm/warm_sun33_survey.json

- generator: 4.7.4-dev, feat/raypath-point-l2-warm @ 7ad4d901
- 11e883260f348bd59fc7eaadae5fa539a3967e7507eac9732bdd581c09de24a7  warm_random_scene.json
- 05cbc4a14376ba36ee70b2a6ee7df754e4e1941485627be49244b15874320e7a  warm_random_survey.json
- b2a20ecf88ad0ab3834123d11ff3ef874ffba2131742b1e6b5e7d8e7c8c954d8  warm_sun33_scene.json
- 0fee5697e023aa23305f1d8217c07affa4597dac26433aa305fef29a2b7411ed  warm_sun33_survey.json

Never hand-edit the survey documents: they are historical writer output by definition. Regenerate
both with the same commands whenever a change touches the warm contract's writer surface
(warm_index, the scope receipt, objects, probes, or the fingerprint projection) — the
config-fingerprint gate fails loudly when the projection changes and the assets do not. The
unit layer's writer-integration coverage does not depend on these files: the refused arm's
witness scene is produced live in-process (~40 ms), and the schema-1 sheet test is live too.
