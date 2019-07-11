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

__kernel void draw(const float3 ray_o,
				   __global Sphere* spheres,
				   __local Sphere* spheres_local,
				   const int sphere_count, const int light_count,
				   __read_only image2d_t rays,
				   __read_only image2d_array_t ts,
				   __write_only image2d_t output)  /* rgb [0 - 255] */
{
	event_t copy_event = async_work_group_copy((__local char *)spheres_local, (__global char *)spheres,
		(sphere_count + light_count) * sizeof(Sphere), 0);
	wait_group_events(1, &copy_event);

	const float3 ray_d = read_imagef(rays, sampler, (int2)(get_global_id(0), get_global_id(1))).xyz;
	int4 coord_ts = (int4)(get_global_id(0), get_global_id(1), 0, 0);

	// check for intersections
	uchar3 color = (uchar3)(0, 0, 0);
	bool color_found = false;
	float min_t = 100; // drop off distance
	float t;

	// spheres
	for (int s = 0; s < sphere_count; s++) {
		coord_ts.z = s;
		t = read_imagef(ts, sampler, coord_ts).x;
		if (0 < t && t < min_t) {
			color_found = true;
			min_t = t;
			// calculate light
			float light = 0;
			float3 intersection = ray_o + t * ray_d;
			for (int l = sphere_count; l < sphere_count + light_count; l++)
				light += diffuse(intersection - spheres_local[s].pos, intersection, spheres_local[l].pos);
			light = clamp(ceiling(light, LIGHT_STEP), AMBIENT, 1.0f);
			color = convert_uchar3(convert_float3(spheres_local[s].color) * light);
	}	}

	// lights
	for (int l = sphere_count; l < sphere_count + light_count; l++) {
		coord_ts.z = l;
		t = read_imagef(ts, sampler, coord_ts).x;
		if (0 < t && t < min_t) {
			color_found = true;
			min_t = t;
			color = spheres_local[l].color;
	}	}

	if (!color_found)
		color = draw_background(ray_d);
	write_imageui(output, (int2)(get_global_id(0), get_global_id(1)), (uint4)(color.x, color.y, color.z, 0));
}

// HELPER FUNCTIONS

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
