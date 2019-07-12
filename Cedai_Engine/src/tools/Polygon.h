#pragma once

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL/cl.hpp>

namespace cd {

	struct Polygon {
		cl_uint3 indices;
		cl_uchar3 color;
	};

}