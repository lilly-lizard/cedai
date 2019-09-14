#pragma once

	/* SETTINGS */

#define WINDOW_TITLE "Cedai"
#define INIT_SCREEN_WIDTH 960
#define INIT_SCREEN_HEIGHT 640

#define PRINT_FPS
//#define HALF_RESOLUTION
//#define RESIZABLE


	/* CONSTANTS */

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#define CL_TARGET_OPENCL_VERSION 120

#define CD_PI 3.14159f

#define BONES_GL 16 /* also defined in primitive.vert */
#define MAX_BONES 50 /* also defined in AnimatedModel.h in the model converter */