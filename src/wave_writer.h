/* Simple wave sound file writer for use in demo programs. */

/* Game_Music_Emu 0.6-pre */
#ifndef WAVE_WRITER_H
#define WAVE_WRITER_H

#include <cstdio>

#ifndef wave_writer_INTERNAL
#define wave_writer_INTERNAL private
#endif

/* ERRORS: If error occurs (out of memory, disk full, etc.), functions print
cause then exit program. */

/* C++ interface */
class Wave_Writer {
wave_writer_INTERNAL:
    FILE* _file;
    int   _sample_count;
    int   _sample_rate;
    int   _chan_count;

public:
    typedef short sample_t;

    /// Creates and opens sound file of given sample rate and filename.
    Wave_Writer( int new_sample_rate, const char filename [] = "out.wav" );

    /// Enables stereo output.
    void enable_stereo();

    /// Appends count samples to file.
    void write( const sample_t in [], int n );

    /// Number of samples written so far.
    int sample_count() const;

    /// Finishes writing sound file and closes it.
    /// May be called multiple times. Must be idempotent.
    void close();

    ~Wave_Writer() {
        close();
    }
};
#endif
