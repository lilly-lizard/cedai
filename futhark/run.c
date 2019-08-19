#include <stdio.h>
#include <png.h>

#define CL_TARGET_OPENCL_VERSION 120
#include "render.h"

enum RESULT {
	SUCCESS,
	FAILURE
};

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
} Pixel;

typedef struct {
	size_t width;
	size_t height;
	Pixel *pixels;
} Bitmap;

Bitmap render();
Bitmap genGradient();
Pixel* getPixel(const Bitmap *bm, int x, int y);
enum RESULT writePNG(const Bitmap *bm, const char *path);

int main() {
	// futhark program
	Bitmap bm = render();
	//bitmap bm = genGradient();

	// write to png file
	char *filePath = "render.png";
	enum RESULT res = writePNG(&bm, filePath);
	if (res != SUCCESS) printf("Error writing to %s", filePath);

	// clean up
	free(bm.pixels);
	return res;
}

Bitmap genGradient() {
	Bitmap gradient;
	gradient.width = 200;
	gradient.height = 200;
	gradient.pixels = calloc(gradient.width * gradient.height, sizeof(Pixel));

	// set pixels
	for (int x = 0; x < gradient.width; x++) {
		for (int y = 0; y < gradient.height; y++) {
			Pixel *p = getPixel(&gradient, x, y);
			p->r = 255 * x / gradient.width;
			p->g = 255 * y / gradient.height;
			p->b = 255;
			p->a = 255;
		}
	}

	return gradient;
}

void convertFutharkPixels(Bitmap *bm, uint8_t *pixels, size_t width, size_t height) {
	bm->width = width;
	bm->height = height;
	bm->pixels = calloc(width * height, sizeof(Pixel));
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			Pixel *p = bm->pixels + width * y + x;
			p->r = *pixels++;
			p->g = *pixels++;
			p->b = *pixels++;
			p->a = *pixels++;
		}
	}
}

typedef struct {
	float *positions;
	float *radii;
	uint8_t *colors;
	int numElements;
} SphereVector;

void initVector(SphereVector *vector) {
	vector->positions = NULL;
	vector->radii = NULL;
	vector->colors = NULL;
	vector->numElements = 0;
}

typedef struct {
	float position[3];
	float radius;
	uint8_t color[4];
} Sphere;

void addSphere(SphereVector *vector, Sphere sphere) {
	vector->numElements++;

	// resize arrays
	vector->positions = realloc(vector->positions, sizeof(float) * 3 * vector->numElements);
	vector->radii = realloc(vector->radii, sizeof(float) * vector->numElements);
	vector->colors = realloc(vector->colors, sizeof(uint8_t) * 4 * vector->numElements);

	// write position
	float *p = vector->positions + 3 * (vector->numElements - 1);
	*p = sphere.position[0];
	*(p + 1) = sphere.position[1];
	*(p + 2) = sphere.position[2];

	// write radius
	float *r = vector->radii + (vector->numElements - 1);
	*r = sphere.radius;

	// write color
	uint8_t *c = vector->colors + 4 * (vector->numElements - 1);
	*c = sphere.color[0];
	*(c + 1) = sphere.color[1];
	*(c + 2) = sphere.color[2];
	*(c + 3) = sphere.color[3];
}

void freeVector(SphereVector *vector) {
	free(vector->positions);
	free(vector->radii);
	free(vector->colors);
	initVector(vector);
}

Bitmap render() {
	struct futhark_context_config *cfg = futhark_context_config_new();
	struct futhark_context *ctx = futhark_context_new(cfg);

	// inputs
	int32_t width = 640;
	int32_t height = 480;

	// primitives
	SphereVector spheres;
	initVector(&spheres);

	// spheres
	Sphere sphere1 = {.position = {5, 0, 0}, .radius = 1, .color = {50, 255, 255, 255}};
	addSphere(&spheres, sphere1);
	Sphere sphere2 = {.position = {10, 1, 1,}, .radius = 2, .color = {255, 200, 50, 255}};
	addSphere(&spheres, sphere2);
	Sphere sphere3 = {.position = {7, -1, -1}, .radius = 2, .color = {255, 90, 150, 255}};
	addSphere(&spheres, sphere3);
	int32_t numSpheres = spheres.numElements;

	// lights
	Sphere light1 = {.position = {0, 3, 4}, .radius = 0.1, .color = {255, 255, 255, 255}};
	addSphere(&spheres, light1);
	Sphere light2 = {.position = {3, -1, 0}, .radius = 0.1, .color = {255, 255, 255, 255}};
	addSphere(&spheres, light2);
	int32_t numLights = spheres.numElements - numSpheres;

	struct futhark_f32_2d *sPositions = futhark_new_f32_2d(ctx, spheres.positions, spheres.numElements, 3);
	struct futhark_f32_1d *sRadii = futhark_new_f32_1d(ctx, spheres.radii, spheres.numElements);
	struct futhark_u8_2d *sColors = futhark_new_u8_2d(ctx, spheres.colors, spheres.numElements, 4);

	// output
	uint8_t *pixelsArray = calloc(width * height * 4, sizeof(uint8_t));
	struct futhark_u8_2d *pixelsFuthark = futhark_new_u8_2d(ctx, pixelsArray, width * height, 4);

	// run program
	futhark_entry_main(ctx, &pixelsFuthark, width, height,
		numSpheres, numLights, sPositions, sRadii, sColors);
	futhark_context_sync(ctx);

	// get pixels
	futhark_values_u8_2d(ctx, pixelsFuthark, pixelsArray);
	Bitmap pixelsBitmap;
	convertFutharkPixels(&pixelsBitmap, pixelsArray, width, height);

	// clean up
	freeVector(&spheres);
	futhark_free_f32_2d(ctx, sPositions);
	futhark_free_f32_1d(ctx, sRadii);
	futhark_free_u8_2d(ctx, sColors);

	futhark_free_u8_2d(ctx, pixelsFuthark);
	free(pixelsArray);
	futhark_context_free(ctx);
	futhark_context_config_free(cfg);

	return pixelsBitmap;
}

Pixel* getPixel(const Bitmap *bm, int x, int y) {
	return bm->pixels + bm->width * y + x;
}

enum RESULT writePNG(const Bitmap *bm, const char *path) {
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
			Pixel *pixel = getPixel(bm, x, y);
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
