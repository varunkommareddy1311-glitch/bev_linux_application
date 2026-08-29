#include "bev/HealthMonitor.hpp"

// HealthMonitor is currently fully header-only (all methods are small,
// noexcept, and atomic-based). This translation unit exists so the class
// has a stable place to grow into (e.g. periodic snapshot serialization
// for a future status/metrics endpoint) without every consumer needing to
// recompile when that logic is added.

namespace bev {
} // namespace bev
