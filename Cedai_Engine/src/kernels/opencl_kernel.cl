/* OpenCL ray tracing tutorial by Sam Lapere, 2016
http://raytracey.blogspot.com */

typedef struct Ray
{
	float3 origin;
	float3 dir;
} Ray;

typedef struct Sphere
{
	float radius;
	float3 pos;
	float3 color;
	float3 emission;
} Sphere;

bool intersect_sphere(const Sphere* sphere, const Ray* ray, float* t)
{
	float3 rayToCenter = sphere->pos - ray->origin;

	/* calculate coefficients a, b, c from quadratic equation */

	/* float a = dot(ray->dir, ray->dir); // ray direction is normalised, dotproduct simplifies to 1 */ 
	float b = dot(rayToCenter, ray->dir);
	float c = dot(rayToCenter, rayToCenter) - sphere->radius*sphere->radius;
	float disc = b * b - c; /* discriminant of quadratic formula */

	/* solve for t (distance to hitpoint along ray) */

	if (disc < 0.0f) return false;
	else *t = b - sqrt(disc);

	if (*t < 0.0f) {
		*t = b + sqrt(disc);
		if (*t < 0.0f) return false; 
	}

	else return true;
}

Ray createCamRay(const int x_coord, const int y_coord, const int width, const int height){

	float fx = (float)x_coord / (float)width;  /* convert int in range [0 - width] to float in range [0-1] */
	float fy = (float)y_coord / (float)height; /* convert int in range [0 - height] to float in range [0-1] */

	/* calculate aspect ratio */
	float aspect_ratio = (float)(width) / (float)(height);
	float fx2 = (fx - 0.5f) * aspect_ratio;
	float fy2 = fy - 0.5f;

	/* determine position of pixel on screen */
	float3 pixel_pos = (float3)(fx2, -fy2, 0.0f);

	/* create camera ray*/
	Ray ray;
	ray.origin = (float3)(0.0f, 0.0f, 40.0f); /* fixed camera position */
	ray.dir = normalize(pixel_pos - ray.origin); /* ray direction is vector from camera to pixel */

	return ray;
}

__kernel void render_kernel(__constant Sphere* spheres, const int sphere_count, const int width, const int height, __global float3* output)
{
	const int work_item_id = get_global_id(0);		/* the unique global id of the work item for the current pixel */
	int x_coord = work_item_id % width;				/* x-coordinate of the pixel */
	int y_coord = work_item_id / width;				/* y-coordinate of the pixel */

	float fx = (float)x_coord / (float)width;  /* convert int in range [0 - width] to float in range [0-1] */
	float fy = (float)y_coord / (float)height; /* convert int in range [0 - height] to float in range [0-1] */

	/*create a camera ray */
	Ray camray = createCamRay(x_coord, y_coord, width, height);
	
	float t_smallest = 1e20;
	int sphere_index = 0;
	for (int i = 0; i < sphere_count; i++) {
		/* intersect ray with sphere */
		Sphere sphere = spheres[i];
		float t = 1e20;
		intersect_sphere(&sphere, &camray, &t);
		if (t < t_smallest) {
			sphere_index = i;
			t_smallest = t;
		}
	}

	/* if ray misses sphere, return background colour 
	background colour is a blue-ish gradient dependent on image height */
	if (t_smallest > 1e19) {
		output[work_item_id] = (float3)(fy * 0.1f, fy * 0.3f, 0.3f);
		return;
	}

	/* for more interesting lighting: compute normal 
	and cosine of angle between normal and ray direction */
	Sphere sphere = spheres[sphere_index];
	float3 hitpoint = camray.origin + camray.dir * t_smallest;
	float3 normal = normalize(hitpoint - sphere.pos);
	float cosine_factor = dot(normal, camray.dir) * -1.0f;

	output[work_item_id] = sphere.color * cosine_factor; /* with cosine weighted colour */
}