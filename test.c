#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eaccodec.h"

static uint8_t *
load_pgm(const char *filename, int *w, int *h) {
	FILE *f = fopen(filename,"rb");
	if (f == NULL)
		return NULL;
	int width, height;
	int n = fscanf(f, "P2\n%d %d\n,255\n", &width, &height);
	if (n != 2)
		return NULL;
	uint8_t * image = malloc(width * height);
	int i;
	for (i=0;i<width * height;i++) {
		int alpha;
		if (fscanf(f, "%d", &alpha) != 1) {
			free(image);
			fclose(f);
			return NULL;
		}
		image[i] = alpha;
	}
	*w = width;
	*h = height;
	return image;
}

static void
save_pgm(const char *filename, const uint8_t * image, int w, int h) {
	FILE *f = fopen(filename,"wb");
	if (f == NULL)
		return;
	fprintf(f, "P2\n%d %d\n255\n", w, h);
	int i;
	for (i=0;i<w*h;i++) {
		fprintf(f,"%d ", image[i]);
		if (i % 16 == 15)
			fprintf(f,"\n");
	}
	fclose(f);
}

static void
read_chunk(const uint8_t *image, int x, int y, int pitch, uint8_t chunk[16]) {
	int i;
	image += y * pitch + x;
	for (i=0;i<4;i++) {
		memcpy(chunk+i*4, image+pitch*i, 4);
	}
}

static void
write_chunk(uint8_t *image, int x, int y, int pitch, const uint8_t chunk[16]) {
	int i;
	image += y * pitch + x;
	for (i=0;i<4;i++) {
		memcpy(image+pitch*i, chunk+i*4, 4);
	}
}

int
main() {
	int w,h;
	uint8_t * image = load_pgm("baby.pgm", &w, &h);
	if (image == NULL)
		return 1;
	int x,y;
	uint8_t chunk[16], encode[8];
	for (y = 0; y < h ; y+=4) {
		for (x = 0; x < w; x+=4) {
			read_chunk(image, x,y, w, chunk);
			eac_encode(chunk, encode);
			eac_decode(encode, chunk);
			write_chunk(image, x,y, w, chunk);
		}
	}
	save_pgm("eac_baby.pgm", image, w, h);
	free(image);

	return 0;
}
