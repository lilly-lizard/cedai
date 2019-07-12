/* https://stackoverflow.com/questions/4911400/shader-optimization-is-a-ternary-operator-equivalent-to-branching */
// TODO use half instead of float (half_rsqrt, fast_normalize etc)
// TODO fast_normalize

#pragma OPENCL EXTENSION cl_khr_gl_sharing : enable
#pragma OPENCL EXTENSION cl_khr_fp16 : enable

typedef struct Sphere
{
	float radius;
	float3 pos;
	uchar3 color;
} Sphere;

// DECLARATIONS AND CONSTANTS

#define AMBIENT 0.4h
#define LIGHT_STEP 0.2h

const sampler_t sampler = CLK_FILTER_NEAREST | CLK_ADDRESS_CLAMP | CLK_NORMALIZED_COORDS_FALSE;

half sphere_intersect(half3 ray_o, half3 ray_d, half3 center, half radius);
half diffuse(half3 normal, half3 intersection, half3 light);
half ceiling(half value, half multiple);
uchar3 draw_background(half3 ray_d);

// ENTRY POINT

__kernel void render_kernel(const float16 viewf, const float3 ray_of,
							__global Sphere* spheres,
							const int sphere_count, const int light_count,
							__write_only image2d_t output)
{
	const int2 coord = (int2)(get_global_id(0), get_global_id(1));
	const int2 dim = (int2)(get_global_size(0), get_global_size(1));
	
	// create a camera ray
	const half3 ray_o = convert_half3(ray_of);
	const half16 view = convert_half16(viewf);
	const half3 uv = (half3)(dim.x, (half)coord.x - (half)dim.x / 2, (half)(dim.y - coord.y) - (half)dim.y / 2);
	const half3 ray_d = normalize((half3)(uv.x * view[0] + uv.y * view[4] + uv.z * view[8],
										  uv.x * view[1] + uv.y * view[5] + uv.z * view[9],
										  uv.x * view[2] + uv.y * view[6] + uv.z * view[10]));
	// check for intersections
	uchar3 color = (uchar3)(0, 0, 0);
	bool color_found = false;
	half min_t = 100; // drop off distance
	half t;

	// spheres
	for (int s = 0; s < sphere_count; s++) {
		half3 pos = convert_half3(spheres[s].pos);
		t = sphere_intersect(ray_o, ray_d, pos, (half)spheres[s].radius);
		if (0 < t && t < min_t) {
			color_found = true;
			min_t = t;
			// calculate light
			half light = 0;
			half3 intersection = ray_o + t * ray_d;
			for (int l = sphere_count; l < sphere_count + light_count; l++)
				light += diffuse(intersection - pos, intersection, convert_half3(spheres[l].pos));
			light = clamp(ceiling(light, LIGHT_STEP), AMBIENT, 1.0h);
			color = convert_uchar3(convert_half3(spheres[s].color) * light);
	}	}

	// lights
	for (int l = sphere_count; l < sphere_count + light_count; l++) {
		t = sphere_intersect(ray_o, ray_d, convert_half3(spheres[l].pos), spheres[l].radius);
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

half sphere_intersect(half3 ray_o, half3 ray_d, half3 center, half radius)
{ 
	// a = P1 . P1 = 1 (assuming ray_d is normalized)
	half3 d = center - ray_o;
	half b = dot(d, ray_d);
	half c = dot(d, d) - radius * radius;

	half discriminant = b * b - c;
	if (discriminant < 0) return -1;

	half t = b - half_sqrt(discriminant);
	if (t < 0) {
		// the following 2 lines allows you to see inside the sphere
		//t = b - t + b; // b + sqrt(discriminant)
		//if (t < 0)
		return -1;
	}
	return t;
}

half diffuse(half3 normal, half3 intersection, half3 light)
{
	return clamp(dot(normalize(normal), normalize(light - intersection)), 0.0h, 1.0h);
}

// rounds up to the nearest multiple of multiple
half ceiling(half value, half multiple)
{
	return ceil(value/multiple) * multiple;
}

uchar3 draw_background(half3 ray_d)
{
	half offset = 0.3f;
	half mult = 0.4f;
	half3 color = fabs(ray_d) * mult + offset;
	return convert_uchar3(color * 255);
}
