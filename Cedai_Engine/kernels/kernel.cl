/* https://stackoverflow.com/questions/4911400/shader-optimization-is-a-ternary-operator-equivalent-to-branching */
// todo https://en.wikipedia.org/wiki/Phong_reflection_model

#pragma OPENCL EXTENSION cl_khr_gl_sharing : enable

typedef struct
{
	float radius;
	float3 pos;
	uint4 color;
} Sphere;

enum primitive_type { NONE, SPHERE, LIGHT, POLYGON };

// CONFIG AND CONSTANTS

//#define HALF_RESOLUTION /* MUST to be the same in src/tools/config.hpp */
#ifdef HALF_RESOLUTION
#	define DITHER
#endif

#define DROP_OFF 1000

#define LIGHT_RADIUS 3
#define LIGHT_W PI / 4.37499f

#define AMBIENT 0.2f
#define LIGHT_STEP 0.2f

#define BACKGROUND_OFFSET 0.3f
#define BACKGROUND_MULTIPLIER 0.5f

#define PI 3.14159265359f

const sampler_t sampler = CLK_FILTER_NEAREST | CLK_ADDRESS_CLAMP | CLK_NORMALIZED_COORDS_FALSE;

// DECLARATIONS

float sphere_intersect(float3 ray_o, float3 ray_d, float3 center, float radius);
float triangle_intersect(float3 O, float3 D, float3 V0, float3 V1, float3 V2);

float diffuse_sphere(float3 normal, float3 intersection, float3 light);
float diffuse_polygon(float3 normal, float3 intersection, float3 light_pos, float3 ray_d);
float ceiling(float value, float multiple);
bool shadow(float3 intersection, float3 light, int s_index, int p_index, const int sphere_count, const int polygon_count,
			__constant Sphere* __restrict spheres, __constant float4* __restrict vertices);
int luminance(uchar4 color);

void draw(__write_only image2d_t output, uchar4 color, float3 ray_d, int2 coord);
void draw_half_res(__write_only image2d_t output, uchar4 color, float3 ray_d, int2 coord);
void draw_dither(__write_only image2d_t output, uchar4 color, bool no_color_found, float3 ray_d, int2 coord);
uchar4 background_color(float3 ray_d);

// ENTRY POINT

