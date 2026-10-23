from __future__ import annotations

import json
from pathlib import Path

import yaml

REPO_ROOT = Path(__file__).resolve().parents[3]
WORKFLOW_PATH = REPO_ROOT / ".github" / "workflows" / "ci.yml"
REGISTRY_PATH = REPO_ROOT / "test" / "duration_registry.json"
ENGLISH_DOC_PATH = REPO_ROOT / "doc" / "testing" / "testing-architecture.md"
CHINESE_DOC_PATH = REPO_ROOT / "doc" / "testing" / "testing-architecture_zh.md"
AGENTS_PATH = REPO_ROOT / "AGENTS.md"
AUTH_EXPRESSION = (
    "${{ github.event_name == 'pull_request' && github.run_attempt == 1 && "
    "contains(github.event.pull_request.labels.*.name, 'ci:allow-cold-build') }}"
)


def load_workflow():
    return yaml.load(WORKFLOW_PATH.read_text(encoding="utf-8"), Loader=yaml.BaseLoader)


def named_steps(job):
    return {step["name"]: step for step in job["steps"] if "name" in step}


def authorized(event: str, attempt: int, labels: list[str]) -> bool:
    return event == "pull_request" and attempt == 1 and "ci:allow-cold-build" in labels


def test_cold_build_authorization_is_single_use_and_single_owner():
    workflow_text = WORKFLOW_PATH.read_text(encoding="utf-8")
    workflow = load_workflow()
    build = workflow["jobs"]["build"]
    assert build["env"]["LUMICE_ALLOW_COLD_BUILD"] == AUTH_EXPRESSION
    assert workflow_text.count("ci:allow-cold-build") == 1
    assert workflow_text.count("--allow-cold-build") == 1
    assert authorized("pull_request", 1, ["ci:allow-cold-build"])
    assert not authorized("pull_request", 2, ["ci:allow-cold-build"])
    assert not authorized("pull_request", 1, [])
    assert not authorized("push", 1, ["ci:allow-cold-build"])

    pull_request_trigger = workflow["on"]["pull_request"]
    assert not isinstance(pull_request_trigger, dict) or "types" not in pull_request_trigger


def test_authorization_audit_precedes_build_and_duration_gate_consumes_the_value():
    workflow = load_workflow()
    build = workflow["jobs"]["build"]
    steps = build["steps"]
    names = [step.get("name") for step in steps]
    assert names.index("Audit cold-build authorization") < names.index("Configure") < names.index("Build")

    audit = named_steps(build)["Audit cold-build authorization"]["run"]
    assert all(value in audit for value in ("GITHUB_EVENT_NAME", "PR_NUMBER", "EVENT_HEAD_SHA", "GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT"))
    assert "LUMICE_ALLOW_COLD_BUILD" in audit
    assert "ci:allow-cold-build" not in audit

    check = named_steps(build)["Collect and check CTest durations"]["run"]
    assert 'if [ "$LUMICE_ALLOW_COLD_BUILD" = "true" ]' in check
    assert "duration_args+=(--allow-cold-build)" in check
    assert '--summary "$GITHUB_STEP_SUMMARY"' in check

    for job_name in ("e2e-test", "e2e-slow"):
        for step in workflow["jobs"][job_name]["steps"]:
            assert "--allow-cold-build" not in step.get("run", "")


