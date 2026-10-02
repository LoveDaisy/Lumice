// Real-kernel probe used to choose the diagnostic-field C ABI shape. This is not a benchmark gate:
// it prints repeated timing and allocation evidence for a human-reviewed API decision.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <new>
#include <optional>
#include <thread>
#include <vector>

#include "analytic/diagnostic_field.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/so3.hpp"

namespace {

std::atomic<bool> g_count_allocations{ false };
std::atomic<size_t> g_allocation_count{ 0 };
std::atomic<size_t> g_allocation_bytes{ 0 };

void RecordAllocation(size_t size) {
  if (g_count_allocations.load(std::memory_order_relaxed)) {
    g_allocation_count.fetch_add(1, std::memory_order_relaxed);
    g_allocation_bytes.fetch_add(size, std::memory_order_relaxed);
  }
}

struct AllocationRecord {
  size_t count = 0;
  size_t bytes = 0;
};

template <typename Function>
AllocationRecord MeasureAllocations(Function&& function) {
  g_allocation_count.store(0, std::memory_order_relaxed);
  g_allocation_bytes.store(0, std::memory_order_relaxed);
  g_count_allocations.store(true, std::memory_order_release);
  function();
  g_count_allocations.store(false, std::memory_order_release);
  return { g_allocation_count.load(std::memory_order_relaxed), g_allocation_bytes.load(std::memory_order_relaxed) };
}

using Clock = std::chrono::steady_clock;

template <typename Function>
double MedianMicros(int repeats, int iterations, Function&& function) {
  std::vector<double> samples;
  samples.reserve(repeats);
  for (int repeat = 0; repeat < repeats; repeat++) {
    const auto start = Clock::now();
    for (int iteration = 0; iteration < iterations; iteration++) {
      function();
    }
    const auto end = Clock::now();
    samples.push_back(std::chrono::duration<double, std::micro>(end - start).count() / iterations);
  }
  std::sort(samples.begin(), samples.end());
  return samples[samples.size() / 2];
}

struct Fixture {
  lumice::analytic::FaceNormalTable normals;
  lumice::analytic::FacePolygonTable polygons;
  std::vector<int> faces{ 3, 5, 6, 7 };
  std::vector<int> slots = std::vector<int>(faces.size());
};

Fixture BuildFixture() {
  Fixture fixture;
  LUMICE_ANALYTIC_Crystal crystal{};
  crystal.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  crystal.height = 0.73;
  const double distances[6] = { 1.37, 0.91, 1.12, 1.46, 0.83, 1.05 };
  std::copy(distances, distances + 6, crystal.face_distance);
  if (lumice::analytic::BuildFaceNormals(crystal, &fixture.normals, &fixture.polygons) !=
          lumice::analytic::Status::kOk ||
      lumice::analytic::ResolveFaceSequence(fixture.normals, fixture.faces.data(),
                                            static_cast<int>(fixture.faces.size()),
                                            fixture.slots.data()) != lumice::analytic::Status::kOk) {
    std::abort();
  }
  return fixture;
}

std::vector<lumice::analytic::DiagnosticRowInput> BuildRows() {
  constexpr double kBasePose[9] = { 0.5930675096922245, -0.2054499789431593, -0.7784993481691032,
                                    0.7535048750192704, 0.4823493488687678,  0.4467320326191859,
                                    0.2837275669892805, -0.8515453081299498, 0.4408732878415432 };
  std::vector<lumice::analytic::DiagnosticRowInput> rows(64);
  for (size_t i = 0; i < rows.size(); i++) {
    auto& row = rows[i];
    row.refractive_index = 1.31 + 1.0e-4 * static_cast<double>(i % 7);
    row.incident_direction[0] = -0.9659258262890683;
    row.incident_direction[1] = 0.0;
    row.incident_direction[2] = -0.25881904510252074;
    const double delta[3] = { 2.0e-4 * static_cast<double>(i), -1.0e-4 * static_cast<double>(i % 5),
                              1.5e-4 * static_cast<double>(i % 3) };
    double increment[9];
    lumice::analytic::so3::Exp(delta, increment);
    lumice::analytic::so3::MatMul(kBasePose, increment, row.pose);
  }
  return rows;
}

double EvaluateRows(lumice::analytic::DiagnosticField* field,
                    const std::vector<lumice::analytic::DiagnosticRowInput>& rows, size_t count) {
  double checksum = 0.0;
  for (size_t i = 0; i < count; i++) {
    const auto result = field->Evaluate(rows[i]);
    checksum += result.outgoing_direction[0] + result.entry_measure + result.fresnel_weight;
  }
  return checksum;
}

}  // namespace

