
typedef struct Sphere
{
	float radius;
	float3 pos;
	uchar3 color;
} Sphere;

const sampler_t sampler = CLK_FILTER_NEAREST | CLK_ADDRESS_CLAMP | CLK_NORMALIZED_COORDS_FALSE;

float intersection(float3 ray_o, float3 ray_d, float3 center, float radius);

// SPHERE INTERSECTION

__kernel void sphere_intersect(const float16 view, const float3 ray_o,
							   __constant Sphere* spheres,
							   __write_only image2d_array_t ts) /* the t value of the intersection between ray[pixel] and sphere[id] */
{
	const int2 coord = (int2)(get_global_id(0), get_global_id(1));
	const int2 dim = (int2)(get_global_size(0), get_global_size(1));
	const int4 coord_ts = (int4)(coord, get_global_id(2), 0);

	// create a camera ray
	const float3 uv = (float3)(dim.x, (float)coord.x - (float)dim.x / 2, (float)(dim.y - coord.y) - (float)dim.y / 2);
	const float3 ray_d = normalize((float3)(uv.x * view[0] + uv.y * view[4] + uv.z * view[8],
										    uv.x * view[1] + uv.y * view[5] + uv.z * view[9],
										    uv.x * view[2] + uv.y * view[6] + uv.z * view[10]));

	const float t = intersection(ray_o, ray_d, spheres[get_global_id(2)].pos, spheres[get_global_id(2)].radius);
	write_imagef(ts, coord_ts, t);
}

float intersection(float3 ray_o, float3 ray_d, float3 center, float radius) {
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