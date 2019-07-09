
typedef struct Sphere
{
	float radius;
	float3 pos;
	uchar3 color;
} Sphere;

float sphere_intersect(float3 ray_o, float3 ray_d, float3 center, float radius);

// SPHERE INTERSECTION

__kernel void entry(const float3 ray_o,
					__constant Sphere* spheres,
					__global float3* rays,
					__global float* ts) /* the t value of the intersection between ray[pixel] and sphere[id] */
{
	ts[get_global_id(2) +
	   get_global_id(0) * get_global_size(2) +
	   get_global_id(1) * get_global_size(2) * get_global_size(0)]
	   = sphere_intersect(ray_o,
						  rays[get_global_id(0) + get_global_id(1) * get_global_size(0)],
						  spheres[get_global_id(2)].pos,
						  spheres[get_global_id(2)].radius);
}

float sphere_intersect(float3 ray_o, float3 ray_d, float3 center, float radius) {
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