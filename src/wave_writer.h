/* Simple wave sound file writer for use in demo programs. */

/* Game_Music_Emu 0.6-pre */
#ifndef WAVE_WRITER_H
#define WAVE_WRITER_H

#include "lib/copy_move.h"

#include <stx/result.h>

#include <cstdio>
#include <cstdint>
#include <memory>

#ifndef wave_writer_INTERNAL
#define wave_writer_INTERNAL private
#endif

/// Zero for success, positive for errno, -1 for application-specific errors.
using Errno = int;
using stx::Result;

/* C++ interface */
class Wave_Writer {
wave_writer_INTERNAL:
    FILE* _file;
    uint32_t   _sample_count;
    uint32_t   _sample_rate;
    uint8_t   _chan_count;

public:
    using Amplitude = int16_t;

wave_writer_INTERNAL:
    Wave_Writer(uint32_t sample_rate, FILE * file);
    DISABLE_COPY_MOVE(Wave_Writer)

public:
    /// Creates and opens sound file of given sample rate and filename.
    /// If opening file or writing header fails, returns Err.
    /// Supports 8-bit paths natively on Linux.
    static Result<std::unique_ptr<Wave_Writer>, Errno> try_make(
        uint32_t sample_rate, char const* filename
    );
#ifdef _WIN32
    /// Creates and opens sound file of given sample rate and filename.
    /// If opening file or writing header fails, returns Err.
    /// Supports Unicode paths natively on Windows.
    static Result<std::unique_ptr<Wave_Writer>, Errno> try_make_w(
        uint32_t sample_rate, wchar_t const* filename
    );
#endif

    /// Enables stereo output.
    void enable_stereo();

    /// Appends count samples to file.
    [[nodiscard]] Errno write(Amplitude const* in, uint32_t n);

    /// Number of samples written so far.
    uint32_t sample_count() const;

    /// Finishes writing sound file and closes it. May be called multiple times,
    /// and does nothing on subsequent calls (making it idempotent).
    [[nodiscard]] Errno close();

    ~Wave_Writer() {
        (void) close();
    }
};
#endif
