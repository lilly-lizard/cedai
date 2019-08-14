#include <stdio.h>
#include <png.h>

#define CL_TARGET_OPENCL_VERSION 120
#include "render.h"

enum RESULT {
	SUCCESS,
	FAILURE
};

// TODO init to 0
typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
} pixel;

typedef struct {
	size_t width;
	size_t height;
	pixel *pixels;
} bitmap;

bitmap render();
bitmap genGradient();
pixel* getPixel(const bitmap *bm, int x, int y);
enum RESULT writePNG(const bitmap *bm, const char *path);

int main() {
	// futhark program
	bitmap bm = render();
	//bitmap bm = genGradient();

	// write to png file
	char *filePath = "render.png";
	enum RESULT res = writePNG(&bm, filePath);
	if (res != SUCCESS) printf("Error writing to %s", filePath);

	// clean up
	free(bm.pixels);
	return res;
}

bitmap genGradient() {
	bitmap gradient;
	gradient.width = 200;
	gradient.height = 200;
	gradient.pixels = calloc(gradient.width * gradient.height, sizeof(pixel));

	// set pixels
	for (int x = 0; x < gradient.width; x++) {
		for (int y = 0; y < gradient.height; y++) {
			pixel *p = getPixel(&gradient, x, y);
			p->r = 255 * x / gradient.width;
			p->g = 255 * y / gradient.height;
			p->b = 255;
			p->a = 255;
		}
	}

	return gradient;
}

void convertFutharkPixels(bitmap *bm, uint8_t *pixels, size_t width, size_t height) {
	bm->width = width;
	bm->height = height;
	bm->pixels = calloc(width * height, sizeof(pixel));
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			pixel *p = bm->pixels + width * y + x;
			p->r = *pixels++;
			p->g = *pixels++;
			p->b = *pixels++;
			p->a = *pixels++;
		}
	}
}

bitmap render() {
	struct futhark_context_config *cfg = futhark_context_config_new();
	struct futhark_context *ctx = futhark_context_new(cfg);

	int32_t width = 200;
	int32_t height = 200;
	uint8_t *pixelsArray = calloc(width * height * 4, sizeof(uint8_t));
	struct futhark_u8_2d *pixelsFuthark = futhark_new_u8_2d(ctx, pixelsArray, width * height, 4);

	futhark_entry_main(ctx, &pixelsFuthark, width, height);
	futhark_context_sync(ctx);

	futhark_values_u8_2d(ctx, pixelsFuthark, pixelsArray);
	bitmap pixelsBitmap;
	convertFutharkPixels(&pixelsBitmap, pixelsArray, width, height);

	futhark_free_u8_2d(ctx, pixelsFuthark);
		free(pixelsArray);
	futhark_context_free(ctx);
	futhark_context_config_free(cfg);

	return pixelsBitmap;
}

pixel* getPixel(const bitmap *bm, int x, int y) {
	return bm->pixels + bm->width * y + x;
}

enum RESULT writePNG(const bitmap *bm, const char *path) {
	enum RESULT result = FAILURE;

	FILE *fp = fopen(path, "wb");
    if (!fp) goto fopen_failed;

	png_structp png = NULL;
	png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png == NULL) goto png_create_write_struct_failed;

	png_infop pngInfo = NULL;
	pngInfo = png_create_info_struct(png);
	if (pngInfo == NULL) goto png_failure;

	// set up error handling
	if (setjmp(png_jmpbuf(png))) goto png_failure;

    int depth = 8;
	png_set_IHDR(png, pngInfo,
                 bm->width, bm->height, depth,
                 PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

	// init rows of PNG
    int pixelSize = 4;
	png_byte **rows = png_malloc(png, bm->height * sizeof(png_byte *));
	for (size_t y = 0; y < bm->height; y++) {
		png_byte *row = png_malloc(png, sizeof(uint8_t) * bm->width * pixelSize);
		rows[y] = row;
		for (size_t x = 0; x < bm->width; x++) {
			pixel *pixel = getPixel(bm, x, y);
			*row++ = pixel->r;
			*row++ = pixel->g;
			*row++ = pixel->b;
			*row++ = pixel->a;
		}
	}

	// write image data to fp
	png_init_io(png, fp);
    png_set_rows(png, pngInfo, rows);
    png_write_png(png, pngInfo, PNG_TRANSFORM_IDENTITY, NULL);

	result = SUCCESS;

	for (int y = 0; y < bm->height; y++) png_free(png, rows[y]);
    png_free(png, rows);

png_failure:
	png_destroy_write_struct(&png, &pngInfo);
png_create_write_struct_failed:
	fclose(fp);
fopen_failed:
	return result;
}
