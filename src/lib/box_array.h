#pragma once

#include "copy_move.h"

#include <gsl/span>
#include <gsl/span_ext>

#include <array>
#include <algorithm>
#include <memory>

/// A fixed-size default/zero-initialized array located behind an owning pointer,
/// freely movable and explicitly copyable.
///
/// TODO add support for non-default types.
template<typename T, size_t N>
class BoxArray {
    using Arr = std::array<T, N>;
    std::unique_ptr<Arr> _arr;

public:
    using Span = gsl::span<T const, N>;
    using SpanMut = gsl::span<T, N>;
    using DynSpan = gsl::span<T const>;
    using DynSpanMut = gsl::span<T>;

    BoxArray()
        // Default-initializes _arr. If it holds integers, elements are initialized to
        // 0.
        : _arr(std::make_unique<Arr>())
    {}

    BoxArray(Span span)
        // TODO use std::make_unique_for_overwrite() when supported by GCC/Clang.
        : BoxArray()
    {
        std::copy(span.begin(), span.end(), _arr->begin());
    }

    explicit BoxArray(BoxArray const& other)
        : BoxArray(Span(other))
    {}

    // Moving a BoxArray leaves the previous one pointing to nullptr,
    // which is illegal to dereference.
    DEFAULT_MOVE(BoxArray)

    T const* data() const {
        return _arr->data();
    }
    T * data() {
        return _arr->data();
    }

    Span span() const {
        // Passing cbegin() doesn't compile on MSVC's stdlib.
        return Span(data(), N);
    }
    SpanMut span_mut() {
        return Span(data(), N);
    }
    operator Span() const {
        return span();
    }
    operator SpanMut() {
        return span_mut();
    }

    DynSpan dyn_span() const {
        return span();
    }
    DynSpanMut dyn_span_mut() {
        return span_mut();
    }

#ifdef UNITTEST
    bool operator==(BoxArray const& other) const {
        return span() == other.span();
    }
    bool operator==(Span const& other) const {
        return span() == other;
    }
#endif

    constexpr size_t size() const noexcept(true) {
        return N;
    }

    T const& operator[](size_t idx) const {
        return data()[idx];
    }
    T & operator[](size_t idx) {
        return data()[idx];
    }

    T const* begin() const {
        return data();
    }
    T * begin() {
        return data();
    }

    T const* end() const {
        return data() + N;
    }
    T * end() {
        return data() + N;
    }
};
