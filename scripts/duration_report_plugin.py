"""pytest plugin: write each test's wall-clock cost to a JSON file.

Loaded explicitly by CI (`PYTHONPATH=scripts pytest -p duration_report_plugin
--duration-report=PATH ...`) and nowhere else, so an unwired pytest call is not
touched by it. With `--duration-report` absent it registers nothing.

Two kinds of entry, both `{id: seconds}` in one JSON object:

- a test's nodeid -> its setup + call + teardown, MINUS the setup of any
  module-, class-, package- or session-scoped fixture that happened to be
  created during its setup phase;
- `<fixture baseid>::<fixture:NAME>` (plus `[<param index>]` for a
  parametrized fixture) -> that shared fixture's own setup time, the largest
  one seen if it was created more than once (once per xdist worker).

Why the split: a shared fixture's setup is paid by whichever test first
requests it on a given process, and under pytest-xdist's default `--dist load`
which test that is changes from run to run (measured: the same module fixture
was charged to a different parametrized case on each of three `-n 3` runs).
Charging it to the test would make one test's number jump by the fixture's
cost between runs; keying it by the fixture makes both numbers stable without
pinning the scheduler (`--dist loadfile` would also stabilise it, at the price
of putting a whole heavy file on one worker). Setup is counted, not dropped,
because that is where this tree's most expensive fixtures spend their time.

Every test that produced a report is present, skipped ones included (they read
~0), so `scripts/check_test_durations.py` can tell "this registered test no
longer exists" apart from "this test was skipped on this runner".

Under xdist each worker times its own fixtures and attaches them to the setup
report through `user_properties`, which xdist forwards to the controller; the
controller alone holds the complete set and is the only process that writes
the file (a worker writing too would race it with a partial file).
"""

import json
import time

import pytest

_PROPERTY = "duration_report_shared_setup"


class _SharedFixtureTimer:
    """Runs wherever tests execute (serial process or xdist worker)."""

    def __init__(self):
        self._item = None

    @pytest.hookimpl(wrapper=True)
    def pytest_runtest_setup(self, item):
        self._item = item
        try:
            return (yield)
        finally:
            self._item = None

    @pytest.hookimpl(wrapper=True)
    def pytest_fixture_setup(self, fixturedef, request):
        # Dependencies are resolved before this hook runs, so the time measured
        # here is this fixture's own body, never its dependencies' again.
        start = time.perf_counter()
        try:
            return (yield)
        finally:
            if fixturedef.scope != "function" and self._item is not None:
                key = f"{fixturedef.baseid}::<fixture:{fixturedef.argname}>"
                if hasattr(request, "param"):
                    key += f"[{request.param_index}]"
                self._item.user_properties.append((_PROPERTY, [key, time.perf_counter() - start]))


class _DurationRecorder:
    """Runs only in the process that writes the file (serial or controller)."""

    def __init__(self, path):
        self._path = path
        self._totals = {}

    def pytest_runtest_logreport(self, report):
        seconds = report.duration
        if report.when == "setup":
            # item.user_properties is copied into every later report of the same
            # test too, so the shared setups are read off the setup report only.
            for name, value in report.user_properties:
                if name != _PROPERTY:
                    continue
                key, fixture_seconds = value
                seconds -= fixture_seconds
                self._totals[key] = max(self._totals.get(key, 0.0), fixture_seconds)
        self._totals[report.nodeid] = self._totals.get(report.nodeid, 0.0) + max(seconds, 0.0)

    def pytest_sessionfinish(self, session):
        with open(self._path, "w", encoding="utf-8") as f:
            json.dump(dict(sorted(self._totals.items())), f, indent=1)
            f.write("\n")


def pytest_addoption(parser):
    parser.addoption(
        "--duration-report",
        default=None,
        metavar="PATH",
        help="write {test or shared-fixture id: seconds} JSON to PATH",
    )


def pytest_configure(config):
    path = config.getoption("--duration-report")
    if not path:
        return
    # The timer is inert in an xdist controller, which runs no tests.
    config.pluginmanager.register(_SharedFixtureTimer(), "duration_report_timer")
    # `workerinput` exists only in an xdist worker process.
    if not hasattr(config, "workerinput"):
        config.pluginmanager.register(_DurationRecorder(path), "duration_report_recorder")
