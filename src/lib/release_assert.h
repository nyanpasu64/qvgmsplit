#pragma once

#include <fmt/core.h>

#include <stdexcept>
#include <cassert>  // assert() is not used in this header, but supplied to includers.


#define release_assert(expr) \
    do { if (!(expr)) { \
        throw std::logic_error("release_assert failed: `" #expr "` is false"); \
    } } while (0)

#define release_assert_equal(lhs, rhs) \
    do { if ((lhs) != (rhs)) { \
        std::string message = fmt::format( \
            "release_assert failed: `{}`={} != `{}`={}", #lhs, lhs, #rhs, rhs \
        ); \
        throw std::logic_error(message); \
    } } while (0)
