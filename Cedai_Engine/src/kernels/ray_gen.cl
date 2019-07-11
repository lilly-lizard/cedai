#pragma OPENCL EXTENSION cl_khr_gl_sharing : enable

// RAY GEN

__kernel void ray_gen(const float16 view,
					  __write_only image2d_t rays)
{
	const int2 coord = (int2)(get_global_id(0), get_global_id(1));
	const int2 dim = (int2)(get_global_size(0), get_global_size(1));

	// create a camera ray
	const float3 uv = (float3)(dim.x, (float)coord.x - (float)dim.x / 2, (float)(dim.y - coord.y) - (float)dim.y / 2);
	const float4 ray = normalize((float4)(uv.x * view[0] + uv.y * view[4] + uv.z * view[8],
										  uv.x * view[1] + uv.y * view[5] + uv.z * view[9],
										  uv.x * view[2] + uv.y * view[6] + uv.z * view[10],
										  0));
	write_imagef(rays, coord, ray);
}