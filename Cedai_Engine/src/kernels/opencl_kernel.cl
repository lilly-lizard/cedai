/* https://stackoverflow.com/questions/4911400/shader-optimization-is-a-ternary-operator-equivalent-to-branching */
// TODO use half instead of float (half_rsqrt, fast_normalize etc)

#pragma OPENCL EXTENSION cl_khr_gl_sharing : enable

typedef struct Sphere
{
	float radius;
	float3 pos;
	uchar3 color;
} Sphere;

// DECLARATIONS AND CONSTANTS

#define AMBIENT 0.4f
#define LIGHT_STEP 0.2f

const sampler_t sampler = CLK_FILTER_NEAREST | CLK_ADDRESS_CLAMP | CLK_NORMALIZED_COORDS_FALSE;

float sphere_intersect(float3 ray_o, float3 ray_d, float3 center, float radius);
float diffuse(float3 normal, float3 intersection, float3 light);
float ceiling(float value, float multiple);
uchar3 draw_background(float3 ray_d);

// ENTRY POINT

__kernel void render_kernel(const float16 view, const float3 ray_o,
							__global Sphere* spheres,
							const int sphere_count, const int light_count,
							__write_only image2d_t output)
{
	const int2 coord = (int2)(get_global_id(0), get_global_id(1));
	const int2 dim = (int2)(get_global_size(0), get_global_size(1));
	
	// create a camera ray
	const float3 uv = (float3)(dim.x, (float)coord.x - (float)dim.x / 2, (float)(dim.y - coord.y) - (float)dim.y / 2);
	const float3 ray_d = normalize((float3)(uv.x * view[0] + uv.y * view[4] + uv.z * view[8],
										    uv.x * view[1] + uv.y * view[5] + uv.z * view[9],
										    uv.x * view[2] + uv.y * view[6] + uv.z * view[10]));
	// check for intersections
	uchar3 color = (uchar3)(0, 0, 0);
	bool color_found = false;
	float min_t = 100; // drop off distance
	float t;

	// spheres
	for (int s = 0; s < sphere_count; s++) {
		t = sphere_intersect(ray_o, ray_d, spheres[s].pos, spheres[s].radius);
		if (0 < t && t < min_t) {
			color_found = true;
			min_t = t;
			// calculate light
			float light = 0;
			float3 intersection = ray_o + t * ray_d;
			for (int l = sphere_count; l < sphere_count + light_count; l++)
				light += diffuse(intersection - spheres[s].pos, intersection, spheres[l].pos);
			light = clamp(ceiling(light, LIGHT_STEP), AMBIENT, 1.0f);
			color = convert_uchar3(convert_float3(spheres[s].color) * light);
	}	}

	// lights
	for (int l = sphere_count; l < sphere_count + light_count; l++) {
		t = sphere_intersect(ray_o, ray_d, spheres[l].pos, spheres[l].radius);
		if (0 < t && t < min_t) {
			color_found = true;
			min_t = t;
			color = spheres[l].color;
	}	}

	if (!color_found)
		color = draw_background(ray_d);
	write_imageui(output, coord, (uint4)(color.x, color.y, color.z, 0));
}

// HELPER FUNCTIONS

float sphere_intersect(float3 ray_o, float3 ray_d, float3 center, float radius)
{ 
	// https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
	// a = P1 . P1 = 1 (assuming ray_d is normalized)
	float3 d = ray_o - center;
	float b = dot(2 * ray_d, d); // 2P1 . (P0 - C)
	float c = dot(d, d) - radius * radius; // (P0 - C) . (P0 - C) - r^2

	float discriminant = b*b - 4*c;
	if (discriminant < 0) return -1;

	float t0, t1, t;
	t0 = b > 0 ? -0.5 * (b + sqrt(discriminant)) : -0.5 * (b - sqrt(discriminant));
	t1 = c / t0;

	t = t0 < t1 ? t0 : t1;
	return t > 0 ? t : -1;
}

float diffuse(float3 normal, float3 intersection, float3 light)
{
	return clamp(dot(normalize(normal), normalize(light - intersection)), 0.0f, 1.0f);
}

// rounds up to the nearest multiple of multiple
float ceiling(float value, float multiple)
{
	return ceil(value/multiple) * multiple;
}

uchar3 draw_background(float3 ray_d)
{
	float offset = 0.3f;
	float mult = 0.4f;
	float3 color = fabs(ray_d) * mult + offset;
	return convert_uchar3(color * 255);
}
