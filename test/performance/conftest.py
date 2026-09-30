"""pytest options for the throughput gates in this directory.

``--dual-gate-profile`` picks which dual-renderer gate the Metal file asserts:
``precise`` (the default — reference machines and the local scheduled gate) or
``ci`` (the loose disaster floor the GitHub-hosted runner can hold). The numbers
and the reasoning behind both live in ``test/e2e/_multi_renderer_throughput.py``.
An option rather than an environment variable, so the profile a run used is
visible on its command line.
"""

from test.e2e._multi_renderer_throughput import DEFAULT_PROFILE, PROFILES


def pytest_addoption(parser):
    parser.addoption(
        "--dual-gate-profile",
        choices=PROFILES,
        default=DEFAULT_PROFILE,
        help="dual-renderer throughput gate profile (default: %(default)s)",
    )
