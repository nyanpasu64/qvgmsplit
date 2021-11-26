#pragma once

#include <QString>
#include <QStringLiteral>

#include <cstdint>
#include <cstdlib>  // div

class QKeyEvent;

namespace format {
namespace detail {
    extern const QString hex_digits[16];
}

/// Converts a nybble into a single hex character.
[[nodiscard]] inline QString format_hex_1(size_t num) {
    return detail::hex_digits[num & 0x0F];
}

/// Converts a byte into 2 hex characters.
[[nodiscard]] inline QString format_hex_2(size_t wnum) {
    auto num = (uint8_t) wnum;
    return detail::hex_digits[num >> 4] + detail::hex_digits[num & 0x0F];
}

}