__attribute__((work_group_size_hint(16, 16, 1)))
__kernel void render(// inputs
					 const float16 view, const float3 ray_o, const float time,
					 const int sphere_count, const int light_count, const int polygon_count,
					 // buffers
					 __constant Sphere* __restrict spheres,
					 __constant float4* __restrict vertices,
					 __constant uchar4* __restrict polygon_colors,
					 // output
					 __write_only image2d_t output)
{
	const int2 coord = (int2)(get_global_id(0), get_global_id(1));
	const int2 dim = (int2)(get_global_size(0), get_global_size(1));

	// create a camera ray
	const float3 uv = (float3)(dim.x, (float)coord.x - (float)dim.x / 2, (float)dim.y / 2 - coord.y);
	const float3 ray_d = fast_normalize((float3)(uv.x * view.s0 + uv.y * view.s4 + uv.z * view.s8,
												 uv.x * view.s1 + uv.y * view.s5 + uv.z * view.s9,
												 uv.x * view.s2 + uv.y * view.s6 + uv.z * view.sA));

	// check for intersections
	uchar4 color = (uchar4)(0, 0, 0, 0);
	float min_t = DROP_OFF; // drop off distance
	int index = 0;
	enum primitive_type primitive_found = NONE;

	const int sphere_total = sphere_count + light_count;
	const float3 light_offset = (float3)(LIGHT_RADIUS * cos(LIGHT_W * time), LIGHT_RADIUS * sin(LIGHT_W * time), 0);

	// spheres
	for (int s = 0; s < sphere_count; s++) {
		Sphere sphere = spheres[s];
		float t = sphere_intersect(ray_o, ray_d, sphere.pos, sphere.radius);
		if (0 < t && t < min_t) {
			primitive_found = SPHERE;
			min_t = t;
			color = convert_uchar4(sphere.color);
			index = s;
	}	}

	// lights
	for (int l = sphere_count; l < sphere_total; l++) {
		Sphere light = spheres[l];
		float t = sphere_intersect(ray_o, ray_d, light.pos + light_offset * (l % 2 * 2 - 1), light.radius);
		if (0 < t && t < min_t) {
			primitive_found = LIGHT;
			min_t = t;
			color = convert_uchar4(light.color);
			index = l;
	}	}

	// polygons
	for (int p = 0; p < polygon_count; p++) {
		float t = triangle_intersect(ray_o, ray_d, vertices[p * 3].xyz, vertices[p * 3 + 1].xyz, vertices[p * 3 + 2].xyz);
		if (0 < t && t < min_t) {
			primitive_found = POLYGON;
			min_t = t;
			color = polygon_colors[p];
			index = p;
	}	}

	// no intersection
	if (primitive_found == NONE) {
		color = background_color(ray_d);

	// sphere lighting
	} else if (primitive_found == SPHERE) {
		float light = 0;
		float3 intersection = mad(min_t, ray_d, ray_o);

		for (int l = sphere_count; l < sphere_total; l++) {
			float3 light_pos = spheres[l].pos + light_offset * (l % 2 * 2 - 1);
			bool in_shadow = shadow(intersection, light_pos, index, -1, sphere_count, polygon_count, spheres, vertices);
			if (!in_shadow)
				light += diffuse_sphere(intersection - spheres[index].pos, intersection,
										spheres[l].pos + light_offset * (l % 2 * 2 - 1));
		}
		light = clamp(ceiling(light, LIGHT_STEP), AMBIENT, 1.0f);
		color = convert_uchar4(convert_float4(color) * light);

	// polygon lighting
	} else if (primitive_found == POLYGON) {
		float light = AMBIENT;
		float3 intersection = mad(min_t, ray_d, ray_o);
		float3 v0 = vertices[index * 3].xyz;
		float3 v1 = vertices[index * 3 + 1].xyz;
		float3 v2 = vertices[index * 3 + 2].xyz;

		for (int l = sphere_count; l < sphere_total; l++) {
			float3 light_pos = spheres[l].pos + light_offset * (l % 2 * 2 - 1);
			bool in_shadow = shadow(intersection, light_pos, -1, index, sphere_count, polygon_count, spheres, vertices);
			if (!in_shadow)
				light += diffuse_polygon(cross(v1 - v0, v2 - v0), intersection, light_pos, ray_d);
		}
		light = clamp(light, 0.0f, 1.0f);
		color = convert_uchar4(convert_float4(color) * light);
	}

	// draw
#ifndef HALF_RESOLUTION
	draw(output, color, ray_d, coord);
#else
#ifdef DITHER
	draw_dither(output, color, primitive_found == NONE, ray_d, coord);
#else
	draw_half_res(output, color, ray_d, coord);
#endif
#endif
}

// INTERSECTION FUNCTIONS

float sphere_intersect(float3 ray_o, float3 ray_d, float3 center, float radius)
{
	// a = P1 . P1 = 1 (assuming ray_d is normalized)
	float3 d = center - ray_o;
	float b = dot(d, ray_d);
	float c = dot(d, d) - native_powr(radius, 2);

	float discriminant = native_powr(b, 2) - c;
	if (discriminant < 0) return -1;

	float t = b - native_sqrt(discriminant);
	return 0 < t ? t : -1;
}

float triangle_intersect(float3 O, float3 D, float3 V0, float3 V1, float3 V2)
{
	// M�ller-Trumbore algorithm. we use Cramer's rule to find [t,u,v]
	// https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/moller-trumbore-ray-triangle-intersection
	// returns -1 for no intersection, otherwise returns t (where intersection = O + tD)

	float3 E1 = V1 - V0;
	float3 E2 = V2 - V0;
	float3 P = cross(D, E2);

	float inv_det0 = native_recip(dot(P, E1));
	float3 T = O - V0;

	float u = dot(T, P) * inv_det0;
	if (u < 0 || 1 < u) return -1;

	float3 Q = cross(T, E1);
	float v = dot(Q, D) * inv_det0;

	return v < 0 || 1 < u + v ? -1 : dot(Q, E2) * inv_det0;
}

