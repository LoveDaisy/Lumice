#ifndef UTIL_CPU_INFO_H_
#define UTIL_CPU_INFO_H_

namespace lumice {

// Returns the number of physical CPU cores on the host machine.
// On SMT systems this is typically half of std::thread::hardware_concurrency().
// On detection failure, falls back to max(1, hardware_concurrency() / 2).
// Always returns a value >= 1.
int PhysicalCoreCount();

// Returns the number of logical CPUs (hardware threads) on the host machine — on an SMT
// system, typically twice PhysicalCoreCount(). A thin wrapper over
// std::thread::hardware_concurrency() with the same >= 1 guarantee as the function above
// (a 0 from the standard library reads as 1), so a caller sizing a worker pool never has
// to carry that fallback itself. The two functions are kept side by side, and a caller
// picks one deliberately: which of them the automatic worker count should be sized from
// is a per-platform measurement (see kMaxDefaultWorkerCount in server.cpp), not a
// property of this file.
int LogicalCoreCount();

}  // namespace lumice

#endif  // UTIL_CPU_INFO_H_