def test_compiler_seed_is_verified_and_saved_before_tests():
    build = load_workflow()["jobs"]["build"]
    steps = build["steps"]
    names = [step.get("name") for step in steps]
    by_name = named_steps(build)
    assert by_name["Build"]["id"] == "build"

    for tool in ("ccache", "sccache"):
        prepare = f"Prepare {tool} seed (main only)"
        stats = f"{tool} statistics (Windows MSVC)" if tool == "sccache" else "ccache statistics (Ubuntu x86_64 / ARM64, macOS)"
        save = f"Save {tool} directory (main only)"
        assert names.index("Build") < names.index(prepare) < names.index(stats) < names.index(save) < names.index("Test")
        assert f"--tool {tool}" in by_name[prepare]["run"]
        assert "steps.build.outcome == 'success'" in by_name[prepare]["if"]
        condition = by_name[save]["if"]
        assert all(
            clause in condition
            for clause in (
                "!cancelled()",
                "github.event_name == 'push'",
                "github.ref == 'refs/heads/main'",
                "steps.build.outcome == 'success'",
                f"steps.{tool}_ready.outcome == 'success'",
                f"steps.{tool}_restore.outputs.cache-hit != 'true'",
            )
        )
        assert by_name[save]["with"]["key"] == f"${{{{ steps.{tool}_restore.outputs.cache-primary-key }}}}"

    assert names.index("Test") < names.index("Save CPM dependencies (main only)")


def test_every_non_build_member_of_an_exceptable_aggregate_has_its_own_budget():
    registry = json.loads(REGISTRY_PATH.read_text(encoding="utf-8"))
    budgets = registry["phase_budgets"]
    direct = {(budget["job"], budget["phase"]) for budget in budgets}
    aggregates = [budget for budget in budgets if "build" in budget.get("members", [])]
    assert aggregates
    for budget in aggregates:
        for member in budget["members"]:
            if member != "build":
                assert (budget["job"], member) in direct


def test_gui_visual_regression_contract_is_unchanged():
    build = load_workflow()["jobs"]["build"]
    step = named_steps(build)["GUI visual regression (Xvfb + Mesa llvmpipe)"]
    assert 'timeout-minutes' in step and step["timeout-minutes"] == "10"
    assert '--filter "modal_layout,defaults_panel_layout"' in step["run"]
    assert "xvfb-run -a build/Release/static/bin/gui_test" in step["run"]


def test_per_miss_collection_runs_on_every_cache_leg_strictly():
    workflow = load_workflow()
    build = workflow["jobs"]["build"]
    steps = build["steps"]
    names = [step.get("name") for step in steps]
    collect = named_steps(build)["Collect compiler-cache statistics"]
    assert collect["if"] == "always() && (matrix.ccache || matrix.msvc)"
    # Every leg judges per-miss now, so no leg may degrade collection to a
    # warning: a miss count that cannot be read is a red on the leg it would
    # have judged (fail-closed), never a silent skip.
    assert "continue-on-error" not in collect
    run = collect["run"]
    assert "collect_cache_stats.py" in run
    assert "${{ matrix.ccache && 'ccache' || 'sccache' }}" in run
    assert "--phase build" in run
    assert "--report ci-duration/cache-stats.json" in run
    # After Build (the measurement exists) and before the diagnostics that stop
    # the sccache server (the counters must still be readable on Windows).
    assert (
        names.index("Build")
        < names.index("Prepare sccache seed (main only)")
        < names.index("Collect compiler-cache statistics")
        < names.index("sccache statistics (Windows MSVC)")
    )


def test_every_judging_leg_passes_the_cache_report_to_the_checker():
    registry = json.loads(REGISTRY_PATH.read_text(encoding="utf-8"))
    per_miss_jobs = {budget["job"] for budget in registry["phase_budgets"] if "per_miss" in budget}
    # All four build-matrix legs carry a per_miss build entry, so all four
    # consume the report — and the report must be passed unconditionally, with
    # no leg-name guard left to desynchronize the workflow from the registry.
    assert per_miss_jobs == {
        "Ubuntu x86_64",
        "Ubuntu ARM64",
        "macOS ARM64",
        "Windows MSVC x86_64",
    }

    check = named_steps(load_workflow()["jobs"]["build"])["Collect and check CTest durations"]["run"]
    assert 'if [ "${{ matrix.name }}"' not in check
    assert "--cache-report ci-duration/cache-stats.json" in check
    workflow_text = WORKFLOW_PATH.read_text(encoding="utf-8")
    assert workflow_text.count("--cache-report ci-duration/cache-stats.json") == 1
    for job_name in ("e2e-test", "e2e-slow"):
        for step in workflow_text_jobs(job_name):
            assert "--cache-report" not in step.get("run", "")


