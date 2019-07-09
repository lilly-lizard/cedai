
// RAY GEN

__kernel void entry(const float16 view,
				   const int width, const int height,
				   __global float3* rays)
{
	const uint x_coord = get_global_id(0);
	const uint y_coord = get_global_id(1);
	
	// create a camera ray
	const float3 uv = (float3)(width, (float)x_coord - (float)width / 2, (float)(height - y_coord) - (float)height / 2);
	rays[x_coord + y_coord * width] = normalize((float3)(uv.x * view[0] + uv.y * view[4] + uv.z * view[8],
														 uv.x * view[1] + uv.y * view[5] + uv.z * view[9],
														 uv.x * view[2] + uv.y * view[6] + uv.z * view[10]));
}