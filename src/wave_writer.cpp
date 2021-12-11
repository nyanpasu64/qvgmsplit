/* Game_Music_Emu 0.6-pre. http://www.slack.net/~ant/ */

#define wave_writer_INTERNAL public
#include "wave_writer.h"
#include "lib/defer.h"

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

[[nodiscard]] static QString write_data(QFile & file, void const* in, int64_t size)
{
    if (file.write((char const*) in, size) == -1) {
        return file.errorString();
    }
    return {};
}

static void set_le32( unsigned char p [4], unsigned n )
{
    p [0] = (unsigned char) (n      );
    p [1] = (unsigned char) (n >>  8);
    p [2] = (unsigned char) (n >> 16);
    p [3] = (unsigned char) (n >> 24);
}

[[nodiscard]] static QString write_header(Wave_Writer & self)
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

    return write_data(self._file, h, sizeof h);
}

Wave_Writer::Wave_Writer(uint32_t sample_rate, QString const& path)
    : _file(path)
    , _sample_count(0)
    , _sample_rate(sample_rate)
    , _chan_count(1)
{}

Result<std::unique_ptr<Wave_Writer>, QString> Wave_Writer::make(
    uint32_t sample_rate, QString const& path
) {
    auto out = std::make_unique<Wave_Writer>(sample_rate, path);
    if (!out->_file.open(QFile::WriteOnly)) {
        return Err(out->_file.errorString());
    }
    // Write a header with dummy information. The real length/channel fields
    // will be written when close() calls write_header() again.
    if (auto err = write_header(*out); !err.isEmpty()) {
        return Err(move(err));
    }
    return Ok(move(out));
}

QString Wave_Writer::close()
{
    // May be called multiple times. Must be idempotent.
    if (_file.isOpen()) {
        _file.seek(0);
        if (auto err = write_header(*this); !err.isEmpty()) {
            _file.close();
            return err;
        }

        if (!_file.flush()) {
            return _file.errorString();
        }
        _file.close();
        return {};
    }
    return {};
}

void Wave_Writer::enable_stereo()
{
    _chan_count = 2;
}

[[nodiscard]] QString Wave_Writer::write(Amplitude const* in, uint32_t nsamp)
{
    _sample_count += nsamp;

    // This only works properly on little-endian CPUs, but is faster than chunking the
    // input to convert to little endian.
    if (
        auto err = write_data(_file, in, (int64_t) nsamp * BYTES_PER_SAMPLE);
        !err.isEmpty()
    ) {
        return err;
    }
    return {};
}

uint32_t Wave_Writer::sample_count() const
{
    return _sample_count;
}
