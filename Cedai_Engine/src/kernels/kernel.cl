/* https://stackoverflow.com/questions/4911400/shader-optimization-is-a-ternary-operator-equivalent-to-branching */

#pragma OPENCL EXTENSION cl_khr_gl_sharing : enable

typedef struct Sphere
{
	float radius;
	float3 pos;
	uchar3 color;
} Sphere;

// DECLARATIONS AND CONSTANTS

// to change resolution, change the output writing code as well
#define RESOLUTION 2

#define AMBIENT 0.2f
#define LIGHT_STEP 0.2f

#define BACKGROUND_OFFSET 0.3f
#define BACKGROUND_MULTIPLIER 0.4f

const sampler_t sampler = CLK_FILTER_NEAREST | CLK_ADDRESS_CLAMP | CLK_NORMALIZED_COORDS_FALSE;

float sphere_intersect(float3 ray_o, float3 ray_d, float3 center, float radius);
float triangle_intersect(float3 O, float3 D, float3 V0, float3 V1, float3 V2);

float diffuse_sphere(float3 normal, float3 intersection, float3 light);
float diffuse_polygon(float3 normal, float3 intersection, float3 light, float3 ray_d);
float ceiling(float value, float multiple);

uchar3 draw_background(float3 ray_d);

// ENTRY POINT

__kernel void render(const float16 view, const float3 ray_o,
					 const int sphere_count, const int light_count, const int polygon_count,
					 __constant Sphere* spheres, __constant float3* vertices, __constant uchar3* polygon_colors,
					 __write_only image2d_t output)
{
	const int2 coord = (int2)(get_global_id(0), get_global_id(1));
	const int2 dim = (int2)(get_global_size(0), get_global_size(1));
	
	// create a camera ray
	const float3 uv = (float3)(dim.x, (float)coord.x - (float)dim.x / 2, (float)(dim.y - coord.y) - (float)dim.y / 2);
	const float3 ray_d = fast_normalize((float3)(uv.x * view[0] + uv.y * view[4] + uv.z * view[8],
												 uv.x * view[1] + uv.y * view[5] + uv.z * view[9],
												 uv.x * view[2] + uv.y * view[6] + uv.z * view[10]));
	// check for intersections
	uchar3 color = (uchar3)(0, 0, 0);
	bool sphere_found = false;
	bool light_found = false;
	bool polygon_found = false;
	float min_t = 100; // drop off distance
	int index = 0;
	const int sphere_total = sphere_count + light_count;

	// spheres
	for (int s = 0; s < sphere_count; s++) {
		Sphere sphere = spheres[s];
		float t = sphere_intersect(ray_o, ray_d, sphere.pos, sphere.radius);
		if (0 < t && t < min_t) {
			sphere_found  = true;
			light_found   = false;
			polygon_found = false;
			min_t = t;
			color = sphere.color;
			index = s;
	}	}

	// lights
	for (int l = sphere_count; l < sphere_total; l++) {
		Sphere light = spheres[l];
		float t = sphere_intersect(ray_o, ray_d, light.pos, light.radius);
		if (0 < t && t < min_t) {
			sphere_found  = false;
			light_found   = true;
			polygon_found = false;
			min_t = t;
			color = light.color;
			index = l;
	}	}

	// polygons
	for (int p = 0; p < polygon_count; p++) {
		float t = triangle_intersect(ray_o, ray_d, vertices[p * 3], vertices[p * 3 + 1], vertices[p * 3 + 2]);
		if (0 < t && t < min_t) {
			sphere_found  = false;
			light_found   = false;
			polygon_found = true;
			min_t = t;
			color = polygon_colors[p];
			index = p;
	}	}

	// lighting
	if (sphere_found) {
		float light = 0;
		float3 intersection = mad(min_t, ray_d, ray_o);
		for (int l = sphere_count; l < sphere_total; l++)
			light += diffuse_sphere(intersection - spheres[index].pos, intersection, spheres[l].pos);
		light = clamp(ceiling(light, LIGHT_STEP), AMBIENT, 1.0f);
		color = convert_uchar3(convert_float3(color) * light);

	} else if (polygon_found) {
		float3 v0 = vertices[index * 3];
		float3 v1 = vertices[index * 3 + 1];
		float3 v2 = vertices[index * 3 + 2];
		float light = 0;
		for (int l = sphere_count; l < sphere_total; l++)
			light += diffuse_polygon(cross(v1 - v0, v2 - v0), mad(min_t, ray_d, ray_o), spheres[l].pos, ray_d);
		light = clamp(light, AMBIENT, 1.0f);
		color = convert_uchar3(convert_float3(color) * light);
	}

	// dither effect and background draw
	int dither = (color.x + color.y + color.z) / 3 / 64;
	if (!(sphere_found || light_found || polygon_found)) {
		color = draw_background(ray_d);
		dither = 2;
	}
	
	// write
	uint4 color_out = (uint4)(color.x, color.y, color.z, 0);
	write_imageui(output, (int2)(coord.x * RESOLUTION, coord.y * RESOLUTION), color_out);
	
	if (dither > 0) write_imageui(output, (int2)(coord.x * RESOLUTION + 1, coord.y * RESOLUTION + 1), color_out);
	else write_imageui(output, (int2)(coord.x * RESOLUTION + 1, coord.y * RESOLUTION + 1), (uint4)(0,0,0,0));
	
	if (dither > 1) write_imageui(output, (int2)(coord.x * RESOLUTION + 1, coord.y * RESOLUTION), color_out);
	else write_imageui(output, (int2)(coord.x * RESOLUTION + 1, coord.y * RESOLUTION), (uint4)(0,0,0,0));
	
	if (dither > 2) write_imageui(output, (int2)(coord.x * RESOLUTION, coord.y * RESOLUTION + 1), color_out);
	else write_imageui(output, (int2)(coord.x * RESOLUTION, coord.y * RESOLUTION + 1), (uint4)(0,0,0,0));
}

