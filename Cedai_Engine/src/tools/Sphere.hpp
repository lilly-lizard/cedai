#pragma once

#include <CL/cl.hpp>

namespace cd {

	// the total size of a struct must be a multiple of float4 (cl_float3 is the size of float4)
	struct Sphere {
		cl_float radius;
		cl_float padding1; // not used
		cl_float padding2; // not used
		cl_float padding3; // not used
		cl_float3 position;
		cl_uchar4 color;
	};

}