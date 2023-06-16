#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <sndfile.h>

int main(int argc, char **argv)
{
	const int width = 1024;
	const int height = 1024;
	int retval = 1;
	if (argc != 3)
	{
		fprintf(stderr, "usage: %s in6ch.wav out.ppm\n", argv[0]);
	}
	else
	{
		SF_INFO info;
		SNDFILE *in = sf_open(argv[1], SFM_READ, &info);
		if (! in)
		{
			fprintf(stderr, "error: could not open %s for reading\n", argv[1]);
		}
		else
		{
			if (info.channels != 6)
			{
				fprintf(stderr, "error: expected 6 channels, got %d\n", info.channels);
			}
			else
			{
				float *audio = malloc(sizeof(*audio) * info.channels * info.frames);
				if (! audio)
				{
					fprintf(stderr, "error: could not allocate audio\n");
				}
				else
				{
					int64_t frames;
					if (info.frames != (frames = sf_readf_float(in, audio, info.frames)))
					{
						fprintf(stderr, "warning: expected %" PRId64 " frames but read %" PRId64 "\n", (int64_t) info.frames, frames);
					}
					int64_t *histogram = calloc(1, sizeof(*histogram) * width * height);
					if (! histogram)
					{
						fprintf(stderr, "error: could not allocate histogram\n");
					}
					else
					{
						unsigned char *image = malloc(sizeof(*image) * width * height);
						if (! image)
						{
							fprintf(stderr, "error: could not allocate image\n");
						}
						else
						{
							for (int64_t frame = 0; frame < frames; ++frame)
							{
								float magnitude = audio[info.channels * frame + 4];
								float phase = audio[info.channels * frame + 5];
								int x = fminf(fmaxf(phase * (width - 1) + rand() / (double) RAND_MAX, 0), width - 1);
								int y = fminf(fmaxf((1 - magnitude) * (height - 1) + rand() / (double) RAND_MAX, 0), height - 1);
								histogram[y * width + x] += 1;
							}
							int64_t peak = 0;
							for (int y = 0; y < height; ++y)
							{
								for (int x = 0; x < width; ++x)
								{
									int64_t h = histogram[y * width + x];
									peak = h > peak ? h : peak;
								}
							}
							float scale = 255.0f / logf(1 + peak);
							for (int y = 0; y < height; ++y)
							{
								for (int x = 0; x < width; ++x)
								{
									int64_t h = histogram[y * width + x];
									int c = fmin(fmax(scale * logf(1  + h) + rand() / (double) RAND_MAX, 0), 255);
									image[y * width + x] = c;
								}
							}
							FILE *out = fopen(argv[2], "wb");
							if (! out)
							{
								fprintf(stderr, "error: could not open %s for writing\n", argv[2]);
							}
							else
							{
								fprintf(out, "P5\n%d %d\n255\n", width, height);
								if (1 != fwrite(image, sizeof(*image) * width * height, 1, out))
								{
									fprintf(stderr, "error: write failed\n");
								}
								else
								{
									// success
									retval = 0;
								}
								fclose(out);
							}
							free(image);
						}
						free(histogram);
					}
					free(audio);
				}
			}
			sf_close(in);
		}
	}
	return retval;
}
