#ifndef PS5_STB_VORBIS_H
#define PS5_STB_VORBIS_H
int stb_vorbis_decode_filename(const char *filename, int *channels, int *sample_rate, short **output);
#endif
