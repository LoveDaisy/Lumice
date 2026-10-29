#include "raypath/query_control.hpp"

#include <atomic>

#include "raypath/detail/schema3/report_assembly.hpp"
#include "raypath/map_analysis.hpp"
#include "raypath/probe_analysis.hpp"

namespace lumice::raypath {

namespace {

constexpr int kWindowCount = 6;
std::atomic<long> g_window_counts[kWindowCount]{};
std::atomic<long> g_query_count{ 0 };

int WindowIndex(BudgetWindow window) {
  switch (window) {
    case BudgetWindow::kPointDiscovery:
      return 0;
    case BudgetWindow::kProbeLambdas:
      return 1;
    case BudgetWindow::kQualityDeadline:
      return 2;
    case BudgetWindow::kHangCap:
      return 3;
    case BudgetWindow::kLegGuardrail:
      return 4;
    case BudgetWindow::kMapMembers:
      return 5;
  }
  return -1;
}

}  // namespace

LegControl ResolveLeg(BudgetWindow window, const BudgetRequest& request) {
  LegControl control;
  const auto start = request.start.value_or(std::chrono::steady_clock::now());
  switch (window) {
    // Point's documented zero: no deadline, echoed 0 — the one kind whose internal contract
    // is an explicit unbounded run.
    case BudgetWindow::kPointDiscovery:
      control.stop.deadline = request.budget_ms > 0 ? start + std::chrono::milliseconds(request.budget_ms) :
                                                      std::chrono::steady_clock::time_point::max();
      control.stop.interrupted = request.interrupted;
      control.resolved_ms = request.budget_ms > 0 ? request.budget_ms : 0;
      break;
    // Probe's documented zero: the default applies and the document echoes the resolved value.
    case BudgetWindow::kProbeLambdas: {
      control.resolved_ms = request.budget_ms > 0 ? request.budget_ms : kDefaultProbeBudgetMs;
      control.stop.deadline = start + std::chrono::milliseconds(control.resolved_ms);
      control.stop.interrupted = request.interrupted;
      break;
    }
    // The map's documented zero: the same shape as the probe's — the default applies and the
    // document echoes the resolved value; polled at the per-member boundaries.
    case BudgetWindow::kMapMembers: {
      control.resolved_ms = request.budget_ms > 0 ? request.budget_ms : kDefaultMapBudgetMs;
      control.stop.deadline = start + std::chrono::milliseconds(control.resolved_ms);
      control.stop.interrupted = request.interrupted;
      break;
    }
    // The report's quality window: the C entry pins the absolute deadline at its own start;
    // an internal caller without one gets begin + budget (the budget is validated positive
    // before this call — this window has no zero rule of its own).
    case BudgetWindow::kQualityDeadline:
      control.stop.deadline = request.absolute_deadline.value_or(start + std::chrono::milliseconds(request.budget_ms));
      control.stop.interrupted = request.interrupted;
      control.resolved_ms = request.budget_ms;
      break;
    // The enumeration leg's enforced robustness floor: fixed span, deliberately not coupled
    // to any request budget (report_assembly.hpp owns the constant and its measured basis).
    case BudgetWindow::kHangCap:
      control.stop.deadline = start + std::chrono::milliseconds(schema3::kEnumerationHangCapMs);
      control.stop.interrupted = request.interrupted;
      control.resolved_ms = static_cast<int>(schema3::kEnumerationHangCapMs);
      break;
    // The MC ESS leg's own guardrail: the survey request's budget, verbatim, from the leg's
    // own start (the leg is budget-bounded; the survey's other legs are recorded, not
    // enforced, and resolve nothing).
    case BudgetWindow::kLegGuardrail:
      control.stop.deadline = start + std::chrono::milliseconds(request.budget_ms);
      control.stop.interrupted = request.interrupted;
      control.resolved_ms = request.budget_ms;
      break;
  }
  g_window_counts[WindowIndex(window)].fetch_add(1, std::memory_order_relaxed);
  return control;
}

QueryDeadline ResolveQueryDeadline(const BudgetRequest& request) {
  QueryDeadline deadline;
  const auto start = request.start.value_or(std::chrono::steady_clock::now());
  if (request.absolute_deadline.has_value()) {
    deadline.at = *request.absolute_deadline;
    deadline.resolved_ms = request.budget_ms;
  } else if (request.budget_ms > 0) {
    deadline.at = start + std::chrono::milliseconds(request.budget_ms);
    deadline.resolved_ms = request.budget_ms;
  } else {
    // Unbounded by explicit request: the resolved span reads 0, the point contract's echo
    // for "no deadline in force".
    deadline.at = std::chrono::steady_clock::time_point::max();
    deadline.resolved_ms = 0;
  }
  deadline.interrupted = request.interrupted;
  g_query_count.fetch_add(1, std::memory_order_relaxed);
  return deadline;
}

long BudgetResolutionCount(BudgetWindow window) {
  return g_window_counts[WindowIndex(window)].load(std::memory_order_relaxed);
}

long QueryDeadlineResolutionCount() {
  return g_query_count.load(std::memory_order_relaxed);
}

void ResetBudgetResolutionCountsForTest() {
  for (auto& count : g_window_counts) {
    count.store(0, std::memory_order_relaxed);
  }
  g_query_count.store(0, std::memory_order_relaxed);
}

}  // namespace lumice::raypath
