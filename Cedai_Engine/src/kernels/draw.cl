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

float diffuse(float3 normal, float3 intersection, float3 light);
float ceiling(float value, float multiple);
uchar3 draw_background(float3 ray_d);

// DRAW

__attribute__((work_group_size_hint(16, 16, 1)))
__kernel void draw(const float16 view, const float3 ray_o,
				   const int sphere_count, const int light_count,
				   __global Sphere* __restrict spheres,
				   __local Sphere* __restrict spheres_local,
				   __constant float* __restrict ts,
				   __write_only image2d_t output)  /* rgb [0 - 255] */
{
	event_t copy_event = async_work_group_copy((__local char *)spheres_local, (__global char *)spheres,
		(sphere_count + light_count) * sizeof(Sphere), 0);
	
	// create a camera ray
	const int2 coord = (int2)(get_global_id(0), get_global_id(1));
	const int2 dim = (int2)(get_global_size(0), get_global_size(1));

	const float3 uv = (float3)(dim.x, (float)coord.x - (float)dim.x / 2, (float)(dim.y - coord.y) - (float)dim.y / 2);
	const float3 ray_d = fast_normalize((float3)(uv.x * view[0] + uv.y * view[4] + uv.z * view[8],
												 uv.x * view[1] + uv.y * view[5] + uv.z * view[9],
												 uv.x * view[2] + uv.y * view[6] + uv.z * view[10]));

	// check for intersections
	const uint total_spheres = sphere_count + light_count;
	uchar3 color = (uchar3)(0, 0, 0);
	bool color_found = false;
	float min_t = 100; // drop off distance

	const uint ts_work_id = coord.x + coord.y * dim.x;
	const uint work_size = dim.x * dim.y;

	// spheres
	wait_group_events(1, &copy_event);
	for (int s = 0; s < sphere_count; s++) {
		float t = ts[ts_work_id + s * work_size];
		if (0 < t && t < min_t) {
			color_found = true;
			min_t = t;
			// calculate light
			float light = 0;
			float3 intersection = mad(min_t, ray_d, ray_o);
			for (int l = sphere_count; l < total_spheres; l++)
				light += diffuse(intersection - spheres_local[s].pos, intersection, spheres_local[l].pos);
			light = clamp(ceiling(light, LIGHT_STEP), AMBIENT, 1.0f);
			color = convert_uchar3(convert_float3(spheres_local[s].color) * light);
	}	}

	// lights
	for (int l = sphere_count; l < total_spheres; l++) {
		float t = ts[ts_work_id + l * work_size];
		if (0 < t && t < min_t) {
			color_found = true;
			min_t = t;
			color = spheres_local[l].color;
	}	}

	if (!color_found)
		color = draw_background(ray_d);
	write_imageui(output, coord, (uint4)(color.x, color.y, color.z, 0));
}

// HELPER FUNCTIONS

float diffuse(float3 normal, float3 intersection, float3 light)
{
	return clamp(dot(fast_normalize(normal), fast_normalize(light - intersection)), 0.0f, 1.0f);
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

/*
global synchronisation: https://industrybestpractice.blogspot.com/2012/07/global-synchronisation-in-opencl.html
*/