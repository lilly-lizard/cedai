/* https://stackoverflow.com/questions/4911400/shader-optimization-is-a-ternary-operator-equivalent-to-branching */
// TODO use half instead of float (half_rsqrt, fast_normalize etc)

typedef struct Sphere
{
	float radius;
	float3 pos;
	float3 color;
} Sphere;

// DECLARATIONS AND CONSTANTS

#define AMBIENT 0.4f
#define LIGHT_STEP 0.2f

float sphere_intersect(float3 ray_o, float3 ray_d, float3 center, float radius);
float diffuse(float3 normal, float3 intersection, float3 light);
float ceiling(float value, float multiple);
float3 draw_background(float3 ray_d);

// ENTRY POINT

__kernel void render_kernel(const float16 view,
							const int width, const int height,
							__global float3* rays,
							__constant Sphere* spheres, const int sphere_count,
							__constant Sphere* lights, const int light_count,
							__global float3* output) /* rgb [0 - 1] */
{
	const int work_item_id = get_global_id(0);
	
	// create a camera ray
	const float3 ray_o = (float3)(view[12], view[13], view[14]);
	const float3 ray_d = rays[work_item_id];

	// check for intersections
	float3 color = (float3)(0,0,0);
	bool color_found = false;
	float min_t = 100; // drop off distance

	// spheres
	for (int s = 0; s < sphere_count; s++) {
		float t = sphere_intersect(ray_o, ray_d, spheres[s].pos, spheres[s].radius);
		if (0 < t && t < min_t) {
			color_found = true;
			min_t = t;
			// calculate light
			float light = 0;
			float3 intersection = ray_o + t * ray_d;
			for (int l = 0; l < light_count; l++)
				light += diffuse(intersection - spheres[s].pos, intersection, lights[l].pos);
			color = spheres[s].color * clamp(ceiling(light, LIGHT_STEP), AMBIENT, 1.0f);
	}	}

	// lights
	for (int l = 0; l < light_count; l++) {
		float t = sphere_intersect(ray_o, ray_d, lights[l].pos, lights[l].radius);
		if (0 < t && t < min_t) {
			color_found = true;
			min_t = t;
			color = lights[l].color;
	}	}

	// TODO output [0 - 256]
	if (color_found)
		output[work_item_id] = color;
	else
		output[work_item_id] = draw_background(ray_d);
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

float3 draw_background(float3 ray_d)
{
	float offset = 0.3f;
	float mult = 0.4f;
	return fabs(ray_d) * mult + offset;
}
