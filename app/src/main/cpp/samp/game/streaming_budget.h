#pragma once
#include <cstddef>

// Saturate instead of wrapping size_t when a model exceeds the streaming budget.
constexpr std::size_t StreamingRemainingBudget(std::size_t available, std::size_t requested) {
    return requested >= available ? 0 : available - requested;
}