void* operator new(std::size_t size) {
  if (void* pointer = std::malloc(size)) {
    RecordAllocation(size);
    return pointer;
  }
  throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
  return ::operator new(size);
}

void operator delete(void* pointer) noexcept {
  std::free(pointer);
}

void operator delete[](void* pointer) noexcept {
  std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
  std::free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept {
  std::free(pointer);
}

int main() {
  const Fixture fixture = BuildFixture();
  const auto rows = BuildRows();
  volatile double checksum = 0.0;
  auto make_field = [&] {
    return lumice::analytic::DiagnosticField(fixture.normals, fixture.polygons, fixture.faces.data(),
                                             fixture.slots.data(), static_cast<int>(fixture.faces.size()));
  };

  std::optional<lumice::analytic::DiagnosticField> prepared;
  const AllocationRecord resident = MeasureAllocations([&] { prepared.emplace(make_field()); });
  checksum += EvaluateRows(&*prepared, rows, 1);
  const AllocationRecord steady_row = MeasureAllocations([&] { checksum += EvaluateRows(&*prepared, rows, 1); });

  std::cout << std::fixed << std::setprecision(2);
  std::cout << "host_threads=" << std::thread::hardware_concurrency() << '\n';
  std::cout << "prepared_retained_allocations=" << resident.count << " retained_bytes=" << resident.bytes << '\n';
  std::cout << "steady_row_allocations=" << steady_row.count << " allocated_bytes=" << steady_row.bytes << '\n';

  for (const size_t count : { 1u, 8u, 64u }) {
    const int iterations = count == 1 ? 200 : (count == 8 ? 30 : 4);
    const double prepared_us = MedianMicros(7, iterations, [&] { checksum += EvaluateRows(&*prepared, rows, count); });
    const double direct_us = MedianMicros(7, iterations, [&] {
      auto field = make_field();
      checksum += EvaluateRows(&field, rows, count);
    });
    std::cout << "batch=" << count << " prepared_us=" << prepared_us << " direct_us=" << direct_us
              << " prepare_share_pct=" << (direct_us - prepared_us) * 100.0 / direct_us << '\n';
  }

  constexpr int kThreadCount = 8;
  constexpr int kRowsPerThread = 8;
  auto run_threads = [&](bool shared_prepared) {
    std::mutex prepared_mutex;
    std::vector<std::thread> threads;
    std::vector<double> partials(kThreadCount);
    threads.reserve(kThreadCount);
    const auto start = Clock::now();
    for (int thread_index = 0; thread_index < kThreadCount; thread_index++) {
      threads.emplace_back([&, thread_index] {
        if (shared_prepared) {
          std::lock_guard<std::mutex> lock(prepared_mutex);
          partials[thread_index] = EvaluateRows(&*prepared, rows, kRowsPerThread);
        } else {
          auto field = make_field();
          partials[thread_index] = EvaluateRows(&field, rows, kRowsPerThread);
        }
      });
    }
    for (auto& thread : threads) {
      thread.join();
    }
    for (double partial : partials) {
      checksum += partial;
    }
    return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
  };
  const double shared_locked_us = MedianMicros(7, 1, [&] { checksum += run_threads(true); });
  const double direct_parallel_us = MedianMicros(7, 1, [&] { checksum += run_threads(false); });
  std::cout << "threads=8 rows_per_thread=8 shared_prepared_locked_us=" << shared_locked_us
            << " direct_local_parallel_us=" << direct_parallel_us << '\n';
  std::cout << "checksum=" << checksum << '\n';
  return 0;
}
