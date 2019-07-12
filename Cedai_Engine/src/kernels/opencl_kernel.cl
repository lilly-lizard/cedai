/* https://stackoverflow.com/questions/4911400/shader-optimization-is-a-ternary-operator-equivalent-to-branching */

// TODO use mad https://www.khronos.org/registry/OpenCL/sdk/1.2/docs/man/xhtml/clBuildProgram.html

#pragma OPENCL EXTENSION cl_khr_gl_sharing : enable

typedef struct Sphere
{
	float radius;
	float3 pos;
	uchar3 color;
} Sphere;

typedef struct Polygon
{
	uint3 indices;
	uchar3 color;
} Polygon;

// DECLARATIONS AND CONSTANTS

#define AMBIENT 0.4f
#define LIGHT_STEP 0.2f

const sampler_t sampler = CLK_FILTER_NEAREST | CLK_ADDRESS_CLAMP | CLK_NORMALIZED_COORDS_FALSE;

float sphere_intersect(float3 ray_o, float3 ray_d, float3 center, float radius);
float triangle_intersect(float3 O, float3 D, float3 V0, float3 V1, float3 V2);

float3 triangle_normal(float3 V0, float3 V1, float3 V2);
float diffuse(float3 normal, float3 intersection, float3 light);
float ceiling(float value, float multiple);

uchar3 draw_background(float3 ray_d);

// ENTRY POINT

__kernel void render_kernel(const float16 view, const float3 ray_o,
							const int sphere_count, const int light_count, const int polygon_count,
							__global Sphere* spheres_g, __global float3* vertices_g, __global Polygon* polygons_g,
							__local Sphere* spheres, __local float3* vertices, __local Polygon* polygons,
							__write_only image2d_t output)
{
	const int sphere_total = sphere_count + light_count;
	const int vertex_count = 4;
	event_t event_spheres = async_work_group_copy((__local char *)spheres, (__global char *)spheres_g, sphere_total * sizeof(Sphere), 0);
	event_t event_vertices = async_work_group_copy((__local char *)vertices, (__global char *)vertices_g, vertex_count * sizeof(float3), 0);
	event_t event_polygons = async_work_group_copy((__local char *)polygons, (__global char *)polygons_g, polygon_count * sizeof(Polygon), 0);

	const int2 coord = (int2)(get_global_id(0), get_global_id(1));
	const int2 dim = (int2)(get_global_size(0), get_global_size(1));
	
	// create a camera ray
	const float3 uv = (float3)(dim.x, (float)coord.x - (float)dim.x / 2, (float)(dim.y - coord.y) - (float)dim.y / 2);
	const float3 ray_d = fast_normalize((float3)(uv.x * view[0] + uv.y * view[4] + uv.z * view[8],
												 uv.x * view[1] + uv.y * view[5] + uv.z * view[9],
												 uv.x * view[2] + uv.y * view[6] + uv.z * view[10]));
	// check for intersections
	uchar3 color = (uchar3)(0, 0, 0);
	bool color_found = false;
	float min_t = 100; // drop off distance
	float t;

	// spheres
	wait_group_events(1, &event_spheres);
	for (int s = 0; s < sphere_count; s++) {
		t = sphere_intersect(ray_o, ray_d, spheres[s].pos, spheres[s].radius);
		if (0 < t && t < min_t) {
			color_found = true;
			min_t = t;
			float light = 0;
			float3 intersection = mad(t, ray_d, ray_o);
			for (int l = sphere_count; l < sphere_total; l++)
				light += diffuse(intersection - spheres[s].pos, intersection, spheres[l].pos);
			light = clamp(ceiling(light, LIGHT_STEP), AMBIENT, 1.0f);
			color = convert_uchar3(convert_float3(spheres[s].color) * light);
	}	}

	// lights
	for (int l = sphere_count; l < sphere_total; l++) {
		t = sphere_intersect(ray_o, ray_d, spheres[l].pos, spheres[l].radius);
		if (0 < t && t < min_t) {
			color_found = true;
			min_t = t;
			color = spheres[l].color;
	}	}

	// polygons
	wait_group_events(1, &event_vertices);
	wait_group_events(1, &event_polygons);
	for (int p = 0; p < polygon_count; p++) {
		uint3 indices = polygons[p].indices;
		t = triangle_intersect(ray_o, ray_d, vertices[indices.x], vertices[indices.y], vertices[indices.z]);
		if (0 < t && t < min_t) {
			color_found = true;
			min_t = t;
			float light = 0;
			for (int l = sphere_count; l < sphere_total; l++)
				light += diffuse(triangle_normal(vertices[indices.x], vertices[indices.y], vertices[indices.z]),
					mad(t, ray_d, ray_o), spheres[l].pos);
			light = clamp(ceiling(light, LIGHT_STEP), AMBIENT, 1.0f);
			color = convert_uchar3(convert_float3(polygons[p].color) * light);
	}	}

	if (!color_found)
		color = draw_background(ray_d);
	write_imageui(output, coord, (uint4)(color.x, color.y, color.z, 0));
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
	if (t < 0) {
		// the following 2 lines allows you to see inside the sphere
		//t = b - t + b; // b + sqrt(discriminant)
		//if (t < 0)
		return -1;
	}
	return t;
}

float triangle_intersect(float3 O, float3 D, float3 V0, float3 V1, float3 V2)
{
	// Möller-Trumbore algorithm. we use Cramer's rule to find [t,u,v] 
	// https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/moller-trumbore-ray-triangle-intersection
	// returns -1 for no intersection, otherwise returns t (where intersection = O + tD)
	
	float3 E1 = V1 - V0;
	float3 E2 = V2 - V0;
	float3 P = cross(D, E2);
	float det0 = dot(P, E1);

	float inv_det0 = 1/det0;
	float3 T = O - V0;

	float u = dot(T, P) * inv_det0;
	if (u < 0 || 1 < u) return -1;

	float3 Q = cross(T, E1);
	float v = dot(Q, D) * inv_det0;

	return v < 0 || 1 < u + v ? -1 : dot(Q, E2) * inv_det0;
}

float3 triangle_normal(float3 V0, float3 V1, float3 V2)
{
	// assume clockwise vertices (0, 1, 2)
	return cross(V1 - V0, V2 - V0);
}

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
