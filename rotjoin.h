#ifndef ROTJOIN_H
#define ROTJOIN_H

#define RJ_FORMAT_NONE 0
#define RJ_FORMAT_WAV  1
#define RJ_FORMAT_FLAC 2

/* defined in splice.c */
extern char *filepath;
extern int outformat;

/* splice.c */
double append_file(const char *filename, double mark_in, double mark_out);
void close_output(void);

#endif /* ROTJOIN_H */
