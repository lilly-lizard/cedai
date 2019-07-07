
typedef struct Sphere
{
	float radius;
	float3 pos;
	float3 color;
} Sphere;

float3 draw_background(float3 ray_d)
{
	float offset = 0.3;
	float mult = 0.4;
	return fabs(ray_d) * mult + offset;
}

__kernel void render_kernel(__constant Sphere* spheres, const int sphere_count,
							const float16 view,
							const int width, const int height,
							__global float3* output) /* rgb [0 - 1] */
{
	const int work_item_id = get_global_id(0);
	int x_coord = work_item_id % width;
	int y_coord = work_item_id / width;
	
	/* create a camera ray */
	const float3 ray_o = (float3)(view[12], view[13], view[14]);
	const float3 uv = (float3)(width, (float)x_coord - (float)width / 2, (float)y_coord - (float)height / 2);
	const float3 ray_d = (float3)(	uv.x * view[0] + uv.y * view[4] + uv.z * view[8],
									uv.x * view[1] + uv.y * view[5] + uv.z * view[9],
									uv.x * view[2] + uv.y * view[6] + uv.z * view[10] );

	/*  */

	//output[work_item_id] = (float3)(1.0, (float)x_coord / width, (float)y_coord / height);
	output[work_item_id] = draw_background(ray_d);
}