def workflow_text_jobs(job_name):
    return load_workflow()["jobs"][job_name]["steps"]


def test_registry_v4_per_miss_shape_and_terminal_values():
    registry = json.loads(REGISTRY_PATH.read_text(encoding="utf-8"))
    assert registry["version"] == 4

    # The four build entries' two-condition shape and re-anchored outer bounds.
    expected = {
        "Ubuntu x86_64": ({"overhead_seconds": 30, "max_seconds_per_miss": 5.0}, 1800, 2160),
        "Ubuntu ARM64": ({"overhead_seconds": 15, "max_seconds_per_miss": 5.0}, 1020, 1380),
        "macOS ARM64": ({"overhead_seconds": 20, "max_seconds_per_miss": 5.0}, 1200, 1680),
        "Windows MSVC x86_64": ({"overhead_seconds": 105, "max_seconds_per_miss": 5.0}, 2640, 3120),
    }
    for job, (per_miss, outer, agg_limit) in expected.items():
        build = next(
            budget for budget in registry["phase_budgets"]
            if budget["job"] == job and budget["phase"] == "build"
        )
        assert build["per_miss"] == per_miss, job
        assert build["max_seconds"] == outer, job
        assert "members" not in build, job
        aggregate = next(
            budget for budget in registry["phase_budgets"]
            if budget["job"] == job and budget["phase"] == "build+ctest"
        )
        assert aggregate["max_seconds"] == agg_limit, job
        assert aggregate["members"] == ["build", "ctest"], job
        # No transitional value survives: the per-miss judgment replaced them.
        assert "Transitional" not in build["reason"], job
        assert "Transitional" not in aggregate["reason"], job
        # An aggregate is the owner's wall clock, not compile cost.
        assert "per_miss" not in aggregate, job
    assert all("members" not in budget for budget in registry["phase_budgets"] if "per_miss" in budget)

    # The four ctest budgets carry the machine_factor block (registry v4); no
    # other phase does — the normalization prices the host on the test phase
    # only, and the owner's wall-clock aggregates stay absolute.
    ctest = [b for b in registry["phase_budgets"] if b["phase"] == "ctest"]
    assert {b["job"] for b in ctest} == {
        "Ubuntu x86_64", "Ubuntu ARM64", "macOS ARM64", "Windows MSVC x86_64",
    }
    for budget in ctest:
        machine = budget["machine_factor"]
        assert len(machine["ruler_seconds"]) == 7, budget["job"]
        assert all(v > 0 for v in machine["ruler_seconds"].values()), budget["job"]
        assert machine["clamp_band"] == [0.8, 2.0], budget["job"]
    assert all("machine_factor" not in b for b in registry["phase_budgets"] if b["phase"] != "ctest")


def test_english_doc_is_the_single_protocol_authority():
    english = ENGLISH_DOC_PATH.read_text(encoding="utf-8")
    chinese = CHINESE_DOC_PATH.read_text(encoding="utf-8")
    agents = AGENTS_PATH.read_text(encoding="utf-8")
    assert english.count("### §7.8 The one-run cold-build exception") == 1
    assert "testing-architecture.md#78-the-one-run-cold-build-exception" in chinese
    assert "测试运行耗时、测试失败、编译/链接失败及坏报告\n一律没有豁免" in chinese
    assert "不得用 whole-run 或单 job rerun 重复消费" in chinese
    assert "cold-build protocol's\n  single authority is §7.8" in agents
    assert "it never excuses test runtime or correctness" in agents
    assert "a PR whole-run/job\n  rerun cannot consume it again" in agents
