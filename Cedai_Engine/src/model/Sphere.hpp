#pragma once

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL/cl.hpp>

namespace cd {
	// the total size of a struct must be a multiple of float4 (cl_float3 is the size of float4)
	class Sphere {
	public:
		Sphere(cl_float radius, cl_float3 position, cl_uchar4 color)
			: radius(radius), position(position), color(color) {}

	private:
		cl_float radius;
		cl_float padding1 = 0; // not used
		cl_float padding2 = 0; // not used
		cl_float padding3 = 0; // not used
		cl_float3 position;
		cl_uchar4 color;
	};
}
