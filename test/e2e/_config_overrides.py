"""Per-test copies of a shared fixture config with one field overridden.

Several configs under ``test/e2e/configs/`` are read by more than one test, and a
test that wants a smaller ray budget than its neighbours must not edit the shared
file to get it — the other readers (a CUDA mirror of the same parity check, the
C API smoke test, ...) were calibrated on the committed value. The override is
therefore written into a copy the caller owns, and this module is the one place
that does it. A config with a single reader needs none of this: change its
``ray_num`` in place.

The fixture configs carry no relative file references, so a copy in another
directory loads exactly as the original does.
"""

from __future__ import annotations

import json
from pathlib import Path


def write_config_with_ray_num(src: Path, out_dir: Path, ray_num: int) -> Path:
    """Copy ``src`` into ``out_dir`` with ``scene.ray_num`` set to ``ray_num``; return the copy.

    Every other field is carried over unchanged. The copy's name records the
    override, so two budgets of one config can share a directory.
    """
    doc = json.loads(Path(src).read_text(encoding="utf-8"))
    doc["scene"]["ray_num"] = ray_num
    out = Path(out_dir) / f"{Path(src).stem}_ray{ray_num}.json"
    out.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
    return out
