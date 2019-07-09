
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

__kernel void entry(__constant Sphere* spheres,
					const int sphere_count, const int light_count,
					__global float3* rays,
					__global float* ts,
					__global uchar4* output)  /* rgb [0 - 255] */
{
	const uint work_item_id = get_global_id(0) + get_global_id(1) * get_global_size(0);
	output[work_item_id] = (uchar4)(draw_background(rays[work_item_id]), 0);
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
