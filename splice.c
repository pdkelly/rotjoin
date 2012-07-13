/* 
    splice.c
    Audio file joining utility for Record of Transmission (ROT) systems
    by Paul Kelly.
    Copyright (C) 2012 NP Broadcast Ltd.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sndfile.h>

#include "rotjoin.h"

char *filepath;
int outformat = RJ_FORMAT_NONE;
static SNDFILE *outfile;
static SF_INFO outfile_info;

static void open_output(struct SF_INFO *infile_info);
static void write_output(short *buffer, int frames);

double append_file(const char *filename, double mark_in, double mark_out)
{
    SNDFILE *infile;
    SF_INFO file_info;
    double file_duration;
    int offset_in, offset_out;
    static int buff_size;
    static short *buff;
    int frames_to_read, frames_read;
    double duration;

    fprintf(stderr, "Opening \"%s\"\n", filename);
    file_info.format = 0;

    if( !(infile = sf_open(filename, SFM_READ, &file_info)))
    {
        fprintf(stderr, "ERROR while opening input file %s for reading: %s\n",
                filename, sf_strerror(NULL));
        return 0.;
    }

    file_duration = (double)file_info.frames / file_info.samplerate;

    if(mark_in > file_duration)
        return 0.;
    else
        offset_in = (int)(0.5 + mark_in * file_info.samplerate);

    if(mark_out > file_duration)
        offset_out = file_info.frames;
    else
        offset_out = (int)(0.5 + mark_out * file_info.samplerate);

    if(!outfile)
        open_output(&file_info);

    /* Seek forward in the file if necessary */
    if(offset_in > 0 && sf_seek(infile, offset_in, SEEK_SET) != offset_in)
    {
        fprintf(stderr, "ERROR seeking %d frames into input file %s: %s\n",
		offset_in, filename, sf_strerror(infile));
        return 0.;
    }

    if(!buff)
    {
        /* allocate buffer for 5 seconds */
        buff_size = file_info.samplerate * 5;
        buff = malloc(file_info.channels * buff_size * sizeof(short));
    }

    frames_to_read = offset_out - offset_in;
    frames_read = 0;
    while(frames_read < frames_to_read)
    {	
        int frames_req = frames_to_read - frames_read, got;

        if(frames_req > buff_size)
            frames_req = buff_size;

        got = sf_readf_short(infile, buff, frames_req);
        write_output(buff, got);
        frames_read += got;
        if(got != frames_req)
	{
	    fprintf(stderr, "WARNING: expected to read %d frames from input file %s but only got %d\n",
		    frames_req, filename, got);
	    break;
	}
    }

    if(sf_close(infile) != 0)
        fprintf(stderr, "ERROR while closing input file %s: %s\n",
                filename, sf_strerror(infile));

    duration = (double)frames_read / outfile_info.samplerate;
    fprintf(stderr, "File \"%s\": %.4fs output\n", filename, duration);

    return duration;
}

/*
 * open_output()
 * 
 * Opens the file with pathname specified by global variable "filepath" and stores
 * the pointer to the resulting SNDFILE stream in global variable "outfile".
 * 
 * Prints an appropriate message to stderr and exits the program should an
 * error occur during opening the output.
 */
static void open_output(struct SF_INFO *infile_info)
{
    fprintf(stderr, "Opening \"%s\" for output\n", filepath);

    /* Use same file format for output as for first input file */
    switch(outformat)
    {
        case RJ_FORMAT_FLAC:
            outfile_info.format = SF_FORMAT_FLAC|SF_FORMAT_PCM_16;
            break;
        case RJ_FORMAT_WAV:
            outfile_info.format = SF_FORMAT_WAV|SF_FORMAT_PCM_16;
            break;
        default:
            fprintf(stderr, "ERROR: File format unspecified; unable to open output\n");
            exit(1);
    }
    outfile_info.samplerate = infile_info->samplerate;
    outfile_info.channels = infile_info->channels;

    if( strcmp(filepath, "-") == 0 )
        outfile = sf_open_fd(fileno(stdout), SFM_WRITE, &outfile_info, 0);
    else
        outfile = sf_open(filepath, SFM_WRITE, &outfile_info);

    if( !outfile)
    {
        fprintf(stderr, "ERROR while opening output file %s for writing: %s\n",
                filepath, sf_strerror(NULL));
        exit(1);
    }
    return;
}

/*
 * write_output()
 * 
 * Writes "frames" audio frames starting from the memory buffer at "buffer" 
 * to the output SNDFILE stream specified by global variable "outfile".
 * 
 * Prints an appropriate message to stderr and exits the program should an
 * error or short count occur during writing.
 */
static void write_output(short *buffer, int frames)
{
    if( sf_writef_short(outfile, buffer, frames) != frames )
    {
        fprintf(stderr, "ERROR while writing to output file %s: %s\n",
                filepath, sf_strerror(outfile));
        exit(1);
    }
    return;
}

/*
 * close_output()
 * 
 * Closes the output stream specified by global variable "outfile".
 * 
 * Prints an appropriate message to stderr and exits the program should an
 * error occur during closing.
 */
void close_output(void)
{
    if(strcmp(filepath, "-") != 0 && sf_close(outfile) != 0 )
    {
        fprintf(stderr, "ERROR while closing output file %s: %s\n",
                filepath, sf_strerror(outfile));
        exit(1);
    }
    return;
}
