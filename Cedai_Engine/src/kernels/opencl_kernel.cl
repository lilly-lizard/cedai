/* https://stackoverflow.com/questions/4911400/shader-optimization-is-a-ternary-operator-equivalent-to-branching */
// TODO use half instead of float (half_rsqrt, fast_normalize etc)

typedef struct Sphere
{
	float radius;
	float3 pos;
	uchar3 color;
} Sphere;

// DECLARATIONS AND CONSTANTS

#define AMBIENT 0.4f
#define LIGHT_STEP 0.2f

float sphere_intersect(float3 ray_o, float3 ray_d, float3 center, float radius);
float diffuse(float3 normal, float3 intersection, float3 light);
float ceiling(float value, float multiple);
uchar3 draw_background(float3 ray_d);

// ENTRY POINT

__kernel void entry(const float16 view,
					__constant Sphere* spheres,
					const int sphere_count, const int light_count,
					__global float3* rays,
					__global uchar4* output)
{
	const uint work_item_id = get_global_id(0) + get_global_id(1) * get_global_size(0);
	
	// create a camera ray
	const float3 ray_o = (float3)(view[12], view[13], view[14]);
	const float3 ray_d = rays[work_item_id];

	// check for intersections
	uchar3 color = (uchar3)(0, 0, 0);
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
			for (int l = sphere_count; l < sphere_count + light_count; l++)
				light += diffuse(intersection - spheres[s].pos, intersection, spheres[l].pos);
			light = clamp(ceiling(light, LIGHT_STEP), AMBIENT, 1.0f);
			color = (uchar3)(spheres[s].color.x * light, spheres[s].color.y * light, spheres[s].color.z * light);
	}	}

	// lights
	for (int l = sphere_count; l < sphere_count + light_count; l++) {
		float t = sphere_intersect(ray_o, ray_d, spheres[l].pos, spheres[l].radius);
		if (0 < t && t < min_t) {
			color_found = true;
			min_t = t;
			color = spheres[l].color;
	}	}

	if (!color_found)
		color = draw_background(ray_d);
	output[work_item_id] = (uchar4)(color, 0);
}

// HELPER FUNCTIONS

float sphere_intersect(float3 ray_o, float3 ray_d, float3 center, float radius)
{ 
	// https://github.com/straaljager/OpenCL-path-tracing-tutorial-2-Part-1-Raytracing-a-sphere/blob/master/opencl_kernel.cl
	// a = P1 . P1 = 1 (assuming ray_d is normalized)
	float3 d = center - ray_o;
	float b = dot(d, ray_d);
	float c = dot(d, d) - radius * radius;

	float discriminant = b * b - c;
	if (discriminant < 0) return -1;

	float t = b - sqrt(discriminant);
	if (t < 0) {
		// the following 2 lines allows you to see inside the sphere
		t = b - t + b; // b + sqrt(discriminant)
		if (t < 0)
			return -1;
	}
	return t;
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
	return (uchar3)(color.x * 255, color.y * 255, color.z * 255);
}
