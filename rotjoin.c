/* 
    rotjoin.c
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
#include <strings.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>

#define PROG_NAME    "rotjoin"
#define PROG_VERSION "0.3"

#include "rotjoin.h"

const char *format_suffix[] = { NULL, ".wav", ".flac" };

struct rj_timestamp
{
    struct tm timespec;
    double frac;
};

static int parse_timestamp(char *, struct rj_timestamp *);
static time_t start_of_minute(struct rj_timestamp *);
static char *minute_filename(time_t, const char *prefix, const char *suffix, char *buff);

int main(int argc, char **argv)
{
    char file_prefix[1024];
    char filename[2048];
    char begin_str[32], end_str[32];
    struct rj_timestamp begin, end;
    time_t begin_min, end_min;
    int format;
    double duration = 0;
    int opt;

    /* Parse command-line options */
    file_prefix[0] = '\0';
    begin_str[0] = '\0';
    end_str[0] = '\0';
    while ( (opt = getopt(argc, argv, "p:b:e:o:f:h")) != -1 )
    {
        switch (opt)
        {
            case 'p':
                strncpy(file_prefix, optarg, sizeof(file_prefix)-1);
                break;
            case 'b':
                strncpy(begin_str, optarg, sizeof(begin_str)-1);
                break;
            case 'e':
                strncpy(end_str, optarg, sizeof(end_str)-1);
                break;
            case 'o':
                filepath = malloc(strlen(optarg) + 1);
                strcpy(filepath, optarg);
                break;
            case 'f':
                if(strcasecmp(optarg, "flac") == 0)
                    outformat = RJ_FORMAT_FLAC;
                else if(strcasecmp(optarg, "wav") == 0)
                    outformat = RJ_FORMAT_WAV;
                else
                {
                    fprintf(stderr, "Unrecognised format option %s\n", optarg);
                    return 1;
                }
                break;
            case 'h':
            default:
                fprintf(stderr, "\nUsage: %s [-p <prefix>] -b <timestamp> -e <timestamp> [-o <filename>] [-f flac|wav] [-h]\n", PROG_NAME);
                fprintf(stderr, "\nCreates output audio file containing all audio between specified timestamps.\n");
                fprintf(stderr, "Source audio should be contained in minute-long files as <prefix>YYYY-MM-DD/HHMM.[flac|wav].\n");
                fprintf(stderr, "Output file will have same sample rate and no. of channels as first input file opened.\n\n");
                fprintf(stderr, "   -p <prefix>    Prefix where the minute-long ROT files are located (may contain / characters)\n");
                fprintf(stderr, "   -b <timestamp> Begin timestamp in YYYYMMDDHHMMSS[.xx] format\n");
                fprintf(stderr, "   -e <timestamp> End timestamp in YYYYMMDDHHMMSS[.xx] format\n");
                fprintf(stderr, "   -o <filename>  Output filename (stdout if not specified)\n");
                fprintf(stderr, "   -f flac|wav    Output format (FLAC or WAV). If not specified, format determined from output\n");
                fprintf(stderr, "                  filename if possible, otherwise from first input file opened.\n");
                fprintf(stderr, "   -h             Display this usage message and exit\n");
                fprintf(stderr, "\n");
                return 0;
        }
    }

    fprintf(stderr, "%s v%s starting...\n", PROG_NAME, PROG_VERSION);

    /* Check all mandatory options have been specified */
    if(begin_str[0] == '\0' || end_str[0] == '\0')
    {
        fprintf(stderr, "ERROR: Begin and end timestamps must be specified\n");
        return 1;
    }

    /* Validate and parse timestamps */
    if( !parse_timestamp(begin_str, &begin))
    {
        fprintf(stderr, "ERROR: Unable to parse begin timestamp %s\n", begin_str);
        return 1;
    }
    if( !parse_timestamp(end_str, &end))
    {
        fprintf(stderr, "ERROR: Unable to parse end timestamp %s\n", end_str);
        return 1;
    }

    /* Try looking for begin file to see what format we should be using */
    begin_min = start_of_minute(&begin);
    format = RJ_FORMAT_FLAC;
    while(format > RJ_FORMAT_NONE)
    {
        struct stat st;

        if(stat(minute_filename(begin_min, file_prefix, format_suffix[format], filename), &st) == 0)
            break;
        format--;
    }
    if(format == RJ_FORMAT_NONE)
    {
        fprintf(stderr, "ERROR: Unable to locate beginning file matching format \"%s\" or similar\n", filename);
        return 1;
    }

    /* Default to standard output if output file not specified */
    if(!filepath)
    {
        filepath = malloc(2);
        strcpy(filepath, "-");
    }
    else if(outformat == RJ_FORMAT_NONE)
    {
        /* Try to detect format from output filename */
        if(strstr(filepath, ".flac") || strstr(filepath, ".FLAC"))
            outformat = RJ_FORMAT_FLAC;
        else if(strstr(filepath, ".wav") || strstr(filepath, ".WAV"))
            outformat = RJ_FORMAT_WAV;
    }

    /* Force output format to be the same as input format if we
     * haven't been able to detect it. */
    if(outformat == RJ_FORMAT_NONE)
        outformat = format;

    fputs("Output file format will be ", stderr);
    switch(outformat)
    {
        case RJ_FORMAT_FLAC:
            fputs("FLAC", stderr);
            break;
        case RJ_FORMAT_WAV:
            fputs("WAV", stderr);
            break;
        default:
            fputs("unknown", stderr);
    }
    fputc('\n', stderr);

    /* Start appending files to output */
    if(begin_min == (end_min = start_of_minute(&end)))
        /* Begin and end minutes are the same */
        duration += append_file(filename, begin.timespec.tm_sec + begin.frac, end.timespec.tm_sec + end.frac);
    else
    {
        /* Output spans more than one input file */
        time_t curr_min;

        /* Start by appending the latter part of the first file */
        duration += append_file(filename, 
                                begin.timespec.tm_sec + begin.frac, 999);

        /* Now append all of any intermediate files */
        for(curr_min = begin_min + 60; curr_min < end_min; curr_min += 60)
            duration += append_file(minute_filename(curr_min, file_prefix, format_suffix[format], filename), 
                                    0, 999);

        /* And finish off with the first part of the last file */
        duration += append_file(minute_filename(end_min, file_prefix, format_suffix[format], filename), 
                                0, end.timespec.tm_sec + end.frac);
    }

    /* Close output and finish up */
    close_output();
    fprintf(stderr, "Total output duration: %.4fs\n", duration);

    return 0;
}

