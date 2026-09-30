"""Self-check for ``test/e2e/_config_overrides.write_config_with_ray_num``.

The slow parity tests that shrink a shared fixture's ray budget go through this
helper, and none of them can tell a copy that kept the old budget from one that
took the new one — they would just stay green at the old cost. So the override
itself is checked here, on every fixture that is read through it, without a build.
"""

from __future__ import annotations

import json

import pytest

from test.e2e._config_overrides import write_config_with_ray_num
from test.e2e.runner import get_project_root

_CONFIGS_DIR = get_project_root() / "test" / "e2e" / "configs"


@pytest.mark.parametrize(
    "name", ["ms_multi_crystal", "illuminant_wavelength_parity", "multi_lens", "cpu_backend_route"]
)
def test_only_the_ray_budget_changes(tmp_path, name):
    src = _CONFIGS_DIR / f"{name}.json"
    original = json.loads(src.read_text(encoding="utf-8"))
    new_budget = original["scene"]["ray_num"] // 4

    copy = write_config_with_ray_num(src, tmp_path, new_budget)

    assert copy.parent == tmp_path
    written = json.loads(copy.read_text(encoding="utf-8"))
    assert written["scene"]["ray_num"] == new_budget
    written["scene"]["ray_num"] = original["scene"]["ray_num"]
    assert written == original, "a field other than scene.ray_num changed in the copy"
    assert json.loads(src.read_text(encoding="utf-8")) == original, "the shared source was modified"
