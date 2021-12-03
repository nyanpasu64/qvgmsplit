/* Simple wave sound file writer for use in demo programs. */

/* Game_Music_Emu 0.6-pre */
#ifndef WAVE_WRITER_H
#define WAVE_WRITER_H

#include "lib/copy_move.h"

#include <stx/result.h>

#include <QFile>

#include <cstdint>
#include <memory>

#ifndef wave_writer_INTERNAL
#define wave_writer_INTERNAL private
#endif

using stx::Result;

/* C++ interface */
class Wave_Writer {
wave_writer_INTERNAL:
    QFile _file;
    uint32_t   _sample_count;
    uint32_t   _sample_rate;
    uint8_t   _chan_count;

public:
    using Amplitude = int16_t;

wave_writer_INTERNAL:
    Wave_Writer(uint32_t sample_rate, QString const& path);
    DISABLE_COPY_MOVE(Wave_Writer)

public:
    /// Creates and opens sound file of given sample rate and filename.
    /// If opening file or writing header fails, returns Err.
    static Result<std::unique_ptr<Wave_Writer>, QString> make(
        uint32_t sample_rate, QString const& path
    );

    /// Enables stereo output.
    void enable_stereo();

    /// Appends nsamp samples to file.
    [[nodiscard]] QString write(Amplitude const* in, uint32_t nsamp);

    /// Number of samples written so far.
    uint32_t sample_count() const;

    /// Finishes writing sound file and closes it. May be called multiple times,
    /// and does nothing on subsequent calls (this function is idempotent).
    [[nodiscard]] QString close();

    ~Wave_Writer() {
        (void) close();
    }
};
#endif
