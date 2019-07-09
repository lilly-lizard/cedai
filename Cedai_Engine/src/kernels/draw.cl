
typedef struct Sphere
{
	float radius;
	float3 pos;
	uchar3 color;
} Sphere;

// DECLARATIONS AND CONSTANTS

#define AMBIENT 0.4f
#define LIGHT_STEP 0.2f

float diffuse(float3 normal, float3 intersection, float3 light);
float ceiling(float value, float multiple);
uchar3 draw_background(float3 ray_d);

// DRAW

__kernel void entry(const float3 ray_o,
					__constant Sphere* spheres,
					const int sphere_count, const int light_count,
					__global float3* rays,
					__global float* ts,
					__global uchar4* output)  /* rgb [0 - 255] */
{
	const uint work_item_id = get_global_id(0) + get_global_id(1) * get_global_size(0);
	const uint ts_work_id = get_global_id(0) * (sphere_count + light_count)
						  + get_global_id(1) * (sphere_count + light_count) * get_global_size(0);
	const float3 ray_d = rays[work_item_id];

	// check for intersections
	uchar3 color = (uchar3)(0, 0, 0);
	bool color_found = false;
	float min_t = 100; // drop off distance
	float t;

	// spheres
	for (int s = 0; s < sphere_count; s++) {
		t = ts[s + ts_work_id];
		if (0 < t && t < min_t) {
			color_found = true;
			min_t = t;
			// calculate light
			float light = 0;
			float3 intersection = ray_o + t * ray_d;
			for (int l = sphere_count; l < sphere_count + light_count; l++)
				light += diffuse(intersection - spheres[s].pos, intersection, spheres[l].pos);
			light = clamp(ceiling(light, LIGHT_STEP), AMBIENT, 1.0f);
			color = (uchar3)(spheres[s].color.x * light, spheres[s].color.y * light, spheres[s].color.z * light);
	}	}

	// lights
	for (int l = sphere_count; l < sphere_count + light_count; l++) {
		t = ts[l + ts_work_id];
		if (0 < t && t < min_t) {
			color_found = true;
			min_t = t;
			color = spheres[l].color;
	}	}

	if (!color_found)
		color = draw_background(ray_d);
	output[work_item_id] = (uchar4)(color, 0);

	//const uint work_item_id = get_global_id(0) + get_global_id(1) * get_global_size(0);
	//output[work_item_id] = (uchar4)(draw_background(rays[work_item_id]), 0);
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
	return (uchar3)(color.x * 255, color.y * 255, color.z * 255);
}