static int parse_timestamp(char *str, struct rj_timestamp *ts)
{
    struct tm *tspec = &ts->timespec;
    time_t now;
    int year, month;
    char *ptr;

    if(strlen(str) < 14) /* Must be at least strlen("YYYYMMDDHHMMSS") */
        return 0;

    ptr = strrchr(str, '.');
    if(ptr)
    {
        if(ptr - str < 14) /* 14 chars should be to left of decimal point */
            return 0;
        ts->frac = strtod(ptr, NULL);
        *ptr = '\0';
    }

    /* Initialise timespec with correct timezone info etc. */
    now = time(NULL);
    localtime_r(&now, tspec);
    if(sscanf(str, "%4d%2d%2d%2d%2d%2d", &year, &month, &tspec->tm_mday,
              &tspec->tm_hour, &tspec->tm_min, &tspec->tm_sec) != 6)
        return 0;

    tspec->tm_year = year - 1900;
    tspec->tm_mon = month - 1;

    return 1;
}

/* Determine the start of the minute containing the given rotjoin timestamp,
 * and return it as a Unix time_t. */
static time_t start_of_minute(struct rj_timestamp *ts)
{
    struct tm timespec = ts->timespec;

    timespec.tm_sec = 0;
    return mktime(&timespec);
}

static char *minute_filename(time_t curr_min, const char *prefix, const char *suffix, char *buff)
{
    struct tm curr_tm;

    localtime_r(&curr_min, &curr_tm);
    sprintf(buff, "%s%4d-%02d-%02d/%02d%02d%s", prefix, 1900+curr_tm.tm_year, 1+curr_tm.tm_mon,
            curr_tm.tm_mday, curr_tm.tm_hour, curr_tm.tm_min, suffix);

    return buff;
}
