/* Game_Music_Emu 0.6-pre. http://www.slack.net/~ant/ */

#define wave_writer_INTERNAL public
#include "wave_writer.h"
#include "lib/defer.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <utility>

/* Copyright (C) 2003-2008 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

static constexpr uint32_t BYTES_PER_SAMPLE = sizeof(Wave_Writer::Amplitude);

using stx::Ok, stx::Err;
using std::move;

[[nodiscard]] static Errno write_data(Wave_Writer & self, void const* in, size_t size)
{
    if (fwrite(in, size, 1, self._file) != size) {
        return -1;
    }
    return 0;
}

static void set_le32( unsigned char p [4], unsigned n )
{
    p [0] = (unsigned char) (n      );
    p [1] = (unsigned char) (n >>  8);
    p [2] = (unsigned char) (n >> 16);
    p [3] = (unsigned char) (n >> 24);
}

[[nodiscard]] static Errno write_header(Wave_Writer & self)
{
    uint32_t data_size  = BYTES_PER_SAMPLE * self._sample_count;
    uint8_t frame_size = BYTES_PER_SAMPLE * self._chan_count;
    unsigned char h [0x2C] =
    {
        'R','I','F','F',
        0,0,0,0,        /* length of rest of file */
        'W','A','V','E',
        'f','m','t',' ',
        16,0,0,0,       /* size of fmt chunk */
        1,0,            /* uncompressed format */
        0,0,            /* channel count */
        0,0,0,0,        /* sample rate */
        0,0,0,0,        /* bytes per second */
        0,0,            /* bytes per sample frame */
        BYTES_PER_SAMPLE * 8,0,/* bits per sample */
        'd','a','t','a',
        0,0,0,0         /* size of sample data */
        /* ... */       /* sample data */
    };

    set_le32( h + 0x04, sizeof h - 8 + data_size );
    h [0x16] = self._chan_count;
    set_le32( h + 0x18, self._sample_rate );
    set_le32( h + 0x1C, self._sample_rate * frame_size );
    h [0x20] = frame_size;
    set_le32( h + 0x28, data_size );

    return write_data(self, h, sizeof h);
}

Wave_Writer::Wave_Writer(uint32_t sample_rate, FILE *file)
    : _file(file)
    , _sample_count(0)
    , _sample_rate(sample_rate)
    , _chan_count(1)
{}

static Result<std::unique_ptr<Wave_Writer>, Errno> make(
    uint32_t sample_rate, FILE * file
) {
    if (!file) {
        return Err(+errno);
    }

    auto out = std::make_unique<Wave_Writer>(sample_rate, file);
    // Write a header with dummy information. The real length/channel fields
    // will be written when close() calls write_header() again.
    if (auto err = write_header(*out)) {
        return Err(+err);
    }
    return Ok(move(out));
}

Result<std::unique_ptr<Wave_Writer>, Errno> Wave_Writer::try_make(
    uint32_t sample_rate, const char *filename
) {
    return make(sample_rate, fopen(filename, "wb"));
}

#ifdef _WIN32
Result<std::unique_ptr<Wave_Writer>, Errno> Wave_Writer::try_make_w(
    uint32_t sample_rate, const wchar_t *filename
) {
    return make(sample_rate, _wfopen(filename, L"wb"));
}
#endif

Errno Wave_Writer::close()
{
    // May be called multiple times. Must be idempotent.
    if ( _file )
    {
        rewind( _file );
        if (auto err = write_header(*this)) {
            fclose( _file );
            _file = NULL;
            return err;
        }

        auto err = fclose( _file );
        _file = NULL;
        return err;
    }
    return 0;
}

void Wave_Writer::enable_stereo()
{
    _chan_count = 2;
}

[[nodiscard]] Errno Wave_Writer::write(Amplitude const* in, uint32_t remain)
{
    _sample_count += remain;

    while ( remain )
    {
        static constexpr uint32_t BUF_SIZE = 4096;
        unsigned char buf [BUF_SIZE];

        auto const nsamp = std::min(BUF_SIZE / BYTES_PER_SAMPLE, remain);
        remain -= nsamp;

        /* Convert to little-endian */
        {
            unsigned char* out = buf;
            Amplitude const* end = in + nsamp;
            do
            {
                auto s = (uint32_t) *in++;
                out [0] = (unsigned char) (s     );
                out [1] = (unsigned char) (s >> 8);
                out += 2;
                static_assert(BYTES_PER_SAMPLE == 2);
            }
            while ( in != end );

            if (auto err = write_data(*this, buf, (size_t) (out - buf))) {
                return err;
            }
        }
    }

    return 0;
}

uint32_t Wave_Writer::sample_count() const
{
    return _sample_count;
}
