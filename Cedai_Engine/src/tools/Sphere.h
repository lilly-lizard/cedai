#pragma once

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL/cl.hpp>

namespace cd {

	// the total size of a struct must be a multiple of float4 (cl_float3 is the size of float4)
	struct Sphere {
		cl_float radius;
		cl_float dummy1;
		cl_float dummy2;
		cl_float dummy3;
		cl_float3 position;
		cl_uchar3 color;
	};

}