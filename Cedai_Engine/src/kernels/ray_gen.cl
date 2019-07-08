
// RAY GEN

__kernel void ray_gen(const float16 view,
					  const int width, const int height,
					  __global float3* rays)
{
	const int work_item_id = get_global_id(0);
	int x_coord = work_item_id % width;
	int y_coord = height - work_item_id / width;
	
	// create a camera ray
	const float3 uv = (float3)(width, (float)x_coord - (float)width / 2, (float)y_coord - (float)height / 2);
	rays[work_item_id] = normalize((float3)(uv.x * view[0] + uv.y * view[4] + uv.z * view[8],
											uv.x * view[1] + uv.y * view[5] + uv.z * view[9],
											uv.x * view[2] + uv.y * view[6] + uv.z * view[10]));
}