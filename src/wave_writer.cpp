/* Game_Music_Emu 0.6-pre. http://www.slack.net/~ant/ */

#define wave_writer_INTERNAL public
#include "wave_writer.h"

#include <stdlib.h>
#include <stdio.h>

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

enum { sample_size = 2 };

static void write_header(Wave_Writer & self);

static void fatal_error( char const str [] )
{
    printf( "Error: %s\n", str );
    exit( EXIT_FAILURE );
}

Wave_Writer::Wave_Writer( int new_sample_rate, char const filename [] )
{
    _file = fopen( filename, "wb" );
    if ( !_file )
        fatal_error( "Couldn't open WAVE file for writing" );

    _sample_rate = new_sample_rate;
    write_header(*this);
}

void Wave_Writer::close()
{
    // May be called multiple times. Must be idempotent.
    if ( _file )
    {
        rewind( _file );
        write_header(*this);

        fclose( _file );
        _file = NULL;
    }
}

static void write_data(Wave_Writer & self, void const* in, unsigned size)
{
    if ( !fwrite( in, size, 1, self._file ) )
        fatal_error( "Couldn't write WAVE data" );
}

static void set_le32( unsigned char p [4], unsigned n )
{
    p [0] = (unsigned char) (n      );
    p [1] = (unsigned char) (n >>  8);
    p [2] = (unsigned char) (n >> 16);
    p [3] = (unsigned char) (n >> 24);
}

static void write_header(Wave_Writer & self)
{
    int data_size  = sample_size * self._sample_count;
    int frame_size = sample_size * self._chan_count;
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
        sample_size*8,0,/* bits per sample */
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

    write_data(self, h, sizeof h);
}

void Wave_Writer::enable_stereo()
{
    _chan_count = 2;
}

void Wave_Writer::write( short const in [], int remain )
{
    _sample_count += remain;

    while ( remain )
    {
        unsigned char buf [4096];

        int n = sizeof buf / sample_size;
        if ( n > remain )
            n = remain;
        remain -= n;

        /* Convert to little-endian */
        {
            unsigned char* out = buf;
            short const* end = in + n;
            do
            {
                unsigned s = *in++;
                out [0] = (unsigned char) (s     );
                out [1] = (unsigned char) (s >> 8);
                out += sample_size;
            }
            while ( in != end );

            write_data(*this, buf, out - buf);
        }
    }
}

int Wave_Writer::sample_count() const
{
    return _sample_count;
}