// LIGHTING FUNCTIONS

float diffuse_sphere(float3 normal, float3 intersection, float3 light)
{
	return clamp(dot(fast_normalize(normal), fast_normalize(light - intersection)), 0.0f, 1.0f);
}

float diffuse_polygon(float3 normal, float3 intersection, float3 light_pos, float3 ray_d)
{
	float3 light_to_int = intersection - light_pos;
	float light = fabs(dot(fast_normalize(normal), fast_normalize(light_to_int)));
	return sign(dot(normal, light_to_int)) == sign(dot(normal, ray_d)) ? light : 0;
}

float ceiling(float value, float multiple)
{
	return ceil(value/multiple) * multiple;
}

bool shadow(float3 intersection, float3 light, int s_index, int p_index, const int sphere_count, const int polygon_count,
			__constant Sphere* __restrict spheres, __constant float4* __restrict vertices) {
	float3 ray_d = fast_normalize(light - intersection);
	float3 ray_o = intersection;

	// spheres
	for (int s = 0; s < sphere_count; s++) {
		if (s == s_index) continue;
		Sphere sphere = spheres[s];
		float t = sphere_intersect(ray_o, ray_d, sphere.pos, sphere.radius);
		if (0 < t && t < 100) return true;
	}

	// polygons
	for (int p = 0; p < polygon_count; p++) {
		if (p == p_index) continue;
		float t = triangle_intersect(ray_o, ray_d, vertices[p * 3].xyz, vertices[p * 3 + 1].xyz, vertices[p * 3 + 2].xyz);
		if (0 < t) return true;
	}

	return false;
}

int luminance(uchar4 color) {
	// from https://stackoverflow.com/questions/596216/formula-to-determine-brightness-of-rgb-color
	const uchar r = color.x;
	const uchar g = color.y;
	const uchar b = color.z;
	return (r + r + r + g + g + g + g + b) >> 3;
}

// DRAWING FUNCTIONS

void draw(__write_only image2d_t output, uchar4 color, float3 ray_d, int2 coord)
{
	uint4 color_out = convert_uint4(color);
	write_imageui(output, coord, color_out);
}

void draw_half_res(__write_only image2d_t output, uchar4 color, float3 ray_d, int2 coord)
{
	uint4 color_out = convert_uint4(color);
	write_imageui(output, (int2)(coord.x * 2, coord.y * 2), color_out);
	write_imageui(output, (int2)(coord.x * 2 + 1, coord.y * 2 + 1), color_out);
	write_imageui(output, (int2)(coord.x * 2 + 1, coord.y * 2), color_out);
	write_imageui(output, (int2)(coord.x * 2, coord.y * 2 + 1), color_out);
}

void draw_dither(__write_only image2d_t output, uchar4 color, bool no_color_found, float3 ray_d, int2 coord)
{
	int dither;
	if (no_color_found) dither = 2;
	else dither = luminance(color) / 64;
	//int dither = select(luminance(color) / 64, 2, (int)no_color_found);

	// write
	uint4 color_out = convert_uint4(color);
	write_imageui(output, (int2)(coord.x * 2, coord.y * 2), color_out);

	if (dither < 1) color_out = (uint4)(0,0,0,0);
	write_imageui(output, (int2)(coord.x * 2 + 1, coord.y * 2 + 1), color_out);

	if (dither < 2) color_out = (uint4)(0,0,0,0);
	write_imageui(output, (int2)(coord.x * 2 + 1, coord.y * 2), color_out);

	if (dither < 3) color_out = (uint4)(0,0,0,0);
	write_imageui(output, (int2)(coord.x * 2, coord.y * 2 + 1), color_out);
}

uchar4 background_color(float3 ray_d)
{
	float3 color = fabs(ray_d) * BACKGROUND_MULTIPLIER + BACKGROUND_OFFSET;
	return (uchar4)(convert_uchar3(color * 255), 0);
}

/*
constant vs. global: https://github.com/RadeonOpenCompute/ROCm/issues/203
*/