// HELPER FUNCTIONS

float sphere_intersect(float3 ray_o, float3 ray_d, float3 center, float radius)
{ 
	// a = P1 . P1 = 1 (assuming ray_d is normalized)
	float3 d = center - ray_o;
	float b = dot(d, ray_d);
	float c = dot(d, d) - radius * radius;

	float discriminant = b * b - c;
	if (discriminant < 0) return -1;

	float t = b - half_sqrt(discriminant);
	return 0 < t ? t : -1;
}

float triangle_intersect(float3 O, float3 D, float3 V0, float3 V1, float3 V2)
{
	// Möller-Trumbore algorithm. we use Cramer's rule to find [t,u,v] 
	// https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/moller-trumbore-ray-triangle-intersection
	// returns -1 for no intersection, otherwise returns t (where intersection = O + tD)
	
	float3 E1 = V1 - V0;
	float3 E2 = V2 - V0;
	float3 P = cross(D, E2);

	float inv_det0 = 1 / dot(P, E1);
	float3 T = O - V0;

	float u = dot(T, P) * inv_det0;
	if (u < 0 || 1 < u) return -1;

	float3 Q = cross(T, E1);
	float v = dot(Q, D) * inv_det0;

	return v < 0 || 1 < u + v ? -1 : dot(Q, E2) * inv_det0;
}

float diffuse_sphere(float3 normal, float3 intersection, float3 light)
{
	return clamp(dot(fast_normalize(normal), fast_normalize(light - intersection)), 0.0f, 1.0f);
}

float diffuse_polygon(float3 normal, float3 intersection, float3 light, float3 ray_d)
{
	float3 light_to_int = intersection - light;
	return sign(dot(normal, light_to_int)) == sign(dot(normal, ray_d)) ?
		clamp(dot(fast_normalize(normal), fast_normalize(light_to_int)), 0.0f, 1.0f) :
		0;
}

// rounds up to the nearest multiple of multiple
float ceiling(float value, float multiple)
{
	return ceil(value/multiple) * multiple;
}

uchar3 draw_background(float3 ray_d)
{
	float3 color = fabs(ray_d) * BACKGROUND_MULTIPLIER + BACKGROUND_OFFSET;
	return convert_uchar3(color * 255);
}
