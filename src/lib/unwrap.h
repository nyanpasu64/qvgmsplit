#pragma once

#include "lib/release_assert.h"

#include <optional>
#include <utility>

template<typename T>
T unwrap(std::optional<T> v) {
    release_assert(v.has_value());
    return std::move(*v);
}

template<typename T, typename Fn>
void debug_unwrap(std::optional<T> v, Fn fn) {
    assert(v.has_value());
    if (v) {
        fn(*v);
    }
}
