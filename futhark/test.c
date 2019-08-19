#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#undef NDEBUG
#include <assert.h>
// Start of panic.h.

#include <stdarg.h>

static const char *fut_progname;

static void panic(int eval, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
        fprintf(stderr, "%s: ", fut_progname);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
        exit(eval);
}

/* For generating arbitrary-sized error messages.  It is the callers
   responsibility to free the buffer at some point. */
static char* msgprintf(const char *s, ...) {
  va_list vl;
  va_start(vl, s);
  size_t needed = 1 + vsnprintf(NULL, 0, s, vl);
  char *buffer = (char*) malloc(needed);
  va_start(vl, s); /* Must re-init. */
  vsnprintf(buffer, needed, s, vl);
  return buffer;
}

// End of panic.h.

// Start of timing.h.

// The function get_wall_time() returns the wall time in microseconds
// (with an unspecified offset).

#ifdef _WIN32

#include <windows.h>

static int64_t get_wall_time(void) {
  LARGE_INTEGER time,freq;
  assert(QueryPerformanceFrequency(&freq));
  assert(QueryPerformanceCounter(&time));
  return ((double)time.QuadPart / freq.QuadPart) * 1000000;
}

#else
/* Assuming POSIX */

#include <time.h>
#include <sys/time.h>

static int64_t get_wall_time(void) {
  struct timeval time;
  assert(gettimeofday(&time,NULL) == 0);
  return time.tv_sec * 1000000 + time.tv_usec;
}

#endif

// End of timing.h.

#ifdef _MSC_VER
#define inline __inline
#endif
#include <string.h>
#include <inttypes.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
// Start of lock.h.

/* A very simple cross-platform implementation of locks.  Uses
   pthreads on Unix and some Windows thing there.  Futhark's
   host-level code is not multithreaded, but user code may be, so we
   need some mechanism for ensuring atomic access to API functions.
   This is that mechanism.  It is not exposed to user code at all, so
   we do not have to worry about name collisions. */

#ifdef _WIN32

typedef HANDLE lock_t;

static lock_t create_lock(lock_t *lock) {
  *lock = CreateMutex(NULL,  /* Default security attributes. */
                      FALSE, /* Initially unlocked. */
                      NULL); /* Unnamed. */
}

static void lock_lock(lock_t *lock) {
  assert(WaitForSingleObject(*lock, INFINITE) == WAIT_OBJECT_0);
}

static void lock_unlock(lock_t *lock) {
  assert(ReleaseMutex(*lock));
}

static void free_lock(lock_t *lock) {
  CloseHandle(*lock);
}

#else
/* Assuming POSIX */

#include <pthread.h>

typedef pthread_mutex_t lock_t;

static void create_lock(lock_t *lock) {
  int r = pthread_mutex_init(lock, NULL);
  assert(r == 0);
}

static void lock_lock(lock_t *lock) {
  int r = pthread_mutex_lock(lock);
  assert(r == 0);
}

static void lock_unlock(lock_t *lock) {
  int r = pthread_mutex_unlock(lock);
  assert(r == 0);
}

static void free_lock(lock_t *lock) {
  /* Nothing to do for pthreads. */
  (void)lock;
}

#endif

// End of lock.h.

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#define CL_SILENCE_DEPRECATION // For macOS.
#ifdef __APPLE__
  #include <OpenCL/cl.h>
#else
  #include <CL/cl.h>
#endif
typedef cl_mem fl_mem_t;
// Start of free_list.h.

/* An entry in the free list.  May be invalid, to avoid having to
   deallocate entries as soon as they are removed.  There is also a
   tag, to help with memory reuse. */
struct free_list_entry {
  size_t size;
  fl_mem_t mem;
  const char *tag;
  unsigned char valid;
};

struct free_list {
  struct free_list_entry *entries;        // Pointer to entries.
  int capacity;                           // Number of entries.
  int used;                               // Number of valid entries.
};

void free_list_init(struct free_list *l) {
  l->capacity = 30; // Picked arbitrarily.
  l->used = 0;
  l->entries = (struct free_list_entry*) malloc(sizeof(struct free_list_entry) * l->capacity);
  for (int i = 0; i < l->capacity; i++) {
    l->entries[i].valid = 0;
  }
}

/* Remove invalid entries from the free list. */
void free_list_pack(struct free_list *l) {
  int p = 0;
  for (int i = 0; i < l->capacity; i++) {
    if (l->entries[i].valid) {
      l->entries[p] = l->entries[i];
      p++;
    }
  }
  // Now p == l->used.
  l->entries = realloc(l->entries, l->used * sizeof(struct free_list_entry));
  l->capacity = l->used;
}

void free_list_destroy(struct free_list *l) {
  assert(l->used == 0);
  free(l->entries);
}

int free_list_find_invalid(struct free_list *l) {
  int i;
  for (i = 0; i < l->capacity; i++) {
    if (!l->entries[i].valid) {
      break;
    }
  }
  return i;
}

void free_list_insert(struct free_list *l, size_t size, fl_mem_t mem, const char *tag) {
  int i = free_list_find_invalid(l);

  if (i == l->capacity) {
    // List is full; so we have to grow it.
    int new_capacity = l->capacity * 2 * sizeof(struct free_list_entry);
    l->entries = realloc(l->entries, new_capacity);
    for (int j = 0; j < l->capacity; j++) {
      l->entries[j+l->capacity].valid = 0;
    }
    l->capacity *= 2;
  }

  // Now 'i' points to the first invalid entry.
  l->entries[i].valid = 1;
  l->entries[i].size = size;
  l->entries[i].mem = mem;
  l->entries[i].tag = tag;

  l->used++;
}

/* Find and remove a memory block of at least the desired size and
   tag.  Returns 0 on success.  */
int free_list_find(struct free_list *l, const char *tag, size_t *size_out, fl_mem_t *mem_out) {
  int i;
  for (i = 0; i < l->capacity; i++) {
    if (l->entries[i].valid && l->entries[i].tag == tag) {
      l->entries[i].valid = 0;
      *size_out = l->entries[i].size;
      *mem_out = l->entries[i].mem;
      l->used--;
      return 0;
    }
  }

  return 1;
}

/* Remove the first block in the free list.  Returns 0 if a block was
   removed, and nonzero if the free list was already empty. */
int free_list_first(struct free_list *l, fl_mem_t *mem_out) {
  for (int i = 0; i < l->capacity; i++) {
    if (l->entries[i].valid) {
      l->entries[i].valid = 0;
      *mem_out = l->entries[i].mem;
      l->used--;
      return 0;
    }
  }

  return 1;
}

// End of free_list.h.

// Start of opencl.h.

#define OPENCL_SUCCEED_FATAL(e) opencl_succeed_fatal(e, #e, __FILE__, __LINE__)
#define OPENCL_SUCCEED_NONFATAL(e) opencl_succeed_nonfatal(e, #e, __FILE__, __LINE__)
// Take care not to override an existing error.
#define OPENCL_SUCCEED_OR_RETURN(e) {             \
    char *error = OPENCL_SUCCEED_NONFATAL(e);     \
    if (error) {                                  \
      if (!ctx->error) {                          \
        ctx->error = error;                       \
        return bad;                               \
      } else {                                    \
        free(error);                              \
      }                                           \
    }                                             \
  }

// OPENCL_SUCCEED_OR_RETURN returns the value of the variable 'bad' in
// scope.  By default, it will be this one.  Create a local variable
// of some other type if needed.  This is a bit of a hack, but it
// saves effort in the code generator.
static const int bad = 1;

struct opencl_config {
  int debugging;
  int logging;
  int preferred_device_num;
  const char *preferred_platform;
  const char *preferred_device;
  int ignore_blacklist;

  const char* dump_program_to;
  const char* load_program_from;
  const char* dump_binary_to;
  const char* load_binary_from;

  size_t default_group_size;
  size_t default_num_groups;
  size_t default_tile_size;
  size_t default_threshold;

  int default_group_size_changed;
  int default_tile_size_changed;

  int num_sizes;
  const char **size_names;
  const char **size_vars;
  size_t *size_values;
  const char **size_classes;
};

void opencl_config_init(struct opencl_config *cfg,
                        int num_sizes,
                        const char *size_names[],
                        const char *size_vars[],
                        size_t *size_values,
                        const char *size_classes[]) {
  cfg->debugging = 0;
  cfg->logging = 0;
  cfg->preferred_device_num = 0;
  cfg->preferred_platform = "";
  cfg->preferred_device = "";
  cfg->ignore_blacklist = 0;
  cfg->dump_program_to = NULL;
  cfg->load_program_from = NULL;
  cfg->dump_binary_to = NULL;
  cfg->load_binary_from = NULL;

  // The following are dummy sizes that mean the concrete defaults
  // will be set during initialisation via hardware-inspection-based
  // heuristics.
  cfg->default_group_size = 0;
  cfg->default_num_groups = 0;
  cfg->default_tile_size = 0;
  cfg->default_threshold = 0;

  cfg->default_group_size_changed = 0;
  cfg->default_tile_size_changed = 0;

  cfg->num_sizes = num_sizes;
  cfg->size_names = size_names;
  cfg->size_vars = size_vars;
  cfg->size_values = size_values;
  cfg->size_classes = size_classes;
}

struct opencl_context {
  cl_device_id device;
  cl_context ctx;
  cl_command_queue queue;

  struct opencl_config cfg;

  struct free_list free_list;

  size_t max_group_size;
  size_t max_num_groups;
  size_t max_tile_size;
  size_t max_threshold;
  size_t max_local_memory;

  size_t lockstep_width;
};

struct opencl_device_option {
  cl_platform_id platform;
  cl_device_id device;
  cl_device_type device_type;
  char *platform_name;
  char *device_name;
};

/* This function must be defined by the user.  It is invoked by
   setup_opencl() after the platform and device has been found, but
   before the program is loaded.  Its intended use is to tune
   constants based on the selected platform and device. */
static void post_opencl_setup(struct opencl_context*, struct opencl_device_option*);

static char *strclone(const char *str) {
  size_t size = strlen(str) + 1;
  char *copy = (char*) malloc(size);
  if (copy == NULL) {
    return NULL;
  }

  memcpy(copy, str, size);
  return copy;
}

// Read a file into a NUL-terminated string; returns NULL on error.
static char* slurp_file(const char *filename, size_t *size) {
  char *s;
  FILE *f = fopen(filename, "rb"); // To avoid Windows messing with linebreaks.
  if (f == NULL) return NULL;
  fseek(f, 0, SEEK_END);
  size_t src_size = ftell(f);
  fseek(f, 0, SEEK_SET);
  s = (char*) malloc(src_size + 1);
  if (fread(s, 1, src_size, f) != src_size) {
    free(s);
    s = NULL;
  } else {
    s[src_size] = '\0';
  }
  fclose(f);

  if (size) {
    *size = src_size;
  }

  return s;
}

static const char* opencl_error_string(cl_int err)
{
    switch (err) {
        case CL_SUCCESS:                            return "Success!";
        case CL_DEVICE_NOT_FOUND:                   return "Device not found.";
        case CL_DEVICE_NOT_AVAILABLE:               return "Device not available";
        case CL_COMPILER_NOT_AVAILABLE:             return "Compiler not available";
        case CL_MEM_OBJECT_ALLOCATION_FAILURE:      return "Memory object allocation failure";
        case CL_OUT_OF_RESOURCES:                   return "Out of resources";
        case CL_OUT_OF_HOST_MEMORY:                 return "Out of host memory";
        case CL_PROFILING_INFO_NOT_AVAILABLE:       return "Profiling information not available";
        case CL_MEM_COPY_OVERLAP:                   return "Memory copy overlap";
        case CL_IMAGE_FORMAT_MISMATCH:              return "Image format mismatch";
        case CL_IMAGE_FORMAT_NOT_SUPPORTED:         return "Image format not supported";
        case CL_BUILD_PROGRAM_FAILURE:              return "Program build failure";
        case CL_MAP_FAILURE:                        return "Map failure";
        case CL_INVALID_VALUE:                      return "Invalid value";
        case CL_INVALID_DEVICE_TYPE:                return "Invalid device type";
        case CL_INVALID_PLATFORM:                   return "Invalid platform";
        case CL_INVALID_DEVICE:                     return "Invalid device";
        case CL_INVALID_CONTEXT:                    return "Invalid context";
        case CL_INVALID_QUEUE_PROPERTIES:           return "Invalid queue properties";
        case CL_INVALID_COMMAND_QUEUE:              return "Invalid command queue";
        case CL_INVALID_HOST_PTR:                   return "Invalid host pointer";
        case CL_INVALID_MEM_OBJECT:                 return "Invalid memory object";
        case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:    return "Invalid image format descriptor";
        case CL_INVALID_IMAGE_SIZE:                 return "Invalid image size";
        case CL_INVALID_SAMPLER:                    return "Invalid sampler";
        case CL_INVALID_BINARY:                     return "Invalid binary";
        case CL_INVALID_BUILD_OPTIONS:              return "Invalid build options";
        case CL_INVALID_PROGRAM:                    return "Invalid program";
        case CL_INVALID_PROGRAM_EXECUTABLE:         return "Invalid program executable";
        case CL_INVALID_KERNEL_NAME:                return "Invalid kernel name";
        case CL_INVALID_KERNEL_DEFINITION:          return "Invalid kernel definition";
        case CL_INVALID_KERNEL:                     return "Invalid kernel";
        case CL_INVALID_ARG_INDEX:                  return "Invalid argument index";
        case CL_INVALID_ARG_VALUE:                  return "Invalid argument value";
        case CL_INVALID_ARG_SIZE:                   return "Invalid argument size";
        case CL_INVALID_KERNEL_ARGS:                return "Invalid kernel arguments";
        case CL_INVALID_WORK_DIMENSION:             return "Invalid work dimension";
        case CL_INVALID_WORK_GROUP_SIZE:            return "Invalid work group size";
        case CL_INVALID_WORK_ITEM_SIZE:             return "Invalid work item size";
        case CL_INVALID_GLOBAL_OFFSET:              return "Invalid global offset";
        case CL_INVALID_EVENT_WAIT_LIST:            return "Invalid event wait list";
        case CL_INVALID_EVENT:                      return "Invalid event";
        case CL_INVALID_OPERATION:                  return "Invalid operation";
        case CL_INVALID_GL_OBJECT:                  return "Invalid OpenGL object";
        case CL_INVALID_BUFFER_SIZE:                return "Invalid buffer size";
        case CL_INVALID_MIP_LEVEL:                  return "Invalid mip-map level";
        default:                                    return "Unknown";
    }
}

static void opencl_succeed_fatal(unsigned int ret,
                                 const char *call,
                                 const char *file,
                                 int line) {
  if (ret != CL_SUCCESS) {
    panic(-1, "%s:%d: OpenCL call\n  %s\nfailed with error code %d (%s)\n",
          file, line, call, ret, opencl_error_string(ret));
  }
}

static char* opencl_succeed_nonfatal(unsigned int ret,
                                     const char *call,
                                     const char *file,
                                     int line) {
  if (ret != CL_SUCCESS) {
    return msgprintf("%s:%d: OpenCL call\n  %s\nfailed with error code %d (%s)\n",
                     file, line, call, ret, opencl_error_string(ret));
  } else {
    return NULL;
  }
}

void set_preferred_platform(struct opencl_config *cfg, const char *s) {
  cfg->preferred_platform = s;
  cfg->ignore_blacklist = 1;
}

void set_preferred_device(struct opencl_config *cfg, const char *s) {
  int x = 0;
  if (*s == '#') {
    s++;
    while (isdigit(*s)) {
      x = x * 10 + (*s++)-'0';
    }
    // Skip trailing spaces.
    while (isspace(*s)) {
      s++;
    }
  }
  cfg->preferred_device = s;
  cfg->preferred_device_num = x;
  cfg->ignore_blacklist = 1;
}

static char* opencl_platform_info(cl_platform_id platform,
                                  cl_platform_info param) {
  size_t req_bytes;
  char *info;

  OPENCL_SUCCEED_FATAL(clGetPlatformInfo(platform, param, 0, NULL, &req_bytes));

  info = (char*) malloc(req_bytes);

  OPENCL_SUCCEED_FATAL(clGetPlatformInfo(platform, param, req_bytes, info, NULL));

  return info;
}

static char* opencl_device_info(cl_device_id device,
                                cl_device_info param) {
  size_t req_bytes;
  char *info;

  OPENCL_SUCCEED_FATAL(clGetDeviceInfo(device, param, 0, NULL, &req_bytes));

  info = (char*) malloc(req_bytes);

  OPENCL_SUCCEED_FATAL(clGetDeviceInfo(device, param, req_bytes, info, NULL));

  return info;
}

static void opencl_all_device_options(struct opencl_device_option **devices_out,
                                      size_t *num_devices_out) {
  size_t num_devices = 0, num_devices_added = 0;

  cl_platform_id *all_platforms;
  cl_uint *platform_num_devices;

  cl_uint num_platforms;

  // Find the number of platforms.
  OPENCL_SUCCEED_FATAL(clGetPlatformIDs(0, NULL, &num_platforms));

  // Make room for them.
  all_platforms = calloc(num_platforms, sizeof(cl_platform_id));
  platform_num_devices = calloc(num_platforms, sizeof(cl_uint));

  // Fetch all the platforms.
  OPENCL_SUCCEED_FATAL(clGetPlatformIDs(num_platforms, all_platforms, NULL));

  // Count the number of devices for each platform, as well as the
  // total number of devices.
  for (cl_uint i = 0; i < num_platforms; i++) {
    if (clGetDeviceIDs(all_platforms[i], CL_DEVICE_TYPE_ALL,
                       0, NULL, &platform_num_devices[i]) == CL_SUCCESS) {
      num_devices += platform_num_devices[i];
    } else {
      platform_num_devices[i] = 0;
    }
  }

  // Make room for all the device options.
  struct opencl_device_option *devices =
    calloc(num_devices, sizeof(struct opencl_device_option));

  // Loop through the platforms, getting information about their devices.
  for (cl_uint i = 0; i < num_platforms; i++) {
    cl_platform_id platform = all_platforms[i];
    cl_uint num_platform_devices = platform_num_devices[i];

    if (num_platform_devices == 0) {
      continue;
    }

    char *platform_name = opencl_platform_info(platform, CL_PLATFORM_NAME);
    cl_device_id *platform_devices =
      calloc(num_platform_devices, sizeof(cl_device_id));

    // Fetch all the devices.
    OPENCL_SUCCEED_FATAL(clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL,
                                  num_platform_devices, platform_devices, NULL));

    // Loop through the devices, adding them to the devices array.
    for (cl_uint i = 0; i < num_platform_devices; i++) {
      char *device_name = opencl_device_info(platform_devices[i], CL_DEVICE_NAME);
      devices[num_devices_added].platform = platform;
      devices[num_devices_added].device = platform_devices[i];
      OPENCL_SUCCEED_FATAL(clGetDeviceInfo(platform_devices[i], CL_DEVICE_TYPE,
                                     sizeof(cl_device_type),
                                     &devices[num_devices_added].device_type,
                                     NULL));
      // We don't want the structs to share memory, so copy the platform name.
      // Each device name is already unique.
      devices[num_devices_added].platform_name = strclone(platform_name);
      devices[num_devices_added].device_name = device_name;
      num_devices_added++;
    }
    free(platform_devices);
    free(platform_name);
  }
  free(all_platforms);
  free(platform_num_devices);

  *devices_out = devices;
  *num_devices_out = num_devices;
}

// Returns 0 on success.
static int select_device_interactively(struct opencl_config *cfg) {
  struct opencl_device_option *devices;
  size_t num_devices;
  int ret = 1;

  opencl_all_device_options(&devices, &num_devices);

  printf("Choose OpenCL device:\n");
  const char *cur_platform = "";
  for (size_t i = 0; i < num_devices; i++) {
    struct opencl_device_option device = devices[i];
    if (strcmp(cur_platform, device.platform_name) != 0) {
      printf("Platform: %s\n", device.platform_name);
      cur_platform = device.platform_name;
    }
    printf("[%d] %s\n", (int)i, device.device_name);
  }

  int selection;
  printf("Choice: ");
  if (scanf("%d", &selection) == 1) {
    ret = 0;
    cfg->preferred_platform = "";
    cfg->preferred_device = "";
    cfg->preferred_device_num = selection;
    cfg->ignore_blacklist = 1;
  }

  // Free all the platform and device names.
  for (size_t j = 0; j < num_devices; j++) {
    free(devices[j].platform_name);
    free(devices[j].device_name);
  }
  free(devices);

  return ret;
}

static int is_blacklisted(const char *platform_name, const char *device_name,
                          const struct opencl_config *cfg) {
  if (strcmp(cfg->preferred_platform, "") != 0 ||
      strcmp(cfg->preferred_device, "") != 0) {
    return 0;
  } else if (strstr(platform_name, "Apple") != NULL &&
             strstr(device_name, "Intel(R) Core(TM)") != NULL) {
    return 1;
  } else {
    return 0;
  }
}

static struct opencl_device_option get_preferred_device(const struct opencl_config *cfg) {
  struct opencl_device_option *devices;
  size_t num_devices;

  opencl_all_device_options(&devices, &num_devices);

  int num_device_matches = 0;

  for (size_t i = 0; i < num_devices; i++) {
    struct opencl_device_option device = devices[i];
    if (strstr(device.platform_name, cfg->preferred_platform) != NULL &&
        strstr(device.device_name, cfg->preferred_device) != NULL &&
        (cfg->ignore_blacklist ||
         !is_blacklisted(device.platform_name, device.device_name, cfg)) &&
        num_device_matches++ == cfg->preferred_device_num) {
      // Free all the platform and device names, except the ones we have chosen.
      for (size_t j = 0; j < num_devices; j++) {
        if (j != i) {
          free(devices[j].platform_name);
          free(devices[j].device_name);
        }
      }
      free(devices);
      return device;
    }
  }

  panic(1, "Could not find acceptable OpenCL device.\n");
  exit(1); // Never reached
}

static void describe_device_option(struct opencl_device_option device) {
  fprintf(stderr, "Using platform: %s\n", device.platform_name);
  fprintf(stderr, "Using device: %s\n", device.device_name);
}

static cl_build_status build_opencl_program(cl_program program, cl_device_id device, const char* options) {
  cl_int clBuildProgram_error = clBuildProgram(program, 1, &device, options, NULL, NULL);

  // Avoid termination due to CL_BUILD_PROGRAM_FAILURE
  if (clBuildProgram_error != CL_SUCCESS &&
      clBuildProgram_error != CL_BUILD_PROGRAM_FAILURE) {
    OPENCL_SUCCEED_FATAL(clBuildProgram_error);
  }

  cl_build_status build_status;
  OPENCL_SUCCEED_FATAL(clGetProgramBuildInfo(program,
                                             device,
                                             CL_PROGRAM_BUILD_STATUS,
                                             sizeof(cl_build_status),
                                             &build_status,
                                             NULL));

  if (build_status != CL_SUCCESS) {
    char *build_log;
    size_t ret_val_size;
    OPENCL_SUCCEED_FATAL(clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size));

    build_log = (char*) malloc(ret_val_size+1);
    OPENCL_SUCCEED_FATAL(clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, ret_val_size, build_log, NULL));

    // The spec technically does not say whether the build log is zero-terminated, so let's be careful.
    build_log[ret_val_size] = '\0';

    fprintf(stderr, "Build log:\n%s\n", build_log);

    free(build_log);
  }

  return build_status;
}

/* Fields in a bitmask indicating which types we must be sure are
   available. */
enum opencl_required_type { OPENCL_F64 = 1 };

// We take as input several strings representing the program, because
// C does not guarantee that the compiler supports particularly large
// literals.  Notably, Visual C has a limit of 2048 characters.  The
// array must be NULL-terminated.
static cl_program setup_opencl_with_command_queue(struct opencl_context *ctx,
                                                  cl_command_queue queue,
                                                  const char *srcs[],
                                                  int required_types,
                                                  const char *extra_build_opts[]) {
  int error;

  ctx->queue = queue;

  OPENCL_SUCCEED_FATAL(clGetCommandQueueInfo(ctx->queue, CL_QUEUE_CONTEXT, sizeof(cl_context), &ctx->ctx, NULL));

  // Fill out the device info.  This is redundant work if we are
  // called from setup_opencl() (which is the common case), but I
  // doubt it matters much.
  struct opencl_device_option device_option;
  OPENCL_SUCCEED_FATAL(clGetCommandQueueInfo(ctx->queue, CL_QUEUE_DEVICE,
                                       sizeof(cl_device_id),
                                       &device_option.device,
                                       NULL));
  OPENCL_SUCCEED_FATAL(clGetDeviceInfo(device_option.device, CL_DEVICE_PLATFORM,
                                 sizeof(cl_platform_id),
                                 &device_option.platform,
                                 NULL));
  OPENCL_SUCCEED_FATAL(clGetDeviceInfo(device_option.device, CL_DEVICE_TYPE,
                                 sizeof(cl_device_type),
                                 &device_option.device_type,
                                 NULL));
  device_option.platform_name = opencl_platform_info(device_option.platform, CL_PLATFORM_NAME);
  device_option.device_name = opencl_device_info(device_option.device, CL_DEVICE_NAME);

  ctx->device = device_option.device;

  if (required_types & OPENCL_F64) {
    cl_uint supported;
    OPENCL_SUCCEED_FATAL(clGetDeviceInfo(device_option.device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE,
                                   sizeof(cl_uint), &supported, NULL));
    if (!supported) {
      panic(1, "Program uses double-precision floats, but this is not supported on the chosen device: %s\n",
            device_option.device_name);
    }
  }

  size_t max_group_size;
  OPENCL_SUCCEED_FATAL(clGetDeviceInfo(device_option.device, CL_DEVICE_MAX_WORK_GROUP_SIZE,
                                 sizeof(size_t), &max_group_size, NULL));

  size_t max_tile_size = sqrt(max_group_size);

  cl_ulong max_local_memory;
  OPENCL_SUCCEED_FATAL(clGetDeviceInfo(device_option.device, CL_DEVICE_LOCAL_MEM_SIZE,
                                       sizeof(size_t), &max_local_memory, NULL));

  // Make sure this function is defined.
  post_opencl_setup(ctx, &device_option);

  if (max_group_size < ctx->cfg.default_group_size) {
    if (ctx->cfg.default_group_size_changed) {
      fprintf(stderr, "Note: Device limits default group size to %zu (down from %zu).\n",
              max_group_size, ctx->cfg.default_group_size);
    }
    ctx->cfg.default_group_size = max_group_size;
  }

  if (max_tile_size < ctx->cfg.default_tile_size) {
    if (ctx->cfg.default_tile_size_changed) {
      fprintf(stderr, "Note: Device limits default tile size to %zu (down from %zu).\n",
              max_tile_size, ctx->cfg.default_tile_size);
    }
    ctx->cfg.default_tile_size = max_tile_size;
  }

  ctx->max_group_size = max_group_size;
  ctx->max_tile_size = max_tile_size; // No limit.
  ctx->max_threshold = ctx->max_num_groups = 0; // No limit.
  ctx->max_local_memory = max_local_memory;

  // Now we go through all the sizes, clamp them to the valid range,
  // or set them to the default.
  for (int i = 0; i < ctx->cfg.num_sizes; i++) {
    const char *size_class = ctx->cfg.size_classes[i];
    size_t *size_value = &ctx->cfg.size_values[i];
    const char* size_name = ctx->cfg.size_names[i];
    size_t max_value, default_value;
    if (strstr(size_class, "group_size") == size_class) {
      max_value = max_group_size;
      default_value = ctx->cfg.default_group_size;
    } else if (strstr(size_class, "num_groups") == size_class) {
      max_value = max_group_size; // Futhark assumes this constraint.
      default_value = ctx->cfg.default_num_groups;
    } else if (strstr(size_class, "tile_size") == size_class) {
      max_value = sqrt(max_group_size);
      default_value = ctx->cfg.default_tile_size;
    } else if (strstr(size_class, "threshold") == size_class) {
      max_value = 0; // No limit.
      default_value = ctx->cfg.default_threshold;
    } else {
      panic(1, "Unknown size class for size '%s': %s\n", size_name, size_class);
    }
    if (*size_value == 0) {
      *size_value = default_value;
    } else if (max_value > 0 && *size_value > max_value) {
      fprintf(stderr, "Note: Device limits %s to %d (down from %d)\n",
              size_name, (int)max_value, (int)*size_value);
      *size_value = max_value;
    }
  }

  if (ctx->lockstep_width == 0) {
    ctx->lockstep_width = 1;
  }

  if (ctx->cfg.logging) {
    fprintf(stderr, "Lockstep width: %d\n", (int)ctx->lockstep_width);
    fprintf(stderr, "Default group size: %d\n", (int)ctx->cfg.default_group_size);
    fprintf(stderr, "Default number of groups: %d\n", (int)ctx->cfg.default_num_groups);
  }

  char *fut_opencl_src = NULL;
  size_t src_size = 0;

  // Maybe we have to read OpenCL source from somewhere else (used for debugging).
  if (ctx->cfg.load_program_from != NULL) {
    fut_opencl_src = slurp_file(ctx->cfg.load_program_from, NULL);
    assert(fut_opencl_src != NULL);
  } else {
    // Build the OpenCL program.  First we have to concatenate all the fragments.
    for (const char **src = srcs; src && *src; src++) {
      src_size += strlen(*src);
    }

    fut_opencl_src = (char*) malloc(src_size + 1);

    size_t n, i;
    for (i = 0, n = 0; srcs && srcs[i]; i++) {
      strncpy(fut_opencl_src+n, srcs[i], src_size-n);
      n += strlen(srcs[i]);
    }
    fut_opencl_src[src_size] = 0;

  }

  cl_program prog;
  error = CL_SUCCESS;
  const char* src_ptr[] = {fut_opencl_src};

  if (ctx->cfg.dump_program_to != NULL) {
    FILE *f = fopen(ctx->cfg.dump_program_to, "w");
    assert(f != NULL);
    fputs(fut_opencl_src, f);
    fclose(f);
  }

  if (ctx->cfg.load_binary_from == NULL) {
    prog = clCreateProgramWithSource(ctx->ctx, 1, src_ptr, &src_size, &error);
    OPENCL_SUCCEED_FATAL(error);

    int compile_opts_size = 1024;

    for (int i = 0; i < ctx->cfg.num_sizes; i++) {
      compile_opts_size += strlen(ctx->cfg.size_names[i]) + 20;
    }

    for (int i = 0; extra_build_opts[i] != NULL; i++) {
      compile_opts_size += strlen(extra_build_opts[i] + 1);
    }

    char *compile_opts = (char*) malloc(compile_opts_size);

    int w = snprintf(compile_opts, compile_opts_size,
                     "-DLOCKSTEP_WIDTH=%d ",
                     (int)ctx->lockstep_width);

    for (int i = 0; i < ctx->cfg.num_sizes; i++) {
      w += snprintf(compile_opts+w, compile_opts_size-w,
                    "-D%s=%d ",
                    ctx->cfg.size_vars[i],
                    (int)ctx->cfg.size_values[i]);
    }

    for (int i = 0; extra_build_opts[i] != NULL; i++) {
      w += snprintf(compile_opts+w, compile_opts_size-w,
                    "%s ", extra_build_opts[i]);
    }

    OPENCL_SUCCEED_FATAL(build_opencl_program(prog, device_option.device, compile_opts));

    free(compile_opts);
  } else {
    size_t binary_size;
    unsigned char *fut_opencl_bin =
      (unsigned char*) slurp_file(ctx->cfg.load_binary_from, &binary_size);
    assert(fut_opencl_src != NULL);
    const unsigned char *binaries[1] = { fut_opencl_bin };
    cl_int status = 0;

    prog = clCreateProgramWithBinary(ctx->ctx, 1, &device_option.device,
                                     &binary_size, binaries,
                                     &status, &error);

    OPENCL_SUCCEED_FATAL(status);
    OPENCL_SUCCEED_FATAL(error);
  }

  free(fut_opencl_src);

  if (ctx->cfg.dump_binary_to != NULL) {
    size_t binary_size;
    OPENCL_SUCCEED_FATAL(clGetProgramInfo(prog, CL_PROGRAM_BINARY_SIZES,
                                          sizeof(size_t), &binary_size, NULL));
    unsigned char *binary = (unsigned char*) malloc(binary_size);
    unsigned char *binaries[1] = { binary };
    OPENCL_SUCCEED_FATAL(clGetProgramInfo(prog, CL_PROGRAM_BINARIES,
                                          sizeof(unsigned char*), binaries, NULL));

    FILE *f = fopen(ctx->cfg.dump_binary_to, "w");
    assert(f != NULL);
    fwrite(binary, sizeof(char), binary_size, f);
    fclose(f);
  }

  return prog;
}

static cl_program setup_opencl(struct opencl_context *ctx,
                               const char *srcs[],
                               int required_types,
                               const char *extra_build_opts[]) {

  ctx->lockstep_width = 0; // Real value set later.

  free_list_init(&ctx->free_list);

  struct opencl_device_option device_option = get_preferred_device(&ctx->cfg);

  if (ctx->cfg.logging) {
    describe_device_option(device_option);
  }

  // Note that NVIDIA's OpenCL requires the platform property
  cl_context_properties properties[] = {
    CL_CONTEXT_PLATFORM,
    (cl_context_properties)device_option.platform,
    0
  };

  cl_int clCreateContext_error;
  ctx->ctx = clCreateContext(properties, 1, &device_option.device, NULL, NULL, &clCreateContext_error);
  OPENCL_SUCCEED_FATAL(clCreateContext_error);

  cl_int clCreateCommandQueue_error;
  cl_command_queue queue = clCreateCommandQueue(ctx->ctx, device_option.device, 0, &clCreateCommandQueue_error);
  OPENCL_SUCCEED_FATAL(clCreateCommandQueue_error);

  return setup_opencl_with_command_queue(ctx, queue, srcs, required_types, extra_build_opts);
}

// Allocate memory from driver. The problem is that OpenCL may perform
// lazy allocation, so we cannot know whether an allocation succeeded
// until the first time we try to use it.  Hence we immediately
// perform a write to see if the allocation succeeded.  This is slow,
// but the assumption is that this operation will be rare (most things
// will go through the free list).
int opencl_alloc_actual(struct opencl_context *ctx, size_t size, cl_mem *mem_out) {
  int error;
  *mem_out = clCreateBuffer(ctx->ctx, CL_MEM_READ_WRITE, size, NULL, &error);

  if (error != CL_SUCCESS) {
    return error;
  }

  int x = 2;
  error = clEnqueueWriteBuffer(ctx->queue, *mem_out, 1, 0, sizeof(x), &x, 0, NULL, NULL);

  // No need to wait for completion here. clWaitForEvents() cannot
  // return mem object allocation failures. This implies that the
  // buffer is faulted onto the device on enqueue. (Observation by
  // Andreas Kloeckner.)

  return error;
}

int opencl_alloc(struct opencl_context *ctx, size_t min_size, const char *tag, cl_mem *mem_out) {
  if (min_size < sizeof(int)) {
    min_size = sizeof(int);
  }

  size_t size;

  if (free_list_find(&ctx->free_list, tag, &size, mem_out) == 0) {
    // Successfully found a free block.  Is it big enough?
    //
    // FIXME: we might also want to check whether the block is *too
    // big*, to avoid internal fragmentation.  However, this can
    // sharply impact performance on programs where arrays change size
    // frequently.  Fortunately, such allocations are usually fairly
    // short-lived, as they are necessarily within a loop, so the risk
    // of internal fragmentation resulting in an OOM situation is
    // limited.  However, it would be preferable if we could go back
    // and *shrink* oversize allocations when we encounter an OOM
    // condition.  That is technically feasible, since we do not
    // expose OpenCL pointer values directly to the application, but
    // instead rely on a level of indirection.
    if (size >= min_size) {
      return CL_SUCCESS;
    } else {
      // Not just right - free it.
      int error = clReleaseMemObject(*mem_out);
      if (error != CL_SUCCESS) {
        return error;
      }
    }
  }

  // We have to allocate a new block from the driver.  If the
  // allocation does not succeed, then we might be in an out-of-memory
  // situation.  We now start freeing things from the free list until
  // we think we have freed enough that the allocation will succeed.
  // Since we don't know how far the allocation is from fitting, we
  // have to check after every deallocation.  This might be pretty
  // expensive.  Let's hope that this case is hit rarely.

  int error = opencl_alloc_actual(ctx, min_size, mem_out);

  while (error == CL_MEM_OBJECT_ALLOCATION_FAILURE) {
    if (ctx->cfg.debugging) {
      fprintf(stderr, "Out of OpenCL memory: releasing entry from the free list...\n");
    }
    cl_mem mem;
    if (free_list_first(&ctx->free_list, &mem) == 0) {
      error = clReleaseMemObject(mem);
      if (error != CL_SUCCESS) {
        return error;
      }
    } else {
      break;
    }
    error = opencl_alloc_actual(ctx, min_size, mem_out);
  }

  return error;
}

int opencl_free(struct opencl_context *ctx, cl_mem mem, const char *tag) {
  size_t size;
  cl_mem existing_mem;

  // If there is already a block with this tag, then remove it.
  if (free_list_find(&ctx->free_list, tag, &size, &existing_mem) == 0) {
    int error = clReleaseMemObject(existing_mem);
    if (error != CL_SUCCESS) {
      return error;
    }
  }

  int error = clGetMemObjectInfo(mem, CL_MEM_SIZE, sizeof(size_t), &size, NULL);

  if (error == CL_SUCCESS) {
    free_list_insert(&ctx->free_list, size, mem, tag);
  }

  return error;
}

int opencl_free_all(struct opencl_context *ctx) {
  cl_mem mem;
  free_list_pack(&ctx->free_list);
  while (free_list_first(&ctx->free_list, &mem) == 0) {
    int error = clReleaseMemObject(mem);
    if (error != CL_SUCCESS) {
      return error;
    }
  }

  return CL_SUCCESS;
}

// End of opencl.h.

const char *opencl_program[] =
           {"#ifdef cl_clang_storage_class_specifiers\n#pragma OPENCL EXTENSION cl_clang_storage_class_specifiers : enable\n#endif\n#pragma OPENCL EXTENSION cl_khr_byte_addressable_store : enable\n__kernel void dummy_kernel(__global unsigned char *dummy, int n)\n{\n    const int thread_gid = get_global_id(0);\n    \n    if (thread_gid >= n)\n        return;\n}\ntypedef char int8_t;\ntypedef short int16_t;\ntypedef int int32_t;\ntypedef long int64_t;\ntypedef uchar uint8_t;\ntypedef ushort uint16_t;\ntypedef uint uint32_t;\ntypedef ulong uint64_t;\n#define ALIGNED_LOCAL_MEMORY(m,size) __local int64_t m[((size + 7) & ~7)/8]\n#ifdef cl_nv_pragma_unroll\nstatic inline void mem_fence_global()\n{\n    asm(\"membar.gl;\");\n}\n#else\nstatic inline void mem_fence_global()\n{\n    mem_fence(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);\n}\n#endif\nstatic inline void mem_fence_local()\n{\n    mem_fence(CLK_LOCAL_MEM_FENCE);\n}\nstatic inline int8_t add8(int8_t x, int8_t y)\n{\n    return x + y;\n}\nstatic inline int16_t add16(int16_t x, int16_t y)\n{\n    return x + y;\n}\nstatic inline int32_t add32(int32_t x, int32_t y)\n{\n    return x + y;\n}\nstatic inline int64_t add64(int64_t x, int64_t y)\n{\n    return x + y;\n}\nstatic inline int8_t sub8(int8_t x, int8_t y)\n{\n    return x - y;\n}\nstatic inline int16_t sub16(int16_t x, int16_t y)\n{\n    return x - y;\n}\nstatic inline int32_t sub32(int32_t x, int32_t y)\n{\n    return x - y;\n}\nstatic inline int64_t sub64(int64_t x, int64_t y)\n{\n    return x - y;\n}\nstatic inline int8_t mul8(int8_t x, int8_t y)\n{\n    return x * y;\n}\nstatic inline int16_t mul16(int16_t x, int16_t y)\n{\n    return x * y;\n}\nstatic inline int32_t mul32(int32_t x, int32_t y)\n{\n    return x * y;\n}\nstatic inline int64_t mul64(int64_t x, int64_t y)\n{\n    return x * y;\n}\nstatic inline uint8_t udiv8(uint8_t x, uint8_t y)\n{\n    return x / y;\n}\nstatic inline uint16_t udiv16(uint16_t x, uint16_t y)\n{\n    return x / y;\n}\nstatic inline uint32_t udiv32(uint32_t x, uint32_t y)\n{\n    return x / y;\n}\nstatic inline uint64_t udiv64(uint64_t x, u",
            "int64_t y)\n{\n    return x / y;\n}\nstatic inline uint8_t umod8(uint8_t x, uint8_t y)\n{\n    return x % y;\n}\nstatic inline uint16_t umod16(uint16_t x, uint16_t y)\n{\n    return x % y;\n}\nstatic inline uint32_t umod32(uint32_t x, uint32_t y)\n{\n    return x % y;\n}\nstatic inline uint64_t umod64(uint64_t x, uint64_t y)\n{\n    return x % y;\n}\nstatic inline int8_t sdiv8(int8_t x, int8_t y)\n{\n    int8_t q = x / y;\n    int8_t r = x % y;\n    \n    return q - ((r != 0 && r < 0 != y < 0) ? 1 : 0);\n}\nstatic inline int16_t sdiv16(int16_t x, int16_t y)\n{\n    int16_t q = x / y;\n    int16_t r = x % y;\n    \n    return q - ((r != 0 && r < 0 != y < 0) ? 1 : 0);\n}\nstatic inline int32_t sdiv32(int32_t x, int32_t y)\n{\n    int32_t q = x / y;\n    int32_t r = x % y;\n    \n    return q - ((r != 0 && r < 0 != y < 0) ? 1 : 0);\n}\nstatic inline int64_t sdiv64(int64_t x, int64_t y)\n{\n    int64_t q = x / y;\n    int64_t r = x % y;\n    \n    return q - ((r != 0 && r < 0 != y < 0) ? 1 : 0);\n}\nstatic inline int8_t smod8(int8_t x, int8_t y)\n{\n    int8_t r = x % y;\n    \n    return r + (r == 0 || (x > 0 && y > 0) || (x < 0 && y < 0) ? 0 : y);\n}\nstatic inline int16_t smod16(int16_t x, int16_t y)\n{\n    int16_t r = x % y;\n    \n    return r + (r == 0 || (x > 0 && y > 0) || (x < 0 && y < 0) ? 0 : y);\n}\nstatic inline int32_t smod32(int32_t x, int32_t y)\n{\n    int32_t r = x % y;\n    \n    return r + (r == 0 || (x > 0 && y > 0) || (x < 0 && y < 0) ? 0 : y);\n}\nstatic inline int64_t smod64(int64_t x, int64_t y)\n{\n    int64_t r = x % y;\n    \n    return r + (r == 0 || (x > 0 && y > 0) || (x < 0 && y < 0) ? 0 : y);\n}\nstatic inline int8_t squot8(int8_t x, int8_t y)\n{\n    return x / y;\n}\nstatic inline int16_t squot16(int16_t x, int16_t y)\n{\n    return x / y;\n}\nstatic inline int32_t squot32(int32_t x, int32_t y)\n{\n    return x / y;\n}\nstatic inline int64_t squot64(int64_t x, int64_t y)\n{\n    return x / y;\n}\nstatic inline int8_t srem8(int8_t x, int8_t y)\n{\n    return x % y;\n}\nstatic inline int16_t srem16(int16_t x, int16_t y)\n{\n    ",
            "return x % y;\n}\nstatic inline int32_t srem32(int32_t x, int32_t y)\n{\n    return x % y;\n}\nstatic inline int64_t srem64(int64_t x, int64_t y)\n{\n    return x % y;\n}\nstatic inline int8_t smin8(int8_t x, int8_t y)\n{\n    return x < y ? x : y;\n}\nstatic inline int16_t smin16(int16_t x, int16_t y)\n{\n    return x < y ? x : y;\n}\nstatic inline int32_t smin32(int32_t x, int32_t y)\n{\n    return x < y ? x : y;\n}\nstatic inline int64_t smin64(int64_t x, int64_t y)\n{\n    return x < y ? x : y;\n}\nstatic inline uint8_t umin8(uint8_t x, uint8_t y)\n{\n    return x < y ? x : y;\n}\nstatic inline uint16_t umin16(uint16_t x, uint16_t y)\n{\n    return x < y ? x : y;\n}\nstatic inline uint32_t umin32(uint32_t x, uint32_t y)\n{\n    return x < y ? x : y;\n}\nstatic inline uint64_t umin64(uint64_t x, uint64_t y)\n{\n    return x < y ? x : y;\n}\nstatic inline int8_t smax8(int8_t x, int8_t y)\n{\n    return x < y ? y : x;\n}\nstatic inline int16_t smax16(int16_t x, int16_t y)\n{\n    return x < y ? y : x;\n}\nstatic inline int32_t smax32(int32_t x, int32_t y)\n{\n    return x < y ? y : x;\n}\nstatic inline int64_t smax64(int64_t x, int64_t y)\n{\n    return x < y ? y : x;\n}\nstatic inline uint8_t umax8(uint8_t x, uint8_t y)\n{\n    return x < y ? y : x;\n}\nstatic inline uint16_t umax16(uint16_t x, uint16_t y)\n{\n    return x < y ? y : x;\n}\nstatic inline uint32_t umax32(uint32_t x, uint32_t y)\n{\n    return x < y ? y : x;\n}\nstatic inline uint64_t umax64(uint64_t x, uint64_t y)\n{\n    return x < y ? y : x;\n}\nstatic inline uint8_t shl8(uint8_t x, uint8_t y)\n{\n    return x << y;\n}\nstatic inline uint16_t shl16(uint16_t x, uint16_t y)\n{\n    return x << y;\n}\nstatic inline uint32_t shl32(uint32_t x, uint32_t y)\n{\n    return x << y;\n}\nstatic inline uint64_t shl64(uint64_t x, uint64_t y)\n{\n    return x << y;\n}\nstatic inline uint8_t lshr8(uint8_t x, uint8_t y)\n{\n    return x >> y;\n}\nstatic inline uint16_t lshr16(uint16_t x, uint16_t y)\n{\n    return x >> y;\n}\nstatic inline uint32_t lshr32(uint32_t x, uint32_t y)\n{\n    return x >> y;\n}\nstatic ",
            "inline uint64_t lshr64(uint64_t x, uint64_t y)\n{\n    return x >> y;\n}\nstatic inline int8_t ashr8(int8_t x, int8_t y)\n{\n    return x >> y;\n}\nstatic inline int16_t ashr16(int16_t x, int16_t y)\n{\n    return x >> y;\n}\nstatic inline int32_t ashr32(int32_t x, int32_t y)\n{\n    return x >> y;\n}\nstatic inline int64_t ashr64(int64_t x, int64_t y)\n{\n    return x >> y;\n}\nstatic inline uint8_t and8(uint8_t x, uint8_t y)\n{\n    return x & y;\n}\nstatic inline uint16_t and16(uint16_t x, uint16_t y)\n{\n    return x & y;\n}\nstatic inline uint32_t and32(uint32_t x, uint32_t y)\n{\n    return x & y;\n}\nstatic inline uint64_t and64(uint64_t x, uint64_t y)\n{\n    return x & y;\n}\nstatic inline uint8_t or8(uint8_t x, uint8_t y)\n{\n    return x | y;\n}\nstatic inline uint16_t or16(uint16_t x, uint16_t y)\n{\n    return x | y;\n}\nstatic inline uint32_t or32(uint32_t x, uint32_t y)\n{\n    return x | y;\n}\nstatic inline uint64_t or64(uint64_t x, uint64_t y)\n{\n    return x | y;\n}\nstatic inline uint8_t xor8(uint8_t x, uint8_t y)\n{\n    return x ^ y;\n}\nstatic inline uint16_t xor16(uint16_t x, uint16_t y)\n{\n    return x ^ y;\n}\nstatic inline uint32_t xor32(uint32_t x, uint32_t y)\n{\n    return x ^ y;\n}\nstatic inline uint64_t xor64(uint64_t x, uint64_t y)\n{\n    return x ^ y;\n}\nstatic inline char ult8(uint8_t x, uint8_t y)\n{\n    return x < y;\n}\nstatic inline char ult16(uint16_t x, uint16_t y)\n{\n    return x < y;\n}\nstatic inline char ult32(uint32_t x, uint32_t y)\n{\n    return x < y;\n}\nstatic inline char ult64(uint64_t x, uint64_t y)\n{\n    return x < y;\n}\nstatic inline char ule8(uint8_t x, uint8_t y)\n{\n    return x <= y;\n}\nstatic inline char ule16(uint16_t x, uint16_t y)\n{\n    return x <= y;\n}\nstatic inline char ule32(uint32_t x, uint32_t y)\n{\n    return x <= y;\n}\nstatic inline char ule64(uint64_t x, uint64_t y)\n{\n    return x <= y;\n}\nstatic inline char slt8(int8_t x, int8_t y)\n{\n    return x < y;\n}\nstatic inline char slt16(int16_t x, int16_t y)\n{\n    return x < y;\n}\nstatic inline char slt32(int32_t x, int32_t y)\n{\n    ",
            "return x < y;\n}\nstatic inline char slt64(int64_t x, int64_t y)\n{\n    return x < y;\n}\nstatic inline char sle8(int8_t x, int8_t y)\n{\n    return x <= y;\n}\nstatic inline char sle16(int16_t x, int16_t y)\n{\n    return x <= y;\n}\nstatic inline char sle32(int32_t x, int32_t y)\n{\n    return x <= y;\n}\nstatic inline char sle64(int64_t x, int64_t y)\n{\n    return x <= y;\n}\nstatic inline int8_t pow8(int8_t x, int8_t y)\n{\n    int8_t res = 1, rem = y;\n    \n    while (rem != 0) {\n        if (rem & 1)\n            res *= x;\n        rem >>= 1;\n        x *= x;\n    }\n    return res;\n}\nstatic inline int16_t pow16(int16_t x, int16_t y)\n{\n    int16_t res = 1, rem = y;\n    \n    while (rem != 0) {\n        if (rem & 1)\n            res *= x;\n        rem >>= 1;\n        x *= x;\n    }\n    return res;\n}\nstatic inline int32_t pow32(int32_t x, int32_t y)\n{\n    int32_t res = 1, rem = y;\n    \n    while (rem != 0) {\n        if (rem & 1)\n            res *= x;\n        rem >>= 1;\n        x *= x;\n    }\n    return res;\n}\nstatic inline int64_t pow64(int64_t x, int64_t y)\n{\n    int64_t res = 1, rem = y;\n    \n    while (rem != 0) {\n        if (rem & 1)\n            res *= x;\n        rem >>= 1;\n        x *= x;\n    }\n    return res;\n}\nstatic inline bool itob_i8_bool(int8_t x)\n{\n    return x;\n}\nstatic inline bool itob_i16_bool(int16_t x)\n{\n    return x;\n}\nstatic inline bool itob_i32_bool(int32_t x)\n{\n    return x;\n}\nstatic inline bool itob_i64_bool(int64_t x)\n{\n    return x;\n}\nstatic inline int8_t btoi_bool_i8(bool x)\n{\n    return x;\n}\nstatic inline int16_t btoi_bool_i16(bool x)\n{\n    return x;\n}\nstatic inline int32_t btoi_bool_i32(bool x)\n{\n    return x;\n}\nstatic inline int64_t btoi_bool_i64(bool x)\n{\n    return x;\n}\n#define sext_i8_i8(x) ((int8_t) (int8_t) x)\n#define sext_i8_i16(x) ((int16_t) (int8_t) x)\n#define sext_i8_i32(x) ((int32_t) (int8_t) x)\n#define sext_i8_i64(x) ((int64_t) (int8_t) x)\n#define sext_i16_i8(x) ((int8_t) (int16_t) x)\n#define sext_i16_i16(x) ((int16_t) (int16_t) x)\n#define sext_i16_i32(x) ((i",
            "nt32_t) (int16_t) x)\n#define sext_i16_i64(x) ((int64_t) (int16_t) x)\n#define sext_i32_i8(x) ((int8_t) (int32_t) x)\n#define sext_i32_i16(x) ((int16_t) (int32_t) x)\n#define sext_i32_i32(x) ((int32_t) (int32_t) x)\n#define sext_i32_i64(x) ((int64_t) (int32_t) x)\n#define sext_i64_i8(x) ((int8_t) (int64_t) x)\n#define sext_i64_i16(x) ((int16_t) (int64_t) x)\n#define sext_i64_i32(x) ((int32_t) (int64_t) x)\n#define sext_i64_i64(x) ((int64_t) (int64_t) x)\n#define zext_i8_i8(x) ((uint8_t) (uint8_t) x)\n#define zext_i8_i16(x) ((uint16_t) (uint8_t) x)\n#define zext_i8_i32(x) ((uint32_t) (uint8_t) x)\n#define zext_i8_i64(x) ((uint64_t) (uint8_t) x)\n#define zext_i16_i8(x) ((uint8_t) (uint16_t) x)\n#define zext_i16_i16(x) ((uint16_t) (uint16_t) x)\n#define zext_i16_i32(x) ((uint32_t) (uint16_t) x)\n#define zext_i16_i64(x) ((uint64_t) (uint16_t) x)\n#define zext_i32_i8(x) ((uint8_t) (uint32_t) x)\n#define zext_i32_i16(x) ((uint16_t) (uint32_t) x)\n#define zext_i32_i32(x) ((uint32_t) (uint32_t) x)\n#define zext_i32_i64(x) ((uint64_t) (uint32_t) x)\n#define zext_i64_i8(x) ((uint8_t) (uint64_t) x)\n#define zext_i64_i16(x) ((uint16_t) (uint64_t) x)\n#define zext_i64_i32(x) ((uint32_t) (uint64_t) x)\n#define zext_i64_i64(x) ((uint64_t) (uint64_t) x)\nstatic inline float fdiv32(float x, float y)\n{\n    return x / y;\n}\nstatic inline float fadd32(float x, float y)\n{\n    return x + y;\n}\nstatic inline float fsub32(float x, float y)\n{\n    return x - y;\n}\nstatic inline float fmul32(float x, float y)\n{\n    return x * y;\n}\nstatic inline float fmin32(float x, float y)\n{\n    return x < y ? x : y;\n}\nstatic inline float fmax32(float x, float y)\n{\n    return x < y ? y : x;\n}\nstatic inline float fpow32(float x, float y)\n{\n    return pow(x, y);\n}\nstatic inline char cmplt32(float x, float y)\n{\n    return x < y;\n}\nstatic inline char cmple32(float x, float y)\n{\n    return x <= y;\n}\nstatic inline float sitofp_i8_f32(int8_t x)\n{\n    return x;\n}\nstatic inline float sitofp_i16_f32(int16_t x)\n{\n    return x;\n}\nstatic inline flo",
            "at sitofp_i32_f32(int32_t x)\n{\n    return x;\n}\nstatic inline float sitofp_i64_f32(int64_t x)\n{\n    return x;\n}\nstatic inline float uitofp_i8_f32(uint8_t x)\n{\n    return x;\n}\nstatic inline float uitofp_i16_f32(uint16_t x)\n{\n    return x;\n}\nstatic inline float uitofp_i32_f32(uint32_t x)\n{\n    return x;\n}\nstatic inline float uitofp_i64_f32(uint64_t x)\n{\n    return x;\n}\nstatic inline int8_t fptosi_f32_i8(float x)\n{\n    return x;\n}\nstatic inline int16_t fptosi_f32_i16(float x)\n{\n    return x;\n}\nstatic inline int32_t fptosi_f32_i32(float x)\n{\n    return x;\n}\nstatic inline int64_t fptosi_f32_i64(float x)\n{\n    return x;\n}\nstatic inline uint8_t fptoui_f32_i8(float x)\n{\n    return x;\n}\nstatic inline uint16_t fptoui_f32_i16(float x)\n{\n    return x;\n}\nstatic inline uint32_t fptoui_f32_i32(float x)\n{\n    return x;\n}\nstatic inline uint64_t fptoui_f32_i64(float x)\n{\n    return x;\n}\nstatic inline float futrts_log32(float x)\n{\n    return log(x);\n}\nstatic inline float futrts_log2_32(float x)\n{\n    return log2(x);\n}\nstatic inline float futrts_log10_32(float x)\n{\n    return log10(x);\n}\nstatic inline float futrts_sqrt32(float x)\n{\n    return sqrt(x);\n}\nstatic inline float futrts_exp32(float x)\n{\n    return exp(x);\n}\nstatic inline float futrts_cos32(float x)\n{\n    return cos(x);\n}\nstatic inline float futrts_sin32(float x)\n{\n    return sin(x);\n}\nstatic inline float futrts_tan32(float x)\n{\n    return tan(x);\n}\nstatic inline float futrts_acos32(float x)\n{\n    return acos(x);\n}\nstatic inline float futrts_asin32(float x)\n{\n    return asin(x);\n}\nstatic inline float futrts_atan32(float x)\n{\n    return atan(x);\n}\nstatic inline float futrts_atan2_32(float x, float y)\n{\n    return atan2(x, y);\n}\nstatic inline float futrts_gamma32(float x)\n{\n    return tgamma(x);\n}\nstatic inline float futrts_lgamma32(float x)\n{\n    return lgamma(x);\n}\nstatic inline float futrts_round32(float x)\n{\n    return rint(x);\n}\nstatic inline char futrts_isnan32(float x)\n{\n    return isnan(x);\n}\nstatic inline char futrts_isi",
            "nf32(float x)\n{\n    return isinf(x);\n}\nstatic inline int32_t futrts_to_bits32(float x)\n{\n    union {\n        float f;\n        int32_t t;\n    } p;\n    \n    p.f = x;\n    return p.t;\n}\nstatic inline float futrts_from_bits32(int32_t x)\n{\n    union {\n        int32_t f;\n        float t;\n    } p;\n    \n    p.f = x;\n    return p.t;\n}\n__kernel void map_transpose_f32(int32_t destoffset_1, int32_t srcoffset_3,\n                                int32_t num_arrays_4, int32_t x_elems_5,\n                                int32_t y_elems_6, int32_t in_elems_7,\n                                int32_t out_elems_8, int32_t mulx_9,\n                                int32_t muly_10, __global\n                                unsigned char *destmem_0, __global\n                                unsigned char *srcmem_2)\n{\n    const int block_dim0 = 0;\n    const int block_dim1 = 1;\n    const int block_dim2 = 2;\n    \n    ALIGNED_LOCAL_MEMORY(block_11_backing_0, 4224);\n    \n    __local char *block_11;\n    \n    block_11 = (__local char *) block_11_backing_0;\n    \n    int32_t get_global_id_0_37;\n    \n    get_global_id_0_37 = get_global_id(0);\n    \n    int32_t get_local_id_0_38;\n    \n    get_local_id_0_38 = get_local_id(0);\n    \n    int32_t get_local_id_1_39;\n    \n    get_local_id_1_39 = get_local_id(1);\n    \n    int32_t get_group_id_0_40;\n    \n    get_group_id_0_40 = get_group_id(0);\n    \n    int32_t get_group_id_1_41;\n    \n    get_group_id_1_41 = get_group_id(1);\n    \n    int32_t get_group_id_2_42;\n    \n    get_group_id_2_42 = get_group_id(2);\n    \n    int32_t our_array_offset_30 = get_group_id_2_42 * x_elems_5 * y_elems_6;\n    int32_t odata_offset_33 = squot32(destoffset_1, 4) + our_array_offset_30;\n    int32_t idata_offset_34 = squot32(srcoffset_3, 4) + our_array_offset_30;\n    int32_t x_index_31 = get_global_id_0_37;\n    int32_t y_index_32 = get_group_id_1_41 * 32 + get_local_id_1_39;\n    \n    if (slt32(x_index_31, x_elems_5)) {\n        for (int32_t j_43 = 0; j_43 < 4; j_43++) {\n            int32_t in",
            "dex_in_35 = (y_index_32 + j_43 * 8) * x_elems_5 +\n                    x_index_31;\n            \n            if (slt32(y_index_32 + j_43 * 8, y_elems_6) && slt32(index_in_35,\n                                                                 in_elems_7)) {\n                ((__local float *) block_11)[(get_local_id_1_39 + j_43 * 8) *\n                                             33 + get_local_id_0_38] =\n                    ((__global float *) srcmem_2)[idata_offset_34 +\n                                                  index_in_35];\n            }\n        }\n    }\n    barrier(CLK_LOCAL_MEM_FENCE);\n    x_index_31 = get_group_id_1_41 * 32 + get_local_id_0_38;\n    y_index_32 = get_group_id_0_40 * 32 + get_local_id_1_39;\n    if (slt32(x_index_31, y_elems_6)) {\n        for (int32_t j_43 = 0; j_43 < 4; j_43++) {\n            int32_t index_out_36 = (y_index_32 + j_43 * 8) * y_elems_6 +\n                    x_index_31;\n            \n            if (slt32(y_index_32 + j_43 * 8, x_elems_5) && slt32(index_out_36,\n                                                                 out_elems_8)) {\n                ((__global float *) destmem_0)[odata_offset_33 + index_out_36] =\n                    ((__local float *) block_11)[get_local_id_0_38 * 33 +\n                                                 get_local_id_1_39 + j_43 * 8];\n            }\n        }\n    }\n}\n__kernel void map_transpose_f32_low_height(int32_t destoffset_1,\n                                           int32_t srcoffset_3,\n                                           int32_t num_arrays_4,\n                                           int32_t x_elems_5, int32_t y_elems_6,\n                                           int32_t in_elems_7,\n                                           int32_t out_elems_8, int32_t mulx_9,\n                                           int32_t muly_10, __global\n                                           unsigned char *destmem_0, __global\n                                           unsigned char *srcmem_2)\n{\n    const",
            " int block_dim0 = 0;\n    const int block_dim1 = 1;\n    const int block_dim2 = 2;\n    \n    ALIGNED_LOCAL_MEMORY(block_11_backing_0, 1088);\n    \n    __local char *block_11;\n    \n    block_11 = (__local char *) block_11_backing_0;\n    \n    int32_t get_global_id_0_37;\n    \n    get_global_id_0_37 = get_global_id(0);\n    \n    int32_t get_local_id_0_38;\n    \n    get_local_id_0_38 = get_local_id(0);\n    \n    int32_t get_local_id_1_39;\n    \n    get_local_id_1_39 = get_local_id(1);\n    \n    int32_t get_group_id_0_40;\n    \n    get_group_id_0_40 = get_group_id(0);\n    \n    int32_t get_group_id_1_41;\n    \n    get_group_id_1_41 = get_group_id(1);\n    \n    int32_t get_group_id_2_42;\n    \n    get_group_id_2_42 = get_group_id(2);\n    \n    int32_t our_array_offset_30 = get_group_id_2_42 * x_elems_5 * y_elems_6;\n    int32_t odata_offset_33 = squot32(destoffset_1, 4) + our_array_offset_30;\n    int32_t idata_offset_34 = squot32(srcoffset_3, 4) + our_array_offset_30;\n    int32_t x_index_31 = get_group_id_0_40 * 16 * mulx_9 + get_local_id_0_38 +\n            srem32(get_local_id_1_39, mulx_9) * 16;\n    int32_t y_index_32 = get_group_id_1_41 * 16 + squot32(get_local_id_1_39,\n                                                          mulx_9);\n    int32_t index_in_35 = y_index_32 * x_elems_5 + x_index_31;\n    \n    if (slt32(x_index_31, x_elems_5) && (slt32(y_index_32, y_elems_6) &&\n                                         slt32(index_in_35, in_elems_7))) {\n        ((__local float *) block_11)[get_local_id_1_39 * 17 +\n                                     get_local_id_0_38] = ((__global\n                                                            float *) srcmem_2)[idata_offset_34 +\n                                                                               index_in_35];\n    }\n    barrier(CLK_LOCAL_MEM_FENCE);\n    x_index_31 = get_group_id_1_41 * 16 + squot32(get_local_id_0_38, mulx_9);\n    y_index_32 = get_group_id_0_40 * 16 * mulx_9 + get_local_id_1_39 +\n        srem32(get_local_id_0_38, mulx",
            "_9) * 16;\n    \n    int32_t index_out_36 = y_index_32 * y_elems_6 + x_index_31;\n    \n    if (slt32(x_index_31, y_elems_6) && (slt32(y_index_32, x_elems_5) &&\n                                         slt32(index_out_36, out_elems_8))) {\n        ((__global float *) destmem_0)[odata_offset_33 + index_out_36] =\n            ((__local float *) block_11)[get_local_id_0_38 * 17 +\n                                         get_local_id_1_39];\n    }\n}\n__kernel void map_transpose_f32_low_width(int32_t destoffset_1,\n                                          int32_t srcoffset_3,\n                                          int32_t num_arrays_4,\n                                          int32_t x_elems_5, int32_t y_elems_6,\n                                          int32_t in_elems_7,\n                                          int32_t out_elems_8, int32_t mulx_9,\n                                          int32_t muly_10, __global\n                                          unsigned char *destmem_0, __global\n                                          unsigned char *srcmem_2)\n{\n    const int block_dim0 = 0;\n    const int block_dim1 = 1;\n    const int block_dim2 = 2;\n    \n    ALIGNED_LOCAL_MEMORY(block_11_backing_0, 1088);\n    \n    __local char *block_11;\n    \n    block_11 = (__local char *) block_11_backing_0;\n    \n    int32_t get_global_id_0_37;\n    \n    get_global_id_0_37 = get_global_id(0);\n    \n    int32_t get_local_id_0_38;\n    \n    get_local_id_0_38 = get_local_id(0);\n    \n    int32_t get_local_id_1_39;\n    \n    get_local_id_1_39 = get_local_id(1);\n    \n    int32_t get_group_id_0_40;\n    \n    get_group_id_0_40 = get_group_id(0);\n    \n    int32_t get_group_id_1_41;\n    \n    get_group_id_1_41 = get_group_id(1);\n    \n    int32_t get_group_id_2_42;\n    \n    get_group_id_2_42 = get_group_id(2);\n    \n    int32_t our_array_offset_30 = get_group_id_2_42 * x_elems_5 * y_elems_6;\n    int32_t odata_offset_33 = squot32(destoffset_1, 4) + our_array_offset_30;\n    int32_t idata_offset_34 = squot32(s",
            "rcoffset_3, 4) + our_array_offset_30;\n    int32_t x_index_31 = get_group_id_0_40 * 16 + squot32(get_local_id_0_38,\n                                                          muly_10);\n    int32_t y_index_32 = get_group_id_1_41 * 16 * muly_10 + get_local_id_1_39 +\n            srem32(get_local_id_0_38, muly_10) * 16;\n    int32_t index_in_35 = y_index_32 * x_elems_5 + x_index_31;\n    \n    if (slt32(x_index_31, x_elems_5) && (slt32(y_index_32, y_elems_6) &&\n                                         slt32(index_in_35, in_elems_7))) {\n        ((__local float *) block_11)[get_local_id_1_39 * 17 +\n                                     get_local_id_0_38] = ((__global\n                                                            float *) srcmem_2)[idata_offset_34 +\n                                                                               index_in_35];\n    }\n    barrier(CLK_LOCAL_MEM_FENCE);\n    x_index_31 = get_group_id_1_41 * 16 * muly_10 + get_local_id_0_38 +\n        srem32(get_local_id_1_39, muly_10) * 16;\n    y_index_32 = get_group_id_0_40 * 16 + squot32(get_local_id_1_39, muly_10);\n    \n    int32_t index_out_36 = y_index_32 * y_elems_6 + x_index_31;\n    \n    if (slt32(x_index_31, y_elems_6) && (slt32(y_index_32, x_elems_5) &&\n                                         slt32(index_out_36, out_elems_8))) {\n        ((__global float *) destmem_0)[odata_offset_33 + index_out_36] =\n            ((__local float *) block_11)[get_local_id_0_38 * 17 +\n                                         get_local_id_1_39];\n    }\n}\n__kernel void map_transpose_f32_small(int32_t destoffset_1, int32_t srcoffset_3,\n                                      int32_t num_arrays_4, int32_t x_elems_5,\n                                      int32_t y_elems_6, int32_t in_elems_7,\n                                      int32_t out_elems_8, int32_t mulx_9,\n                                      int32_t muly_10, __global\n                                      unsigned char *destmem_0, __global\n                        ",
            "              unsigned char *srcmem_2)\n{\n    const int block_dim0 = 0;\n    const int block_dim1 = 1;\n    const int block_dim2 = 2;\n    \n    ALIGNED_LOCAL_MEMORY(block_11_backing_0, 1);\n    \n    __local char *block_11;\n    \n    block_11 = (__local char *) block_11_backing_0;\n    \n    int32_t get_global_id_0_37;\n    \n    get_global_id_0_37 = get_global_id(0);\n    \n    int32_t get_local_id_0_38;\n    \n    get_local_id_0_38 = get_local_id(0);\n    \n    int32_t get_local_id_1_39;\n    \n    get_local_id_1_39 = get_local_id(1);\n    \n    int32_t get_group_id_0_40;\n    \n    get_group_id_0_40 = get_group_id(0);\n    \n    int32_t get_group_id_1_41;\n    \n    get_group_id_1_41 = get_group_id(1);\n    \n    int32_t get_group_id_2_42;\n    \n    get_group_id_2_42 = get_group_id(2);\n    \n    int32_t our_array_offset_30 = squot32(get_global_id_0_37, y_elems_6 *\n                                          x_elems_5) * (y_elems_6 * x_elems_5);\n    int32_t x_index_31 = squot32(srem32(get_global_id_0_37, y_elems_6 *\n                                        x_elems_5), y_elems_6);\n    int32_t y_index_32 = srem32(get_global_id_0_37, y_elems_6);\n    int32_t odata_offset_33 = squot32(destoffset_1, 4) + our_array_offset_30;\n    int32_t idata_offset_34 = squot32(srcoffset_3, 4) + our_array_offset_30;\n    int32_t index_in_35 = y_index_32 * x_elems_5 + x_index_31;\n    int32_t index_out_36 = x_index_31 * y_elems_6 + y_index_32;\n    \n    if (slt32(get_global_id_0_37, in_elems_7)) {\n        ((__global float *) destmem_0)[odata_offset_33 + index_out_36] =\n            ((__global float *) srcmem_2)[idata_offset_34 + index_in_35];\n    }\n}\n__kernel void replicate_14945(__global unsigned char *mem_14872)\n{\n    const int block_dim0 = 0;\n    const int block_dim1 = 1;\n    const int block_dim2 = 2;\n    int32_t replicate_gtid_14945;\n    int32_t replicate_ltid_14946;\n    int32_t replicate_gid_14947;\n    \n    replicate_gtid_14945 = get_global_id(0);\n    replicate_ltid_14946 = get_local_id(0);\n    replicate_gid_14947 = ge",
            "t_group_id(0);\n    if (slt32(replicate_gtid_14945, 4)) {\n        ((__global int8_t *) mem_14872)[replicate_gtid_14945] = 0;\n    }\n}\n__kernel void replicate_14950(int32_t iota_arg_14637, __global\n                              unsigned char *res_mem_14874, __global\n                              unsigned char *mem_14878)\n{\n    const int block_dim0 = 0;\n    const int block_dim1 = 1;\n    const int block_dim2 = 2;\n    int32_t replicate_gtid_14950;\n    int32_t replicate_ltid_14951;\n    int32_t replicate_gid_14952;\n    \n    replicate_gtid_14950 = get_global_id(0);\n    replicate_ltid_14951 = get_local_id(0);\n    replicate_gid_14952 = get_group_id(0);\n    if (slt32(replicate_gtid_14950, iota_arg_14637 * 4)) {\n        ((__global int8_t *) mem_14878)[squot32(replicate_gtid_14950, 4) * 4 +\n                                        (replicate_gtid_14950 -\n                                         squot32(replicate_gtid_14950, 4) *\n                                         4)] = ((__global\n                                                 int8_t *) res_mem_14874)[replicate_gtid_14950 -\n                                                                          squot32(replicate_gtid_14950,\n                                                                                  4) *\n                                                                          4];\n    }\n}\n__kernel void segmap_14729(int32_t s_14578, int32_t numS_14585, __global\n                           unsigned char *sRadii_mem_14844, __global\n                           unsigned char *mem_14849, __global\n                           unsigned char *mem_14852)\n{\n    const int32_t segmap_group_sizze_14732 = mainzisegmap_group_sizze_14731;\n    const int block_dim0 = 0;\n    const int block_dim1 = 1;\n    const int block_dim2 = 2;\n    int32_t global_tid_14888;\n    int32_t local_tid_14889;\n    int32_t group_sizze_14892;\n    int32_t wave_sizze_14891;\n    int32_t group_tid_14890;\n    \n    global_tid_14888 = get_global_id(0);\n    local_tid_14889",
            " = get_local_id(0);\n    group_sizze_14892 = get_local_size(0);\n    wave_sizze_14891 = LOCKSTEP_WIDTH;\n    group_tid_14890 = get_group_id(0);\n    \n    int32_t phys_tid_14729 = global_tid_14888;\n    int32_t gtid_14728 = group_tid_14890 * segmap_group_sizze_14732 +\n            local_tid_14889;\n    \n    if (slt32(gtid_14728, numS_14585)) {\n        float x_14737 = ((__global float *) sRadii_mem_14844)[gtid_14728];\n        float res_14738;\n        float redout_14835 = 0.0F;\n        \n        for (int32_t i_14836 = 0; i_14836 < 3; i_14836++) {\n            float x_14742 = ((__global float *) mem_14849)[i_14836 * s_14578 +\n                                                           gtid_14728];\n            float res_14743 = x_14742 * x_14742;\n            float res_14741 = res_14743 + redout_14835;\n            float redout_tmp_14893 = res_14741;\n            \n            redout_14835 = redout_tmp_14893;\n        }\n        res_14738 = redout_14835;\n        \n        float y_14744 = x_14737 * x_14737;\n        float c_14745 = res_14738 - y_14744;\n        float disc_14746 = 0.0F - c_14745;\n        bool cond_14747 = disc_14746 < 0.0F;\n        float res_14748;\n        \n        if (cond_14747) {\n            res_14748 = 100.0F;\n        } else {\n            float res_14749;\n            \n            res_14749 = futrts_sqrt32(disc_14746);\n            \n            float t_14750 = 0.0F - res_14749;\n            bool cond_14751 = 0.0F < t_14750;\n            float res_14752;\n            \n            if (cond_14751) {\n                res_14752 = t_14750;\n            } else {\n                res_14752 = 100.0F;\n            }\n            res_14748 = res_14752;\n        }\n        ((__global float *) mem_14852)[gtid_14728] = res_14748;\n    }\n}\n__kernel void segmap_14754(int32_t sizze_14581, int32_t numS_14585,\n                           int32_t numL_14586, __global\n                           unsigned char *sPositions_mem_14843, __global\n                           unsigned char *sRadii_mem_14844, __glob",
            "al\n                           unsigned char *mem_14855)\n{\n    const int32_t segmap_group_sizze_14757 = mainzisegmap_group_sizze_14756;\n    const int block_dim0 = 0;\n    const int block_dim1 = 1;\n    const int block_dim2 = 2;\n    int32_t global_tid_14894;\n    int32_t local_tid_14895;\n    int32_t group_sizze_14898;\n    int32_t wave_sizze_14897;\n    int32_t group_tid_14896;\n    \n    global_tid_14894 = get_global_id(0);\n    local_tid_14895 = get_local_id(0);\n    group_sizze_14898 = get_local_size(0);\n    wave_sizze_14897 = LOCKSTEP_WIDTH;\n    group_tid_14896 = get_group_id(0);\n    \n    int32_t phys_tid_14754 = global_tid_14894;\n    int32_t gtid_14753 = group_tid_14896 * segmap_group_sizze_14757 +\n            local_tid_14895;\n    \n    if (slt32(gtid_14753, numL_14586)) {\n        int32_t j_p_i_t_s_14808 = numS_14585 + gtid_14753;\n        float x_14762 = ((__global float *) sRadii_mem_14844)[j_p_i_t_s_14808];\n        float res_14763;\n        float redout_14837 = 0.0F;\n        \n        for (int32_t i_14838 = 0; i_14838 < 3; i_14838++) {\n            float x_14767 = ((__global\n                              float *) sPositions_mem_14843)[j_p_i_t_s_14808 *\n                                                             sizze_14581 +\n                                                             i_14838];\n            float res_14768 = x_14767 * x_14767;\n            float res_14766 = res_14768 + redout_14837;\n            float redout_tmp_14899 = res_14766;\n            \n            redout_14837 = redout_tmp_14899;\n        }\n        res_14763 = redout_14837;\n        \n        float y_14769 = x_14762 * x_14762;\n        float c_14770 = res_14763 - y_14769;\n        float disc_14771 = 0.0F - c_14770;\n        bool cond_14772 = disc_14771 < 0.0F;\n        float res_14773;\n        \n        if (cond_14772) {\n            res_14773 = 100.0F;\n        } else {\n            float res_14774;\n            \n            res_14774 = futrts_sqrt32(disc_14771);\n            \n            float t_14775 = 0.0F - r",
            "es_14774;\n            bool cond_14776 = 0.0F < t_14775;\n            float res_14777;\n            \n            if (cond_14776) {\n                res_14777 = t_14775;\n            } else {\n                res_14777 = 100.0F;\n            }\n            res_14773 = res_14777;\n        }\n        ((__global float *) mem_14855)[gtid_14753] = res_14773;\n    }\n}\n__kernel void segred_nonseg_14798(int32_t numS_14585, int32_t totalS_14611,\n                                  int32_t num_groups_14792, __global\n                                  unsigned char *mem_14852, __global\n                                  unsigned char *mem_14855, __global\n                                  unsigned char *mem_14859, __global\n                                  unsigned char *mem_14861, __global\n                                  unsigned char *mem_14864, __global\n                                  unsigned char *counter_mem_14900, __global\n                                  unsigned char *group_res_arr_mem_14902,\n                                  __global\n                                  unsigned char *group_res_arr_mem_14904,\n                                  __global\n                                  unsigned char *group_res_arr_mem_14906)\n{\n    const int32_t segred_group_sizze_14780 = mainzisegred_group_sizze_14779;\n    const int block_dim0 = 0;\n    const int block_dim1 = 1;\n    const int block_dim2 = 2;\n    \n    ALIGNED_LOCAL_MEMORY(sync_arr_mem_14914_backing_0, 1);\n    ALIGNED_LOCAL_MEMORY(red_arr_mem_14916_backing_1, 4 *\n                         mainzisegred_group_sizze_14779);\n    ALIGNED_LOCAL_MEMORY(red_arr_mem_14918_backing_2,\n                         mainzisegred_group_sizze_14779);\n    ALIGNED_LOCAL_MEMORY(red_arr_mem_14920_backing_3, 4 *\n                         mainzisegred_group_sizze_14779);\n    \n    int32_t global_tid_14909;\n    int32_t local_tid_14910;\n    int32_t group_sizze_14913;\n    int32_t wave_sizze_14912;\n    int32_t group_tid_14911;\n    \n    global_tid_14909 = get_global_id",
            "(0);\n    local_tid_14910 = get_local_id(0);\n    group_sizze_14913 = get_local_size(0);\n    wave_sizze_14912 = LOCKSTEP_WIDTH;\n    group_tid_14911 = get_group_id(0);\n    \n    int32_t phys_tid_14798 = global_tid_14909;\n    __local char *sync_arr_mem_14914;\n    \n    sync_arr_mem_14914 = (__local char *) sync_arr_mem_14914_backing_0;\n    \n    __local char *red_arr_mem_14916;\n    \n    red_arr_mem_14916 = (__local char *) red_arr_mem_14916_backing_1;\n    \n    __local char *red_arr_mem_14918;\n    \n    red_arr_mem_14918 = (__local char *) red_arr_mem_14918_backing_2;\n    \n    __local char *red_arr_mem_14920;\n    \n    red_arr_mem_14920 = (__local char *) red_arr_mem_14920_backing_3;\n    \n    int32_t dummy_14796 = 0;\n    int32_t gtid_14797;\n    \n    gtid_14797 = 0;\n    \n    int32_t x_acc_14922;\n    int8_t x_acc_14923;\n    float x_acc_14924;\n    int32_t chunk_sizze_14925;\n    int32_t starting_point_14926 = phys_tid_14798 * squot32(totalS_14611 +\n                                                            segred_group_sizze_14780 *\n                                                            num_groups_14792 -\n                                                            1,\n                                                            segred_group_sizze_14780 *\n                                                            num_groups_14792);\n    int32_t remaining_elements_14927 = totalS_14611 - starting_point_14926;\n    \n    if (sle32(remaining_elements_14927, 0) || sle32(totalS_14611,\n                                                    starting_point_14926)) {\n        chunk_sizze_14925 = 0;\n    } else {\n        if (slt32(totalS_14611, (phys_tid_14798 + 1) * squot32(totalS_14611 +\n                                                               segred_group_sizze_14780 *\n                                                               num_groups_14792 -\n                                                               1,\n                                                               segred_gr",
            "oup_sizze_14780 *\n                                                               num_groups_14792))) {\n            chunk_sizze_14925 = totalS_14611 - phys_tid_14798 *\n                squot32(totalS_14611 + segred_group_sizze_14780 *\n                        num_groups_14792 - 1, segred_group_sizze_14780 *\n                        num_groups_14792);\n        } else {\n            chunk_sizze_14925 = squot32(totalS_14611 +\n                                        segred_group_sizze_14780 *\n                                        num_groups_14792 - 1,\n                                        segred_group_sizze_14780 *\n                                        num_groups_14792);\n        }\n    }\n    \n    int32_t x_14706;\n    int8_t x_14707;\n    float x_14708;\n    int32_t x_14709;\n    int8_t x_14710;\n    float x_14711;\n    \n    // neutral-initialise the accumulators\n    {\n        x_acc_14922 = 0;\n        x_acc_14923 = 0;\n        x_acc_14924 = 100.0F;\n    }\n    for (int32_t i_14938 = 0; i_14938 < squot32(totalS_14611 +\n                                                segred_group_sizze_14780 *\n                                                num_groups_14792 - 1,\n                                                segred_group_sizze_14780 *\n                                                num_groups_14792); i_14938++) {\n        gtid_14797 = local_tid_14910 + (squot32(phys_tid_14798,\n                                                segred_group_sizze_14780) *\n                                        squot32(totalS_14611 +\n                                                segred_group_sizze_14780 *\n                                                num_groups_14792 - 1,\n                                                segred_group_sizze_14780 *\n                                                num_groups_14792) + i_14938) *\n            segred_group_sizze_14780;\n        if (slt32(gtid_14797, totalS_14611)) {\n            // apply map function\n            {\n                bool index_concat_cmp_14824 =",
            " sle32(numS_14585, gtid_14797);\n                int32_t index_concat_branch_14828;\n                \n                if (index_concat_cmp_14824) {\n                    int32_t index_concat_i_14825 = gtid_14797 - numS_14585;\n                    \n                    index_concat_branch_14828 = index_concat_i_14825;\n                } else {\n                    index_concat_branch_14828 = gtid_14797;\n                }\n                \n                int8_t index_concat_branch_14822;\n                \n                if (index_concat_cmp_14824) {\n                    index_concat_branch_14822 = 2;\n                } else {\n                    index_concat_branch_14822 = 1;\n                }\n                \n                float index_concat_branch_14816;\n                \n                if (index_concat_cmp_14824) {\n                    int32_t index_concat_i_14813 = gtid_14797 - numS_14585;\n                    float index_concat_14814 = ((__global\n                                                 float *) mem_14855)[index_concat_i_14813];\n                    \n                    index_concat_branch_14816 = index_concat_14814;\n                } else {\n                    float index_concat_14815 = ((__global\n                                                 float *) mem_14852)[gtid_14797];\n                    \n                    index_concat_branch_14816 = index_concat_14815;\n                }\n                // save map-out results\n                { }\n                // load accumulator\n                {\n                    x_14706 = x_acc_14922;\n                    x_14707 = x_acc_14923;\n                    x_14708 = x_acc_14924;\n                }\n                // load new values\n                {\n                    x_14709 = index_concat_branch_14828;\n                    x_14710 = index_concat_branch_14822;\n                    x_14711 = index_concat_branch_14816;\n                }\n                // apply reduction operator\n                {\n                    bool con",
            "d_14712 = x_14711 < x_14708;\n                    int32_t res_14713;\n                    \n                    if (cond_14712) {\n                        res_14713 = x_14709;\n                    } else {\n                        res_14713 = x_14706;\n                    }\n                    \n                    int8_t res_14714;\n                    \n                    if (cond_14712) {\n                        res_14714 = x_14710;\n                    } else {\n                        res_14714 = x_14707;\n                    }\n                    \n                    float res_14715;\n                    \n                    if (cond_14712) {\n                        res_14715 = x_14711;\n                    } else {\n                        res_14715 = x_14708;\n                    }\n                    // store in accumulator\n                    {\n                        x_acc_14922 = res_14713;\n                        x_acc_14923 = res_14714;\n                        x_acc_14924 = res_14715;\n                    }\n                }\n            }\n        }\n        // to reduce current chunk, first store our result in memory\n        {\n            x_14706 = x_acc_14922;\n            x_14707 = x_acc_14923;\n            x_14708 = x_acc_14924;\n            ((__local int32_t *) red_arr_mem_14916)[local_tid_14910] = x_14706;\n            ((__local int8_t *) red_arr_mem_14918)[local_tid_14910] = x_14707;\n            ((__local float *) red_arr_mem_14920)[local_tid_14910] = x_14708;\n        }\n        barrier(CLK_LOCAL_MEM_FENCE);\n        \n        int32_t offset_14939;\n        int32_t skip_waves_14940;\n        int32_t x_14928;\n        int8_t x_14929;\n        float x_14930;\n        int32_t x_14931;\n        int8_t x_14932;\n        float x_14933;\n        \n        offset_14939 = 0;\n        // participating threads read initial accumulator\n        {\n            if (slt32(local_tid_14910, segred_group_sizze_14780)) {\n                x_14928 = ((__local\n                            int32_t *) red_ar",
            "r_mem_14916)[local_tid_14910 +\n                                                          offset_14939];\n                x_14929 = ((__local\n                            int8_t *) red_arr_mem_14918)[local_tid_14910 +\n                                                         offset_14939];\n                x_14930 = ((__local\n                            float *) red_arr_mem_14920)[local_tid_14910 +\n                                                        offset_14939];\n            }\n        }\n        offset_14939 = 1;\n        while (slt32(offset_14939, wave_sizze_14912)) {\n            if (slt32(local_tid_14910 + offset_14939,\n                      segred_group_sizze_14780) && ((local_tid_14910 -\n                                                     squot32(local_tid_14910,\n                                                             wave_sizze_14912) *\n                                                     wave_sizze_14912) & (2 *\n                                                                          offset_14939 -\n                                                                          1)) ==\n                0) {\n                // read array element\n                {\n                    x_14931 = ((volatile __local\n                                int32_t *) red_arr_mem_14916)[local_tid_14910 +\n                                                              offset_14939];\n                    x_14932 = ((volatile __local\n                                int8_t *) red_arr_mem_14918)[local_tid_14910 +\n                                                             offset_14939];\n                    x_14933 = ((volatile __local\n                                float *) red_arr_mem_14920)[local_tid_14910 +\n                                                            offset_14939];\n                }\n                // apply reduction operation\n                {\n                    bool cond_14934 = x_14933 < x_14930;\n                    int32_t res_14935;\n                    \n       ",
            "             if (cond_14934) {\n                        res_14935 = x_14931;\n                    } else {\n                        res_14935 = x_14928;\n                    }\n                    \n                    int8_t res_14936;\n                    \n                    if (cond_14934) {\n                        res_14936 = x_14932;\n                    } else {\n                        res_14936 = x_14929;\n                    }\n                    \n                    float res_14937;\n                    \n                    if (cond_14934) {\n                        res_14937 = x_14933;\n                    } else {\n                        res_14937 = x_14930;\n                    }\n                    x_14928 = res_14935;\n                    x_14929 = res_14936;\n                    x_14930 = res_14937;\n                }\n                // write result of operation\n                {\n                    ((volatile __local\n                      int32_t *) red_arr_mem_14916)[local_tid_14910] = x_14928;\n                    ((volatile __local\n                      int8_t *) red_arr_mem_14918)[local_tid_14910] = x_14929;\n                    ((volatile __local\n                      float *) red_arr_mem_14920)[local_tid_14910] = x_14930;\n                }\n            }\n            offset_14939 *= 2;\n        }\n        skip_waves_14940 = 1;\n        while (slt32(skip_waves_14940, squot32(segred_group_sizze_14780 +\n                                               wave_sizze_14912 - 1,\n                                               wave_sizze_14912))) {\n            barrier(CLK_LOCAL_MEM_FENCE);\n            offset_14939 = skip_waves_14940 * wave_sizze_14912;\n            if (slt32(local_tid_14910 + offset_14939,\n                      segred_group_sizze_14780) && ((local_tid_14910 -\n                                                     squot32(local_tid_14910,\n                                                             wave_sizze_14912) *\n                                                ",
            "     wave_sizze_14912) == 0 &&\n                                                    (squot32(local_tid_14910,\n                                                             wave_sizze_14912) &\n                                                     (2 * skip_waves_14940 -\n                                                      1)) == 0)) {\n                // read array element\n                {\n                    x_14931 = ((__local\n                                int32_t *) red_arr_mem_14916)[local_tid_14910 +\n                                                              offset_14939];\n                    x_14932 = ((__local\n                                int8_t *) red_arr_mem_14918)[local_tid_14910 +\n                                                             offset_14939];\n                    x_14933 = ((__local\n                                float *) red_arr_mem_14920)[local_tid_14910 +\n                                                            offset_14939];\n                }\n                // apply reduction operation\n                {\n                    bool cond_14934 = x_14933 < x_14930;\n                    int32_t res_14935;\n                    \n                    if (cond_14934) {\n                        res_14935 = x_14931;\n                    } else {\n                        res_14935 = x_14928;\n                    }\n                    \n                    int8_t res_14936;\n                    \n                    if (cond_14934) {\n                        res_14936 = x_14932;\n                    } else {\n                        res_14936 = x_14929;\n                    }\n                    \n                    float res_14937;\n                    \n                    if (cond_14934) {\n                        res_14937 = x_14933;\n                    } else {\n                        res_14937 = x_14930;\n                    }\n                    x_14928 = res_14935;\n                    x_14929 = res_14936;\n                    x_14930 = res_14937;\n        ",
            "        }\n                // write result of operation\n                {\n                    ((__local int32_t *) red_arr_mem_14916)[local_tid_14910] =\n                        x_14928;\n                    ((__local int8_t *) red_arr_mem_14918)[local_tid_14910] =\n                        x_14929;\n                    ((__local float *) red_arr_mem_14920)[local_tid_14910] =\n                        x_14930;\n                }\n            }\n            skip_waves_14940 *= 2;\n        }\n        barrier(CLK_LOCAL_MEM_FENCE);\n        // first thread saves the result in accumulator\n        {\n            if (local_tid_14910 == 0) {\n                x_acc_14922 = x_14928;\n                x_acc_14923 = x_14929;\n                x_acc_14924 = x_14930;\n            }\n        }\n        // first thread keeps accumulator; others reset to neutral element\n        {\n            if (!(local_tid_14910 == 0)) {\n                x_acc_14922 = 0;\n                x_acc_14923 = 0;\n                x_acc_14924 = 100.0F;\n            }\n        }\n    }\n    x_14706 = x_acc_14922;\n    x_14707 = x_acc_14923;\n    x_14708 = x_acc_14924;\n    \n    int32_t old_counter_14941;\n    \n    // first thread in group saves group result to global memory\n    {\n        if (local_tid_14910 == 0) {\n            ((__global int32_t *) group_res_arr_mem_14902)[group_tid_14911 *\n                                                           segred_group_sizze_14780] =\n                x_acc_14922;\n            ((__global int8_t *) group_res_arr_mem_14904)[group_tid_14911 *\n                                                          segred_group_sizze_14780] =\n                x_acc_14923;\n            ((__global float *) group_res_arr_mem_14906)[group_tid_14911 *\n                                                         segred_group_sizze_14780] =\n                x_acc_14924;\n            mem_fence_global();\n            old_counter_14941 = atomic_add(&((volatile __global\n                                              int *) counter_mem_14900)[",
            "0],\n                                           (int) 1);\n            ((__local bool *) sync_arr_mem_14914)[0] = old_counter_14941 ==\n                num_groups_14792 - 1;\n        }\n    }\n    barrier(CLK_LOCAL_MEM_FENCE);\n    barrier(CLK_GLOBAL_MEM_FENCE);\n    \n    bool is_last_group_14942 = ((__local bool *) sync_arr_mem_14914)[0];\n    \n    if (is_last_group_14942) {\n        if (local_tid_14910 == 0) {\n            old_counter_14941 = atomic_add(&((volatile __global\n                                              int *) counter_mem_14900)[0],\n                                           (int) (0 - num_groups_14792));\n        }\n        // read in the per-group-results\n        {\n            if (slt32(local_tid_14910, num_groups_14792)) {\n                x_14706 = ((__global\n                            int32_t *) group_res_arr_mem_14902)[local_tid_14910 *\n                                                                segred_group_sizze_14780];\n            } else {\n                x_14706 = 0;\n            }\n            ((__local int32_t *) red_arr_mem_14916)[local_tid_14910] = x_14706;\n            if (slt32(local_tid_14910, num_groups_14792)) {\n                x_14707 = ((__global\n                            int8_t *) group_res_arr_mem_14904)[local_tid_14910 *\n                                                               segred_group_sizze_14780];\n            } else {\n                x_14707 = 0;\n            }\n            ((__local int8_t *) red_arr_mem_14918)[local_tid_14910] = x_14707;\n            if (slt32(local_tid_14910, num_groups_14792)) {\n                x_14708 = ((__global\n                            float *) group_res_arr_mem_14906)[local_tid_14910 *\n                                                              segred_group_sizze_14780];\n            } else {\n                x_14708 = 100.0F;\n            }\n            ((__local float *) red_arr_mem_14920)[local_tid_14910] = x_14708;\n        }\n        barrier(CLK_LOCAL_MEM_FENCE);\n        // reduce the per-group r",
            "esults\n        {\n            int32_t offset_14943;\n            int32_t skip_waves_14944;\n            int32_t x_14928;\n            int8_t x_14929;\n            float x_14930;\n            int32_t x_14931;\n            int8_t x_14932;\n            float x_14933;\n            \n            offset_14943 = 0;\n            // participating threads read initial accumulator\n            {\n                if (slt32(local_tid_14910, segred_group_sizze_14780)) {\n                    x_14928 = ((__local\n                                int32_t *) red_arr_mem_14916)[local_tid_14910 +\n                                                              offset_14943];\n                    x_14929 = ((__local\n                                int8_t *) red_arr_mem_14918)[local_tid_14910 +\n                                                             offset_14943];\n                    x_14930 = ((__local\n                                float *) red_arr_mem_14920)[local_tid_14910 +\n                                                            offset_14943];\n                }\n            }\n            offset_14943 = 1;\n            while (slt32(offset_14943, wave_sizze_14912)) {\n                if (slt32(local_tid_14910 + offset_14943,\n                          segred_group_sizze_14780) && ((local_tid_14910 -\n                                                         squot32(local_tid_14910,\n                                                                 wave_sizze_14912) *\n                                                         wave_sizze_14912) &\n                                                        (2 * offset_14943 -\n                                                         1)) == 0) {\n                    // read array element\n                    {\n                        x_14931 = ((volatile __local\n                                    int32_t *) red_arr_mem_14916)[local_tid_14910 +\n                                                                  offset_14943];\n                        x_14932 = ((volat",
            "ile __local\n                                    int8_t *) red_arr_mem_14918)[local_tid_14910 +\n                                                                 offset_14943];\n                        x_14933 = ((volatile __local\n                                    float *) red_arr_mem_14920)[local_tid_14910 +\n                                                                offset_14943];\n                    }\n                    // apply reduction operation\n                    {\n                        bool cond_14934 = x_14933 < x_14930;\n                        int32_t res_14935;\n                        \n                        if (cond_14934) {\n                            res_14935 = x_14931;\n                        } else {\n                            res_14935 = x_14928;\n                        }\n                        \n                        int8_t res_14936;\n                        \n                        if (cond_14934) {\n                            res_14936 = x_14932;\n                        } else {\n                            res_14936 = x_14929;\n                        }\n                        \n                        float res_14937;\n                        \n                        if (cond_14934) {\n                            res_14937 = x_14933;\n                        } else {\n                            res_14937 = x_14930;\n                        }\n                        x_14928 = res_14935;\n                        x_14929 = res_14936;\n                        x_14930 = res_14937;\n                    }\n                    // write result of operation\n                    {\n                        ((volatile __local\n                          int32_t *) red_arr_mem_14916)[local_tid_14910] =\n                            x_14928;\n                        ((volatile __local\n                          int8_t *) red_arr_mem_14918)[local_tid_14910] =\n                            x_14929;\n                        ((volatile __local\n                          flo",
            "at *) red_arr_mem_14920)[local_tid_14910] =\n                            x_14930;\n                    }\n                }\n                offset_14943 *= 2;\n            }\n            skip_waves_14944 = 1;\n            while (slt32(skip_waves_14944, squot32(segred_group_sizze_14780 +\n                                                   wave_sizze_14912 - 1,\n                                                   wave_sizze_14912))) {\n                barrier(CLK_LOCAL_MEM_FENCE);\n                offset_14943 = skip_waves_14944 * wave_sizze_14912;\n                if (slt32(local_tid_14910 + offset_14943,\n                          segred_group_sizze_14780) && ((local_tid_14910 -\n                                                         squot32(local_tid_14910,\n                                                                 wave_sizze_14912) *\n                                                         wave_sizze_14912) ==\n                                                        0 &&\n                                                        (squot32(local_tid_14910,\n                                                                 wave_sizze_14912) &\n                                                         (2 * skip_waves_14944 -\n                                                          1)) == 0)) {\n                    // read array element\n                    {\n                        x_14931 = ((__local\n                                    int32_t *) red_arr_mem_14916)[local_tid_14910 +\n                                                                  offset_14943];\n                        x_14932 = ((__local\n                                    int8_t *) red_arr_mem_14918)[local_tid_14910 +\n                                                                 offset_14943];\n                        x_14933 = ((__local\n                                    float *) red_arr_mem_14920)[local_tid_14910 +\n                                                                offset_14943];\n               ",
            "     }\n                    // apply reduction operation\n                    {\n                        bool cond_14934 = x_14933 < x_14930;\n                        int32_t res_14935;\n                        \n                        if (cond_14934) {\n                            res_14935 = x_14931;\n                        } else {\n                            res_14935 = x_14928;\n                        }\n                        \n                        int8_t res_14936;\n                        \n                        if (cond_14934) {\n                            res_14936 = x_14932;\n                        } else {\n                            res_14936 = x_14929;\n                        }\n                        \n                        float res_14937;\n                        \n                        if (cond_14934) {\n                            res_14937 = x_14933;\n                        } else {\n                            res_14937 = x_14930;\n                        }\n                        x_14928 = res_14935;\n                        x_14929 = res_14936;\n                        x_14930 = res_14937;\n                    }\n                    // write result of operation\n                    {\n                        ((__local\n                          int32_t *) red_arr_mem_14916)[local_tid_14910] =\n                            x_14928;\n                        ((__local\n                          int8_t *) red_arr_mem_14918)[local_tid_14910] =\n                            x_14929;\n                        ((__local float *) red_arr_mem_14920)[local_tid_14910] =\n                            x_14930;\n                    }\n                }\n                skip_waves_14944 *= 2;\n            }\n            // and back to memory with the final result\n            {\n                if (local_tid_14910 == 0) {\n                    ((__global int32_t *) mem_14859)[0] = x_14928;\n                    ((__global int8_t *) mem_14861)[0] = x_14929;\n                    ((__global float",
            " *) mem_14864)[0] = x_14930;\n                }\n            }\n        }\n    }\n}\n",
            NULL};
static int32_t counter_mem_realtype_14969[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
struct memblock_device {
    int *references;
    cl_mem mem;
    int64_t size;
    const char *desc;
} ;
struct memblock {
    int *references;
    char *mem;
    int64_t size;
    const char *desc;
} ;
static const char *size_names[] = {"main.group_size_14948",
                                   "main.group_size_14953",
                                   "main.segmap_group_size_14731",
                                   "main.segmap_group_size_14756",
                                   "main.segred_group_size_14779",
                                   "main.segred_max_num_groups_14782"};
static const char *size_vars[] = {"mainzigroup_sizze_14948",
                                  "mainzigroup_sizze_14953",
                                  "mainzisegmap_group_sizze_14731",
                                  "mainzisegmap_group_sizze_14756",
                                  "mainzisegred_group_sizze_14779",
                                  "mainzisegred_max_num_groups_14782"};
static const char *size_classes[] = {"group_size", "group_size", "group_size",
                                     "group_size", "group_size", "num_groups"};
int futhark_get_num_sizes(void)
{
    return 6;
}
const char *futhark_get_size_name(int i)
{
    return size_names[i];
}
const char *futhark_get_size_class(int i)
{
    return size_classes[i];
}
struct sizes {
    size_t mainzigroup_sizze_14948;
    size_t mainzigroup_sizze_14953;
    size_t mainzisegmap_group_sizze_14731;
    size_t mainzisegmap_group_sizze_14756;
    size_t mainzisegred_group_sizze_14779;
    size_t mainzisegred_max_num_groups_14782;
} ;
struct futhark_context_config {
    struct opencl_config opencl;
    size_t sizes[6];
    int num_build_opts;
    const char **build_opts;
} ;
struct futhark_context_config *futhark_context_config_new(void)
{
    struct futhark_context_config *cfg =
                                  (struct futhark_context_config *) malloc(sizeof(struct futhark_context_config));
    
    if (cfg == NULL)
        return NULL;
    cfg->num_build_opts = 0;
    cfg->build_opts = (const char **) malloc(sizeof(const char *));
    cfg->build_opts[0] = NULL;
    cfg->sizes[0] = 0;
    cfg->sizes[1] = 0;
    cfg->sizes[2] = 0;
    cfg->sizes[3] = 0;
    cfg->sizes[4] = 0;
    cfg->sizes[5] = 0;
    opencl_config_init(&cfg->opencl, 6, size_names, size_vars, cfg->sizes,
                       size_classes);
    return cfg;
}
void futhark_context_config_free(struct futhark_context_config *cfg)
{
    free(cfg->build_opts);
    free(cfg);
}
void futhark_context_config_add_build_option(struct futhark_context_config *cfg,
                                             const char *opt)
{
    cfg->build_opts[cfg->num_build_opts] = opt;
    cfg->num_build_opts++;
    cfg->build_opts = (const char **) realloc(cfg->build_opts,
                                              (cfg->num_build_opts + 1) *
                                              sizeof(const char *));
    cfg->build_opts[cfg->num_build_opts] = NULL;
}
void futhark_context_config_set_debugging(struct futhark_context_config *cfg,
                                          int flag)
{
    cfg->opencl.logging = cfg->opencl.debugging = flag;
}
void futhark_context_config_set_logging(struct futhark_context_config *cfg,
                                        int flag)
{
    cfg->opencl.logging = flag;
}
void futhark_context_config_set_device(struct futhark_context_config *cfg, const
                                       char *s)
{
    set_preferred_device(&cfg->opencl, s);
}
void futhark_context_config_set_platform(struct futhark_context_config *cfg,
                                         const char *s)
{
    set_preferred_platform(&cfg->opencl, s);
}
void futhark_context_config_select_device_interactively(struct futhark_context_config *cfg)
{
    select_device_interactively(&cfg->opencl);
}
void futhark_context_config_dump_program_to(struct futhark_context_config *cfg,
                                            const char *path)
{
    cfg->opencl.dump_program_to = path;
}
void futhark_context_config_load_program_from(struct futhark_context_config *cfg,
                                              const char *path)
{
    cfg->opencl.load_program_from = path;
}
void futhark_context_config_dump_binary_to(struct futhark_context_config *cfg,
                                           const char *path)
{
    cfg->opencl.dump_binary_to = path;
}
void futhark_context_config_load_binary_from(struct futhark_context_config *cfg,
                                             const char *path)
{
    cfg->opencl.load_binary_from = path;
}
void futhark_context_config_set_default_group_size(struct futhark_context_config *cfg,
                                                   int size)
{
    cfg->opencl.default_group_size = size;
    cfg->opencl.default_group_size_changed = 1;
}
void futhark_context_config_set_default_num_groups(struct futhark_context_config *cfg,
                                                   int num)
{
    cfg->opencl.default_num_groups = num;
}
void futhark_context_config_set_default_tile_size(struct futhark_context_config *cfg,
                                                  int size)
{
    cfg->opencl.default_tile_size = size;
    cfg->opencl.default_tile_size_changed = 1;
}
void futhark_context_config_set_default_threshold(struct futhark_context_config *cfg,
                                                  int size)
{
    cfg->opencl.default_threshold = size;
}
int futhark_context_config_set_size(struct futhark_context_config *cfg, const
                                    char *size_name, size_t size_value)
{
    for (int i = 0; i < 6; i++) {
        if (strcmp(size_name, size_names[i]) == 0) {
            cfg->sizes[i] = size_value;
            return 0;
        }
    }
    if (strcmp(size_name, "default_group_size") == 0) {
        cfg->opencl.default_group_size = size_value;
        return 0;
    }
    if (strcmp(size_name, "default_num_groups") == 0) {
        cfg->opencl.default_num_groups = size_value;
        return 0;
    }
    if (strcmp(size_name, "default_threshold") == 0) {
        cfg->opencl.default_threshold = size_value;
        return 0;
    }
    if (strcmp(size_name, "default_tile_size") == 0) {
        cfg->opencl.default_tile_size = size_value;
        return 0;
    }
    return 1;
}
struct futhark_context {
    int detail_memory;
    int debugging;
    int logging;
    lock_t lock;
    char *error;
    int64_t peak_mem_usage_device;
    int64_t cur_mem_usage_device;
    int64_t peak_mem_usage_default;
    int64_t cur_mem_usage_default;
    struct memblock_device counter_mem_14900;
    int total_runs;
    long total_runtime;
    cl_kernel map_transpose_f32;
    int map_transpose_f32_total_runtime;
    int map_transpose_f32_runs;
    cl_kernel map_transpose_f32_low_height;
    int map_transpose_f32_low_height_total_runtime;
    int map_transpose_f32_low_height_runs;
    cl_kernel map_transpose_f32_low_width;
    int map_transpose_f32_low_width_total_runtime;
    int map_transpose_f32_low_width_runs;
    cl_kernel map_transpose_f32_small;
    int map_transpose_f32_small_total_runtime;
    int map_transpose_f32_small_runs;
    cl_kernel replicate_14945;
    int replicate_14945_total_runtime;
    int replicate_14945_runs;
    cl_kernel replicate_14950;
    int replicate_14950_total_runtime;
    int replicate_14950_runs;
    cl_kernel segmap_14729;
    int segmap_14729_total_runtime;
    int segmap_14729_runs;
    cl_kernel segmap_14754;
    int segmap_14754_total_runtime;
    int segmap_14754_runs;
    cl_kernel segred_nonseg_14798;
    int segred_nonseg_14798_total_runtime;
    int segred_nonseg_14798_runs;
    struct opencl_context opencl;
    struct sizes sizes;
} ;
void post_opencl_setup(struct opencl_context *ctx,
                       struct opencl_device_option *option)
{
    if ((ctx->lockstep_width == 0 && strstr(option->platform_name,
                                            "NVIDIA CUDA") != NULL) &&
        (option->device_type & CL_DEVICE_TYPE_GPU) == CL_DEVICE_TYPE_GPU)
        ctx->lockstep_width = 32;
    if ((ctx->lockstep_width == 0 && strstr(option->platform_name,
                                            "AMD Accelerated Parallel Processing") !=
         NULL) && (option->device_type & CL_DEVICE_TYPE_GPU) ==
        CL_DEVICE_TYPE_GPU)
        ctx->lockstep_width = 32;
    if ((ctx->lockstep_width == 0 && strstr(option->platform_name, "") !=
         NULL) && (option->device_type & CL_DEVICE_TYPE_GPU) ==
        CL_DEVICE_TYPE_GPU)
        ctx->lockstep_width = 1;
    if ((ctx->cfg.default_num_groups == 0 && strstr(option->platform_name,
                                                    "") != NULL) &&
        (option->device_type & CL_DEVICE_TYPE_GPU) == CL_DEVICE_TYPE_GPU)
        ctx->cfg.default_num_groups = 256;
    if ((ctx->cfg.default_group_size == 0 && strstr(option->platform_name,
                                                    "") != NULL) &&
        (option->device_type & CL_DEVICE_TYPE_GPU) == CL_DEVICE_TYPE_GPU)
        ctx->cfg.default_group_size = 256;
    if ((ctx->cfg.default_tile_size == 0 && strstr(option->platform_name, "") !=
         NULL) && (option->device_type & CL_DEVICE_TYPE_GPU) ==
        CL_DEVICE_TYPE_GPU)
        ctx->cfg.default_tile_size = 32;
    if ((ctx->cfg.default_threshold == 0 && strstr(option->platform_name, "") !=
         NULL) && (option->device_type & CL_DEVICE_TYPE_GPU) ==
        CL_DEVICE_TYPE_GPU)
        ctx->cfg.default_threshold = 32768;
    if ((ctx->lockstep_width == 0 && strstr(option->platform_name, "") !=
         NULL) && (option->device_type & CL_DEVICE_TYPE_CPU) ==
        CL_DEVICE_TYPE_CPU)
        ctx->lockstep_width = 1;
    if ((ctx->cfg.default_num_groups == 0 && strstr(option->platform_name,
                                                    "") != NULL) &&
        (option->device_type & CL_DEVICE_TYPE_CPU) == CL_DEVICE_TYPE_CPU)
        clGetDeviceInfo(ctx->device, CL_DEVICE_MAX_COMPUTE_UNITS,
                        sizeof(ctx->cfg.default_num_groups),
                        &ctx->cfg.default_num_groups, NULL);
    if ((ctx->cfg.default_group_size == 0 && strstr(option->platform_name,
                                                    "") != NULL) &&
        (option->device_type & CL_DEVICE_TYPE_CPU) == CL_DEVICE_TYPE_CPU)
        ctx->cfg.default_group_size = 32;
    if ((ctx->cfg.default_tile_size == 0 && strstr(option->platform_name, "") !=
         NULL) && (option->device_type & CL_DEVICE_TYPE_CPU) ==
        CL_DEVICE_TYPE_CPU)
        ctx->cfg.default_tile_size = 4;
    if ((ctx->cfg.default_threshold == 0 && strstr(option->platform_name, "") !=
         NULL) && (option->device_type & CL_DEVICE_TYPE_CPU) ==
        CL_DEVICE_TYPE_CPU)
        clGetDeviceInfo(ctx->device, CL_DEVICE_MAX_COMPUTE_UNITS,
                        sizeof(ctx->cfg.default_threshold),
                        &ctx->cfg.default_threshold, NULL);
}
static void init_context_early(struct futhark_context_config *cfg,
                               struct futhark_context *ctx)
{
    ctx->opencl.cfg = cfg->opencl;
    ctx->detail_memory = cfg->opencl.debugging;
    ctx->debugging = cfg->opencl.debugging;
    ctx->logging = cfg->opencl.logging;
    ctx->error = NULL;
    create_lock(&ctx->lock);
    ctx->peak_mem_usage_device = 0;
    ctx->cur_mem_usage_device = 0;
    ctx->peak_mem_usage_default = 0;
    ctx->cur_mem_usage_default = 0;
    ctx->total_runs = 0;
    ctx->total_runtime = 0;
    ctx->map_transpose_f32_total_runtime = 0;
    ctx->map_transpose_f32_runs = 0;
    ctx->map_transpose_f32_low_height_total_runtime = 0;
    ctx->map_transpose_f32_low_height_runs = 0;
    ctx->map_transpose_f32_low_width_total_runtime = 0;
    ctx->map_transpose_f32_low_width_runs = 0;
    ctx->map_transpose_f32_small_total_runtime = 0;
    ctx->map_transpose_f32_small_runs = 0;
    ctx->replicate_14945_total_runtime = 0;
    ctx->replicate_14945_runs = 0;
    ctx->replicate_14950_total_runtime = 0;
    ctx->replicate_14950_runs = 0;
    ctx->segmap_14729_total_runtime = 0;
    ctx->segmap_14729_runs = 0;
    ctx->segmap_14754_total_runtime = 0;
    ctx->segmap_14754_runs = 0;
    ctx->segred_nonseg_14798_total_runtime = 0;
    ctx->segred_nonseg_14798_runs = 0;
}
static int init_context_late(struct futhark_context_config *cfg,
                             struct futhark_context *ctx, cl_program prog)
{
    cl_int error;
    
    {
        ctx->map_transpose_f32 = clCreateKernel(prog, "map_transpose_f32",
                                                &error);
        OPENCL_SUCCEED_FATAL(error);
        if (ctx->debugging)
            fprintf(stderr, "Created kernel %s.\n", "map_transpose_f32");
    }
    {
        ctx->map_transpose_f32_low_height = clCreateKernel(prog,
                                                           "map_transpose_f32_low_height",
                                                           &error);
        OPENCL_SUCCEED_FATAL(error);
        if (ctx->debugging)
            fprintf(stderr, "Created kernel %s.\n",
                    "map_transpose_f32_low_height");
    }
    {
        ctx->map_transpose_f32_low_width = clCreateKernel(prog,
                                                          "map_transpose_f32_low_width",
                                                          &error);
        OPENCL_SUCCEED_FATAL(error);
        if (ctx->debugging)
            fprintf(stderr, "Created kernel %s.\n",
                    "map_transpose_f32_low_width");
    }
    {
        ctx->map_transpose_f32_small = clCreateKernel(prog,
                                                      "map_transpose_f32_small",
                                                      &error);
        OPENCL_SUCCEED_FATAL(error);
        if (ctx->debugging)
            fprintf(stderr, "Created kernel %s.\n", "map_transpose_f32_small");
    }
    {
        ctx->replicate_14945 = clCreateKernel(prog, "replicate_14945", &error);
        OPENCL_SUCCEED_FATAL(error);
        if (ctx->debugging)
            fprintf(stderr, "Created kernel %s.\n", "replicate_14945");
    }
    {
        ctx->replicate_14950 = clCreateKernel(prog, "replicate_14950", &error);
        OPENCL_SUCCEED_FATAL(error);
        if (ctx->debugging)
            fprintf(stderr, "Created kernel %s.\n", "replicate_14950");
    }
    {
        ctx->segmap_14729 = clCreateKernel(prog, "segmap_14729", &error);
        OPENCL_SUCCEED_FATAL(error);
        if (ctx->debugging)
            fprintf(stderr, "Created kernel %s.\n", "segmap_14729");
    }
    {
        ctx->segmap_14754 = clCreateKernel(prog, "segmap_14754", &error);
        OPENCL_SUCCEED_FATAL(error);
        if (ctx->debugging)
            fprintf(stderr, "Created kernel %s.\n", "segmap_14754");
    }
    {
        ctx->segred_nonseg_14798 = clCreateKernel(prog, "segred_nonseg_14798",
                                                  &error);
        OPENCL_SUCCEED_FATAL(error);
        if (ctx->debugging)
            fprintf(stderr, "Created kernel %s.\n", "segred_nonseg_14798");
    }
    {
        cl_int success;
        
        ctx->counter_mem_14900.references = NULL;
        ctx->counter_mem_14900.size = 0;
        ctx->counter_mem_14900.mem = clCreateBuffer(ctx->opencl.ctx,
                                                    CL_MEM_READ_WRITE, (10 >
                                                                        0 ? 10 : 1) *
                                                    sizeof(int32_t), NULL,
                                                    &success);
        OPENCL_SUCCEED_OR_RETURN(success);
        if (10 > 0)
            OPENCL_SUCCEED_OR_RETURN(clEnqueueWriteBuffer(ctx->opencl.queue,
                                                          ctx->counter_mem_14900.mem,
                                                          CL_TRUE, 0, 10 *
                                                          sizeof(int32_t),
                                                          counter_mem_realtype_14969,
                                                          0, NULL, NULL));
    }
    ctx->sizes.mainzigroup_sizze_14948 = cfg->sizes[0];
    ctx->sizes.mainzigroup_sizze_14953 = cfg->sizes[1];
    ctx->sizes.mainzisegmap_group_sizze_14731 = cfg->sizes[2];
    ctx->sizes.mainzisegmap_group_sizze_14756 = cfg->sizes[3];
    ctx->sizes.mainzisegred_group_sizze_14779 = cfg->sizes[4];
    ctx->sizes.mainzisegred_max_num_groups_14782 = cfg->sizes[5];
    return 0;
}
struct futhark_context *futhark_context_new(struct futhark_context_config *cfg)
{
    struct futhark_context *ctx =
                           (struct futhark_context *) malloc(sizeof(struct futhark_context));
    
    if (ctx == NULL)
        return NULL;
    
    int required_types = 0;
    
    init_context_early(cfg, ctx);
    
    cl_program prog = setup_opencl(&ctx->opencl, opencl_program, required_types,
                                   cfg->build_opts);
    
    init_context_late(cfg, ctx, prog);
    return ctx;
}
struct futhark_context *futhark_context_new_with_command_queue(struct futhark_context_config *cfg,
                                                               cl_command_queue queue)
{
    struct futhark_context *ctx =
                           (struct futhark_context *) malloc(sizeof(struct futhark_context));
    
    if (ctx == NULL)
        return NULL;
    
    int required_types = 0;
    
    init_context_early(cfg, ctx);
    
    cl_program prog = setup_opencl_with_command_queue(&ctx->opencl, queue,
                                                      opencl_program,
                                                      required_types,
                                                      cfg->build_opts);
    
    init_context_late(cfg, ctx, prog);
    return ctx;
}
void futhark_context_free(struct futhark_context *ctx)
{
    free_lock(&ctx->lock);
    free(ctx);
}
int futhark_context_sync(struct futhark_context *ctx)
{
    ctx->error = OPENCL_SUCCEED_NONFATAL(clFinish(ctx->opencl.queue));
    return ctx->error != NULL;
}
char *futhark_context_get_error(struct futhark_context *ctx)
{
    char *error = ctx->error;
    
    ctx->error = NULL;
    return error;
}
int futhark_context_clear_caches(struct futhark_context *ctx)
{
    ctx->error = OPENCL_SUCCEED_NONFATAL(opencl_free_all(&ctx->opencl));
    return ctx->error != NULL;
}
cl_command_queue futhark_context_get_command_queue(struct futhark_context *ctx)
{
    return ctx->opencl.queue;
}
static int memblock_unref_device(struct futhark_context *ctx,
                                 struct memblock_device *block, const
                                 char *desc)
{
    if (block->references != NULL) {
        *block->references -= 1;
        if (ctx->detail_memory)
            fprintf(stderr,
                    "Unreferencing block %s (allocated as %s) in %s: %d references remaining.\n",
                    desc, block->desc, "space 'device'", *block->references);
        if (*block->references == 0) {
            ctx->cur_mem_usage_device -= block->size;
            OPENCL_SUCCEED_OR_RETURN(opencl_free(&ctx->opencl, block->mem,
                                                 block->desc));
            free(block->references);
            if (ctx->detail_memory)
                fprintf(stderr,
                        "%lld bytes freed (now allocated: %lld bytes)\n",
                        (long long) block->size,
                        (long long) ctx->cur_mem_usage_device);
        }
        block->references = NULL;
    }
    return 0;
}
static int memblock_alloc_device(struct futhark_context *ctx,
                                 struct memblock_device *block, int64_t size,
                                 const char *desc)
{
    if (size < 0)
        panic(1, "Negative allocation of %lld bytes attempted for %s in %s.\n",
              (long long) size, desc, "space 'device'",
              ctx->cur_mem_usage_device);
    
    int ret = memblock_unref_device(ctx, block, desc);
    
    ctx->cur_mem_usage_device += size;
    if (ctx->detail_memory)
        fprintf(stderr,
                "Allocating %lld bytes for %s in %s (then allocated: %lld bytes)",
                (long long) size, desc, "space 'device'",
                (long long) ctx->cur_mem_usage_device);
    if (ctx->cur_mem_usage_device > ctx->peak_mem_usage_device) {
        ctx->peak_mem_usage_device = ctx->cur_mem_usage_device;
        if (ctx->detail_memory)
            fprintf(stderr, " (new peak).\n");
    } else if (ctx->detail_memory)
        fprintf(stderr, ".\n");
    OPENCL_SUCCEED_OR_RETURN(opencl_alloc(&ctx->opencl, size, desc,
                                          &block->mem));
    block->references = (int *) malloc(sizeof(int));
    *block->references = 1;
    block->size = size;
    block->desc = desc;
    return ret;
}
static int memblock_set_device(struct futhark_context *ctx,
                               struct memblock_device *lhs,
                               struct memblock_device *rhs, const
                               char *lhs_desc)
{
    int ret = memblock_unref_device(ctx, lhs, lhs_desc);
    
    (*rhs->references)++;
    *lhs = *rhs;
    return ret;
}
static int memblock_unref(struct futhark_context *ctx, struct memblock *block,
                          const char *desc)
{
    if (block->references != NULL) {
        *block->references -= 1;
        if (ctx->detail_memory)
            fprintf(stderr,
                    "Unreferencing block %s (allocated as %s) in %s: %d references remaining.\n",
                    desc, block->desc, "default space", *block->references);
        if (*block->references == 0) {
            ctx->cur_mem_usage_default -= block->size;
            free(block->mem);
            free(block->references);
            if (ctx->detail_memory)
                fprintf(stderr,
                        "%lld bytes freed (now allocated: %lld bytes)\n",
                        (long long) block->size,
                        (long long) ctx->cur_mem_usage_default);
        }
        block->references = NULL;
    }
    return 0;
}
static int memblock_alloc(struct futhark_context *ctx, struct memblock *block,
                          int64_t size, const char *desc)
{
    if (size < 0)
        panic(1, "Negative allocation of %lld bytes attempted for %s in %s.\n",
              (long long) size, desc, "default space",
              ctx->cur_mem_usage_default);
    
    int ret = memblock_unref(ctx, block, desc);
    
    ctx->cur_mem_usage_default += size;
    if (ctx->detail_memory)
        fprintf(stderr,
                "Allocating %lld bytes for %s in %s (then allocated: %lld bytes)",
                (long long) size, desc, "default space",
                (long long) ctx->cur_mem_usage_default);
    if (ctx->cur_mem_usage_default > ctx->peak_mem_usage_default) {
        ctx->peak_mem_usage_default = ctx->cur_mem_usage_default;
        if (ctx->detail_memory)
            fprintf(stderr, " (new peak).\n");
    } else if (ctx->detail_memory)
        fprintf(stderr, ".\n");
    block->mem = (char *) malloc(size);
    block->references = (int *) malloc(sizeof(int));
    *block->references = 1;
    block->size = size;
    block->desc = desc;
    return ret;
}
static int memblock_set(struct futhark_context *ctx, struct memblock *lhs,
                        struct memblock *rhs, const char *lhs_desc)
{
    int ret = memblock_unref(ctx, lhs, lhs_desc);
    
    (*rhs->references)++;
    *lhs = *rhs;
    return ret;
}
void futhark_debugging_report(struct futhark_context *ctx)
{
    if (ctx->detail_memory) {
        fprintf(stderr, "Peak memory usage for space 'device': %lld bytes.\n",
                (long long) ctx->peak_mem_usage_device);
        fprintf(stderr, "Peak memory usage for default space: %lld bytes.\n",
                (long long) ctx->peak_mem_usage_default);
    }
    if (ctx->debugging) {
        fprintf(stderr,
                "Kernel map_transpose_f32            executed %6d times, with average runtime: %6ldus\tand total runtime: %6ldus\n",
                ctx->map_transpose_f32_runs,
                (long) ctx->map_transpose_f32_total_runtime /
                (ctx->map_transpose_f32_runs !=
                 0 ? ctx->map_transpose_f32_runs : 1),
                (long) ctx->map_transpose_f32_total_runtime);
        ctx->total_runtime += ctx->map_transpose_f32_total_runtime;
        ctx->total_runs += ctx->map_transpose_f32_runs;
        fprintf(stderr,
                "Kernel map_transpose_f32_low_height executed %6d times, with average runtime: %6ldus\tand total runtime: %6ldus\n",
                ctx->map_transpose_f32_low_height_runs,
                (long) ctx->map_transpose_f32_low_height_total_runtime /
                (ctx->map_transpose_f32_low_height_runs !=
                 0 ? ctx->map_transpose_f32_low_height_runs : 1),
                (long) ctx->map_transpose_f32_low_height_total_runtime);
        ctx->total_runtime += ctx->map_transpose_f32_low_height_total_runtime;
        ctx->total_runs += ctx->map_transpose_f32_low_height_runs;
        fprintf(stderr,
                "Kernel map_transpose_f32_low_width  executed %6d times, with average runtime: %6ldus\tand total runtime: %6ldus\n",
                ctx->map_transpose_f32_low_width_runs,
                (long) ctx->map_transpose_f32_low_width_total_runtime /
                (ctx->map_transpose_f32_low_width_runs !=
                 0 ? ctx->map_transpose_f32_low_width_runs : 1),
                (long) ctx->map_transpose_f32_low_width_total_runtime);
        ctx->total_runtime += ctx->map_transpose_f32_low_width_total_runtime;
        ctx->total_runs += ctx->map_transpose_f32_low_width_runs;
        fprintf(stderr,
                "Kernel map_transpose_f32_small      executed %6d times, with average runtime: %6ldus\tand total runtime: %6ldus\n",
                ctx->map_transpose_f32_small_runs,
                (long) ctx->map_transpose_f32_small_total_runtime /
                (ctx->map_transpose_f32_small_runs !=
                 0 ? ctx->map_transpose_f32_small_runs : 1),
                (long) ctx->map_transpose_f32_small_total_runtime);
        ctx->total_runtime += ctx->map_transpose_f32_small_total_runtime;
        ctx->total_runs += ctx->map_transpose_f32_small_runs;
        fprintf(stderr,
                "Kernel replicate_14945              executed %6d times, with average runtime: %6ldus\tand total runtime: %6ldus\n",
                ctx->replicate_14945_runs,
                (long) ctx->replicate_14945_total_runtime /
                (ctx->replicate_14945_runs !=
                 0 ? ctx->replicate_14945_runs : 1),
                (long) ctx->replicate_14945_total_runtime);
        ctx->total_runtime += ctx->replicate_14945_total_runtime;
        ctx->total_runs += ctx->replicate_14945_runs;
        fprintf(stderr,
                "Kernel replicate_14950              executed %6d times, with average runtime: %6ldus\tand total runtime: %6ldus\n",
                ctx->replicate_14950_runs,
                (long) ctx->replicate_14950_total_runtime /
                (ctx->replicate_14950_runs !=
                 0 ? ctx->replicate_14950_runs : 1),
                (long) ctx->replicate_14950_total_runtime);
        ctx->total_runtime += ctx->replicate_14950_total_runtime;
        ctx->total_runs += ctx->replicate_14950_runs;
        fprintf(stderr,
                "Kernel segmap_14729                 executed %6d times, with average runtime: %6ldus\tand total runtime: %6ldus\n",
                ctx->segmap_14729_runs, (long) ctx->segmap_14729_total_runtime /
                (ctx->segmap_14729_runs != 0 ? ctx->segmap_14729_runs : 1),
                (long) ctx->segmap_14729_total_runtime);
        ctx->total_runtime += ctx->segmap_14729_total_runtime;
        ctx->total_runs += ctx->segmap_14729_runs;
        fprintf(stderr,
                "Kernel segmap_14754                 executed %6d times, with average runtime: %6ldus\tand total runtime: %6ldus\n",
                ctx->segmap_14754_runs, (long) ctx->segmap_14754_total_runtime /
                (ctx->segmap_14754_runs != 0 ? ctx->segmap_14754_runs : 1),
                (long) ctx->segmap_14754_total_runtime);
        ctx->total_runtime += ctx->segmap_14754_total_runtime;
        ctx->total_runs += ctx->segmap_14754_runs;
        fprintf(stderr,
                "Kernel segred_nonseg_14798          executed %6d times, with average runtime: %6ldus\tand total runtime: %6ldus\n",
                ctx->segred_nonseg_14798_runs,
                (long) ctx->segred_nonseg_14798_total_runtime /
                (ctx->segred_nonseg_14798_runs !=
                 0 ? ctx->segred_nonseg_14798_runs : 1),
                (long) ctx->segred_nonseg_14798_total_runtime);
        ctx->total_runtime += ctx->segred_nonseg_14798_total_runtime;
        ctx->total_runs += ctx->segred_nonseg_14798_runs;
        if (ctx->debugging)
            fprintf(stderr, "Ran %d kernels with cumulative runtime: %6ldus\n",
                    ctx->total_runs, ctx->total_runtime);
    }
}
static int futrts_main(struct futhark_context *ctx,
                       struct memblock_device *out_mem_p_14955,
                       int32_t *out_out_arrsizze_14956,
                       int32_t *out_out_arrsizze_14957,
                       struct memblock_device sPositions_mem_14843,
                       struct memblock_device sRadii_mem_14844,
                       struct memblock_device sColors_mem_14845,
                       int32_t s_14578, int32_t s_14579, int32_t s_14580,
                       int32_t sizze_14581, int32_t sizze_14582,
                       int32_t width_14583, int32_t height_14584,
                       int32_t numS_14585, int32_t numL_14586);
static int futrts__map_transpose_f32(struct futhark_context *ctx,
                                     struct memblock_device destmem_0,
                                     int32_t destoffset_1,
                                     struct memblock_device srcmem_2,
                                     int32_t srcoffset_3, int32_t num_arrays_4,
                                     int32_t x_elems_5, int32_t y_elems_6,
                                     int32_t in_elems_7, int32_t out_elems_8);
static inline int8_t add8(int8_t x, int8_t y)
{
    return x + y;
}
static inline int16_t add16(int16_t x, int16_t y)
{
    return x + y;
}
static inline int32_t add32(int32_t x, int32_t y)
{
    return x + y;
}
static inline int64_t add64(int64_t x, int64_t y)
{
    return x + y;
}
static inline int8_t sub8(int8_t x, int8_t y)
{
    return x - y;
}
static inline int16_t sub16(int16_t x, int16_t y)
{
    return x - y;
}
static inline int32_t sub32(int32_t x, int32_t y)
{
    return x - y;
}
static inline int64_t sub64(int64_t x, int64_t y)
{
    return x - y;
}
static inline int8_t mul8(int8_t x, int8_t y)
{
    return x * y;
}
static inline int16_t mul16(int16_t x, int16_t y)
{
    return x * y;
}
static inline int32_t mul32(int32_t x, int32_t y)
{
    return x * y;
}
static inline int64_t mul64(int64_t x, int64_t y)
{
    return x * y;
}
static inline uint8_t udiv8(uint8_t x, uint8_t y)
{
    return x / y;
}
static inline uint16_t udiv16(uint16_t x, uint16_t y)
{
    return x / y;
}
static inline uint32_t udiv32(uint32_t x, uint32_t y)
{
    return x / y;
}
static inline uint64_t udiv64(uint64_t x, uint64_t y)
{
    return x / y;
}
static inline uint8_t umod8(uint8_t x, uint8_t y)
{
    return x % y;
}
static inline uint16_t umod16(uint16_t x, uint16_t y)
{
    return x % y;
}
static inline uint32_t umod32(uint32_t x, uint32_t y)
{
    return x % y;
}
static inline uint64_t umod64(uint64_t x, uint64_t y)
{
    return x % y;
}
static inline int8_t sdiv8(int8_t x, int8_t y)
{
    int8_t q = x / y;
    int8_t r = x % y;
    
    return q - ((r != 0 && r < 0 != y < 0) ? 1 : 0);
}
static inline int16_t sdiv16(int16_t x, int16_t y)
{
    int16_t q = x / y;
    int16_t r = x % y;
    
    return q - ((r != 0 && r < 0 != y < 0) ? 1 : 0);
}
static inline int32_t sdiv32(int32_t x, int32_t y)
{
    int32_t q = x / y;
    int32_t r = x % y;
    
    return q - ((r != 0 && r < 0 != y < 0) ? 1 : 0);
}
static inline int64_t sdiv64(int64_t x, int64_t y)
{
    int64_t q = x / y;
    int64_t r = x % y;
    
    return q - ((r != 0 && r < 0 != y < 0) ? 1 : 0);
}
static inline int8_t smod8(int8_t x, int8_t y)
{
    int8_t r = x % y;
    
    return r + (r == 0 || (x > 0 && y > 0) || (x < 0 && y < 0) ? 0 : y);
}
static inline int16_t smod16(int16_t x, int16_t y)
{
    int16_t r = x % y;
    
    return r + (r == 0 || (x > 0 && y > 0) || (x < 0 && y < 0) ? 0 : y);
}
static inline int32_t smod32(int32_t x, int32_t y)
{
    int32_t r = x % y;
    
    return r + (r == 0 || (x > 0 && y > 0) || (x < 0 && y < 0) ? 0 : y);
}
static inline int64_t smod64(int64_t x, int64_t y)
{
    int64_t r = x % y;
    
    return r + (r == 0 || (x > 0 && y > 0) || (x < 0 && y < 0) ? 0 : y);
}
static inline int8_t squot8(int8_t x, int8_t y)
{
    return x / y;
}
static inline int16_t squot16(int16_t x, int16_t y)
{
    return x / y;
}
static inline int32_t squot32(int32_t x, int32_t y)
{
    return x / y;
}
static inline int64_t squot64(int64_t x, int64_t y)
{
    return x / y;
}
static inline int8_t srem8(int8_t x, int8_t y)
{
    return x % y;
}
static inline int16_t srem16(int16_t x, int16_t y)
{
    return x % y;
}
static inline int32_t srem32(int32_t x, int32_t y)
{
    return x % y;
}
static inline int64_t srem64(int64_t x, int64_t y)
{
    return x % y;
}
static inline int8_t smin8(int8_t x, int8_t y)
{
    return x < y ? x : y;
}
static inline int16_t smin16(int16_t x, int16_t y)
{
    return x < y ? x : y;
}
static inline int32_t smin32(int32_t x, int32_t y)
{
    return x < y ? x : y;
}
static inline int64_t smin64(int64_t x, int64_t y)
{
    return x < y ? x : y;
}
static inline uint8_t umin8(uint8_t x, uint8_t y)
{
    return x < y ? x : y;
}
static inline uint16_t umin16(uint16_t x, uint16_t y)
{
    return x < y ? x : y;
}
static inline uint32_t umin32(uint32_t x, uint32_t y)
{
    return x < y ? x : y;
}
static inline uint64_t umin64(uint64_t x, uint64_t y)
{
    return x < y ? x : y;
}
static inline int8_t smax8(int8_t x, int8_t y)
{
    return x < y ? y : x;
}
static inline int16_t smax16(int16_t x, int16_t y)
{
    return x < y ? y : x;
}
static inline int32_t smax32(int32_t x, int32_t y)
{
    return x < y ? y : x;
}
static inline int64_t smax64(int64_t x, int64_t y)
{
    return x < y ? y : x;
}
static inline uint8_t umax8(uint8_t x, uint8_t y)
{
    return x < y ? y : x;
}
static inline uint16_t umax16(uint16_t x, uint16_t y)
{
    return x < y ? y : x;
}
static inline uint32_t umax32(uint32_t x, uint32_t y)
{
    return x < y ? y : x;
}
static inline uint64_t umax64(uint64_t x, uint64_t y)
{
    return x < y ? y : x;
}
static inline uint8_t shl8(uint8_t x, uint8_t y)
{
    return x << y;
}
static inline uint16_t shl16(uint16_t x, uint16_t y)
{
    return x << y;
}
static inline uint32_t shl32(uint32_t x, uint32_t y)
{
    return x << y;
}
static inline uint64_t shl64(uint64_t x, uint64_t y)
{
    return x << y;
}
static inline uint8_t lshr8(uint8_t x, uint8_t y)
{
    return x >> y;
}
static inline uint16_t lshr16(uint16_t x, uint16_t y)
{
    return x >> y;
}
static inline uint32_t lshr32(uint32_t x, uint32_t y)
{
    return x >> y;
}
static inline uint64_t lshr64(uint64_t x, uint64_t y)
{
    return x >> y;
}
static inline int8_t ashr8(int8_t x, int8_t y)
{
    return x >> y;
}
static inline int16_t ashr16(int16_t x, int16_t y)
{
    return x >> y;
}
static inline int32_t ashr32(int32_t x, int32_t y)
{
    return x >> y;
}
static inline int64_t ashr64(int64_t x, int64_t y)
{
    return x >> y;
}
static inline uint8_t and8(uint8_t x, uint8_t y)
{
    return x & y;
}
static inline uint16_t and16(uint16_t x, uint16_t y)
{
    return x & y;
}
static inline uint32_t and32(uint32_t x, uint32_t y)
{
    return x & y;
}
static inline uint64_t and64(uint64_t x, uint64_t y)
{
    return x & y;
}
static inline uint8_t or8(uint8_t x, uint8_t y)
{
    return x | y;
}
static inline uint16_t or16(uint16_t x, uint16_t y)
{
    return x | y;
}
static inline uint32_t or32(uint32_t x, uint32_t y)
{
    return x | y;
}
static inline uint64_t or64(uint64_t x, uint64_t y)
{
    return x | y;
}
static inline uint8_t xor8(uint8_t x, uint8_t y)
{
    return x ^ y;
}
static inline uint16_t xor16(uint16_t x, uint16_t y)
{
    return x ^ y;
}
static inline uint32_t xor32(uint32_t x, uint32_t y)
{
    return x ^ y;
}
static inline uint64_t xor64(uint64_t x, uint64_t y)
{
    return x ^ y;
}
static inline char ult8(uint8_t x, uint8_t y)
{
    return x < y;
}
static inline char ult16(uint16_t x, uint16_t y)
{
    return x < y;
}
static inline char ult32(uint32_t x, uint32_t y)
{
    return x < y;
}
static inline char ult64(uint64_t x, uint64_t y)
{
    return x < y;
}
static inline char ule8(uint8_t x, uint8_t y)
{
    return x <= y;
}
static inline char ule16(uint16_t x, uint16_t y)
{
    return x <= y;
}
static inline char ule32(uint32_t x, uint32_t y)
{
    return x <= y;
}
static inline char ule64(uint64_t x, uint64_t y)
{
    return x <= y;
}
static inline char slt8(int8_t x, int8_t y)
{
    return x < y;
}
static inline char slt16(int16_t x, int16_t y)
{
    return x < y;
}
static inline char slt32(int32_t x, int32_t y)
{
    return x < y;
}
static inline char slt64(int64_t x, int64_t y)
{
    return x < y;
}
static inline char sle8(int8_t x, int8_t y)
{
    return x <= y;
}
static inline char sle16(int16_t x, int16_t y)
{
    return x <= y;
}
static inline char sle32(int32_t x, int32_t y)
{
    return x <= y;
}
static inline char sle64(int64_t x, int64_t y)
{
    return x <= y;
}
static inline int8_t pow8(int8_t x, int8_t y)
{
    int8_t res = 1, rem = y;
    
    while (rem != 0) {
        if (rem & 1)
            res *= x;
        rem >>= 1;
        x *= x;
    }
    return res;
}
static inline int16_t pow16(int16_t x, int16_t y)
{
    int16_t res = 1, rem = y;
    
    while (rem != 0) {
        if (rem & 1)
            res *= x;
        rem >>= 1;
        x *= x;
    }
    return res;
}
static inline int32_t pow32(int32_t x, int32_t y)
{
    int32_t res = 1, rem = y;
    
    while (rem != 0) {
        if (rem & 1)
            res *= x;
        rem >>= 1;
        x *= x;
    }
    return res;
}
static inline int64_t pow64(int64_t x, int64_t y)
{
    int64_t res = 1, rem = y;
    
    while (rem != 0) {
        if (rem & 1)
            res *= x;
        rem >>= 1;
        x *= x;
    }
    return res;
}
static inline bool itob_i8_bool(int8_t x)
{
    return x;
}
static inline bool itob_i16_bool(int16_t x)
{
    return x;
}
static inline bool itob_i32_bool(int32_t x)
{
    return x;
}
static inline bool itob_i64_bool(int64_t x)
{
    return x;
}
static inline int8_t btoi_bool_i8(bool x)
{
    return x;
}
static inline int16_t btoi_bool_i16(bool x)
{
    return x;
}
static inline int32_t btoi_bool_i32(bool x)
{
    return x;
}
static inline int64_t btoi_bool_i64(bool x)
{
    return x;
}
#define sext_i8_i8(x) ((int8_t) (int8_t) x)
#define sext_i8_i16(x) ((int16_t) (int8_t) x)
#define sext_i8_i32(x) ((int32_t) (int8_t) x)
#define sext_i8_i64(x) ((int64_t) (int8_t) x)
#define sext_i16_i8(x) ((int8_t) (int16_t) x)
#define sext_i16_i16(x) ((int16_t) (int16_t) x)
#define sext_i16_i32(x) ((int32_t) (int16_t) x)
#define sext_i16_i64(x) ((int64_t) (int16_t) x)
#define sext_i32_i8(x) ((int8_t) (int32_t) x)
#define sext_i32_i16(x) ((int16_t) (int32_t) x)
#define sext_i32_i32(x) ((int32_t) (int32_t) x)
#define sext_i32_i64(x) ((int64_t) (int32_t) x)
#define sext_i64_i8(x) ((int8_t) (int64_t) x)
#define sext_i64_i16(x) ((int16_t) (int64_t) x)
#define sext_i64_i32(x) ((int32_t) (int64_t) x)
#define sext_i64_i64(x) ((int64_t) (int64_t) x)
#define zext_i8_i8(x) ((uint8_t) (uint8_t) x)
#define zext_i8_i16(x) ((uint16_t) (uint8_t) x)
#define zext_i8_i32(x) ((uint32_t) (uint8_t) x)
#define zext_i8_i64(x) ((uint64_t) (uint8_t) x)
#define zext_i16_i8(x) ((uint8_t) (uint16_t) x)
#define zext_i16_i16(x) ((uint16_t) (uint16_t) x)
#define zext_i16_i32(x) ((uint32_t) (uint16_t) x)
#define zext_i16_i64(x) ((uint64_t) (uint16_t) x)
#define zext_i32_i8(x) ((uint8_t) (uint32_t) x)
#define zext_i32_i16(x) ((uint16_t) (uint32_t) x)
#define zext_i32_i32(x) ((uint32_t) (uint32_t) x)
#define zext_i32_i64(x) ((uint64_t) (uint32_t) x)
#define zext_i64_i8(x) ((uint8_t) (uint64_t) x)
#define zext_i64_i16(x) ((uint16_t) (uint64_t) x)
#define zext_i64_i32(x) ((uint32_t) (uint64_t) x)
#define zext_i64_i64(x) ((uint64_t) (uint64_t) x)
static inline float fdiv32(float x, float y)
{
    return x / y;
}
static inline float fadd32(float x, float y)
{
    return x + y;
}
static inline float fsub32(float x, float y)
{
    return x - y;
}
static inline float fmul32(float x, float y)
{
    return x * y;
}
static inline float fmin32(float x, float y)
{
    return x < y ? x : y;
}
static inline float fmax32(float x, float y)
{
    return x < y ? y : x;
}
static inline float fpow32(float x, float y)
{
    return pow(x, y);
}
static inline char cmplt32(float x, float y)
{
    return x < y;
}
static inline char cmple32(float x, float y)
{
    return x <= y;
}
static inline float sitofp_i8_f32(int8_t x)
{
    return x;
}
static inline float sitofp_i16_f32(int16_t x)
{
    return x;
}
static inline float sitofp_i32_f32(int32_t x)
{
    return x;
}
static inline float sitofp_i64_f32(int64_t x)
{
    return x;
}
static inline float uitofp_i8_f32(uint8_t x)
{
    return x;
}
static inline float uitofp_i16_f32(uint16_t x)
{
    return x;
}
static inline float uitofp_i32_f32(uint32_t x)
{
    return x;
}
static inline float uitofp_i64_f32(uint64_t x)
{
    return x;
}
static inline int8_t fptosi_f32_i8(float x)
{
    return x;
}
static inline int16_t fptosi_f32_i16(float x)
{
    return x;
}
static inline int32_t fptosi_f32_i32(float x)
{
    return x;
}
static inline int64_t fptosi_f32_i64(float x)
{
    return x;
}
static inline uint8_t fptoui_f32_i8(float x)
{
    return x;
}
static inline uint16_t fptoui_f32_i16(float x)
{
    return x;
}
static inline uint32_t fptoui_f32_i32(float x)
{
    return x;
}
static inline uint64_t fptoui_f32_i64(float x)
{
    return x;
}
static inline double fdiv64(double x, double y)
{
    return x / y;
}
static inline double fadd64(double x, double y)
{
    return x + y;
}
static inline double fsub64(double x, double y)
{
    return x - y;
}
static inline double fmul64(double x, double y)
{
    return x * y;
}
static inline double fmin64(double x, double y)
{
    return x < y ? x : y;
}
static inline double fmax64(double x, double y)
{
    return x < y ? y : x;
}
static inline double fpow64(double x, double y)
{
    return pow(x, y);
}
static inline char cmplt64(double x, double y)
{
    return x < y;
}
static inline char cmple64(double x, double y)
{
    return x <= y;
}
static inline double sitofp_i8_f64(int8_t x)
{
    return x;
}
static inline double sitofp_i16_f64(int16_t x)
{
    return x;
}
static inline double sitofp_i32_f64(int32_t x)
{
    return x;
}
static inline double sitofp_i64_f64(int64_t x)
{
    return x;
}
static inline double uitofp_i8_f64(uint8_t x)
{
    return x;
}
static inline double uitofp_i16_f64(uint16_t x)
{
    return x;
}
static inline double uitofp_i32_f64(uint32_t x)
{
    return x;
}
static inline double uitofp_i64_f64(uint64_t x)
{
    return x;
}
static inline int8_t fptosi_f64_i8(double x)
{
    return x;
}
static inline int16_t fptosi_f64_i16(double x)
{
    return x;
}
static inline int32_t fptosi_f64_i32(double x)
{
    return x;
}
static inline int64_t fptosi_f64_i64(double x)
{
    return x;
}
static inline uint8_t fptoui_f64_i8(double x)
{
    return x;
}
static inline uint16_t fptoui_f64_i16(double x)
{
    return x;
}
static inline uint32_t fptoui_f64_i32(double x)
{
    return x;
}
static inline uint64_t fptoui_f64_i64(double x)
{
    return x;
}
static inline float fpconv_f32_f32(float x)
{
    return x;
}
static inline double fpconv_f32_f64(float x)
{
    return x;
}
static inline float fpconv_f64_f32(double x)
{
    return x;
}
static inline double fpconv_f64_f64(double x)
{
    return x;
}
static inline float futrts_log32(float x)
{
    return log(x);
}
static inline float futrts_log2_32(float x)
{
    return log2(x);
}
static inline float futrts_log10_32(float x)
{
    return log10(x);
}
static inline float futrts_sqrt32(float x)
{
    return sqrt(x);
}
static inline float futrts_exp32(float x)
{
    return exp(x);
}
static inline float futrts_cos32(float x)
{
    return cos(x);
}
static inline float futrts_sin32(float x)
{
    return sin(x);
}
static inline float futrts_tan32(float x)
{
    return tan(x);
}
static inline float futrts_acos32(float x)
{
    return acos(x);
}
static inline float futrts_asin32(float x)
{
    return asin(x);
}
static inline float futrts_atan32(float x)
{
    return atan(x);
}
static inline float futrts_atan2_32(float x, float y)
{
    return atan2(x, y);
}
static inline float futrts_gamma32(float x)
{
    return tgamma(x);
}
static inline float futrts_lgamma32(float x)
{
    return lgamma(x);
}
static inline float futrts_round32(float x)
{
    return rint(x);
}
static inline char futrts_isnan32(float x)
{
    return isnan(x);
}
static inline char futrts_isinf32(float x)
{
    return isinf(x);
}
static inline int32_t futrts_to_bits32(float x)
{
    union {
        float f;
        int32_t t;
    } p;
    
    p.f = x;
    return p.t;
}
static inline float futrts_from_bits32(int32_t x)
{
    union {
        int32_t f;
        float t;
    } p;
    
    p.f = x;
    return p.t;
}
static inline double futrts_log64(double x)
{
    return log(x);
}
static inline double futrts_log2_64(double x)
{
    return log2(x);
}
static inline double futrts_log10_64(double x)
{
    return log10(x);
}
static inline double futrts_sqrt64(double x)
{
    return sqrt(x);
}
static inline double futrts_exp64(double x)
{
    return exp(x);
}
static inline double futrts_cos64(double x)
{
    return cos(x);
}
static inline double futrts_sin64(double x)
{
    return sin(x);
}
static inline double futrts_tan64(double x)
{
    return tan(x);
}
static inline double futrts_acos64(double x)
{
    return acos(x);
}
static inline double futrts_asin64(double x)
{
    return asin(x);
}
static inline double futrts_atan64(double x)
{
    return atan(x);
}
static inline double futrts_atan2_64(double x, double y)
{
    return atan2(x, y);
}
static inline double futrts_gamma64(double x)
{
    return tgamma(x);
}
static inline double futrts_lgamma64(double x)
{
    return lgamma(x);
}
static inline double futrts_round64(double x)
{
    return rint(x);
}
static inline char futrts_isnan64(double x)
{
    return isnan(x);
}
static inline char futrts_isinf64(double x)
{
    return isinf(x);
}
static inline int64_t futrts_to_bits64(double x)
{
    union {
        double f;
        int64_t t;
    } p;
    
    p.f = x;
    return p.t;
}
static inline double futrts_from_bits64(int64_t x)
{
    union {
        int64_t f;
        double t;
    } p;
    
    p.f = x;
    return p.t;
}
static int futrts_main(struct futhark_context *ctx,
                       struct memblock_device *out_mem_p_14955,
                       int32_t *out_out_arrsizze_14956,
                       int32_t *out_out_arrsizze_14957,
                       struct memblock_device sPositions_mem_14843,
                       struct memblock_device sRadii_mem_14844,
                       struct memblock_device sColors_mem_14845,
                       int32_t s_14578, int32_t s_14579, int32_t s_14580,
                       int32_t sizze_14581, int32_t sizze_14582,
                       int32_t width_14583, int32_t height_14584,
                       int32_t numS_14585, int32_t numL_14586)
{
    struct memblock_device out_mem_14885;
    
    out_mem_14885.references = NULL;
    
    int32_t out_arrsizze_14886;
    int32_t out_arrsizze_14887;
    bool dim_zzero_14590 = 0 == s_14578;
    bool dim_zzero_14591 = 0 == sizze_14581;
    bool old_empty_14592 = dim_zzero_14590 || dim_zzero_14591;
    bool both_empty_14593 = dim_zzero_14590 && old_empty_14592;
    bool dim_match_14594 = 3 == sizze_14581;
    bool empty_or_match_14595 = both_empty_14593 || dim_match_14594;
    bool empty_or_match_cert_14596;
    
    if (!empty_or_match_14595) {
        ctx->error = msgprintf("Error at %s:\n%s\n", "test.fut:69:1-85:44",
                               "function arguments of wrong shape");
        if (memblock_unref_device(ctx, &out_mem_14885, "out_mem_14885") != 0)
            return 1;
        return 1;
    }
    
    bool dim_zzero_14597 = 0 == s_14579;
    bool both_empty_14598 = dim_zzero_14590 && dim_zzero_14597;
    bool dim_match_14599 = s_14578 == s_14579;
    bool empty_or_match_14600 = both_empty_14598 || dim_match_14599;
    bool empty_or_match_cert_14601;
    
    if (!empty_or_match_14600) {
        ctx->error = msgprintf("Error at %s:\n%s\n", "test.fut:69:1-85:44",
                               "function arguments of wrong shape");
        if (memblock_unref_device(ctx, &out_mem_14885, "out_mem_14885") != 0)
            return 1;
        return 1;
    }
    
    bool dim_zzero_14602 = 0 == s_14580;
    bool dim_zzero_14603 = 0 == sizze_14582;
    bool old_empty_14604 = dim_zzero_14602 || dim_zzero_14603;
    bool both_empty_14605 = dim_zzero_14590 && old_empty_14604;
    bool dim_match_14606 = s_14578 == s_14580;
    bool dim_match_14607 = 4 == sizze_14582;
    bool match_14608 = dim_match_14606 && dim_match_14607;
    bool empty_or_match_14609 = both_empty_14605 || match_14608;
    bool empty_or_match_cert_14610;
    
    if (!empty_or_match_14609) {
        ctx->error = msgprintf("Error at %s:\n%s\n", "test.fut:69:1-85:44",
                               "function arguments of wrong shape");
        if (memblock_unref_device(ctx, &out_mem_14885, "out_mem_14885") != 0)
            return 1;
        return 1;
    }
    
    int32_t totalS_14611 = numS_14585 + numL_14586;
    bool empty_slice_14612 = numS_14585 == 0;
    int32_t m_14613 = numS_14585 - 1;
    bool zzero_leq_i_p_m_t_s_14614 = sle32(0, m_14613);
    bool i_p_m_t_s_leq_w_14615 = slt32(m_14613, s_14578);
    bool i_lte_j_14616 = sle32(0, numS_14585);
    bool y_14617 = zzero_leq_i_p_m_t_s_14614 && i_p_m_t_s_leq_w_14615;
    bool y_14618 = i_lte_j_14616 && y_14617;
    bool ok_or_empty_14619 = empty_slice_14612 || y_14618;
    bool index_certs_14620;
    
    if (!ok_or_empty_14619) {
        ctx->error = msgprintf("Error at %s:\n%s%d%s%d%s%d%s\n",
                               "test.fut:69:1-85:44 -> test.fut:82:9-28",
                               "Index [", 0, ":", numS_14585,
                               "] out of bounds for array of shape [", s_14578,
                               "].");
        if (memblock_unref_device(ctx, &out_mem_14885, "out_mem_14885") != 0)
            return 1;
        return 1;
    }
    
    bool empty_slice_14623 = numL_14586 == 0;
    int32_t m_14624 = numL_14586 - 1;
    int32_t i_p_m_t_s_14625 = numS_14585 + m_14624;
    bool zzero_leq_i_p_m_t_s_14626 = sle32(0, i_p_m_t_s_14625);
    bool i_p_m_t_s_leq_w_14627 = slt32(i_p_m_t_s_14625, s_14578);
    bool i_lte_j_14628 = sle32(numS_14585, totalS_14611);
    bool y_14629 = i_lte_j_14616 && i_p_m_t_s_leq_w_14627;
    bool y_14630 = zzero_leq_i_p_m_t_s_14626 && y_14629;
    bool y_14631 = i_lte_j_14628 && y_14630;
    bool forwards_ok_14632 = i_lte_j_14616 && y_14631;
    bool ok_or_empty_14633 = empty_slice_14623 || forwards_ok_14632;
    bool index_certs_14634;
    
    if (!ok_or_empty_14633) {
        ctx->error = msgprintf("Error at %s:\n%s%d%s%d%s%d%s\n",
                               "test.fut:69:1-85:44 -> test.fut:84:9-33",
                               "Index [", numS_14585, ":", totalS_14611,
                               "] out of bounds for array of shape [", s_14578,
                               "].");
        if (memblock_unref_device(ctx, &out_mem_14885, "out_mem_14885") != 0)
            return 1;
        return 1;
    }
    
    int32_t iota_arg_14637 = width_14583 * height_14584;
    int32_t segmap_group_sizze_14732;
    
    segmap_group_sizze_14732 = ctx->sizes.mainzisegmap_group_sizze_14731;
    
    int32_t y_14733 = segmap_group_sizze_14732 - 1;
    int32_t x_14734 = numS_14585 + y_14733;
    int32_t segmap_usable_groups_14735 = squot32(x_14734,
                                                 segmap_group_sizze_14732);
    int32_t convop_x_14847 = s_14578 * sizze_14581;
    int64_t binop_x_14848 = sext_i32_i64(convop_x_14847);
    int64_t bytes_14846 = 4 * binop_x_14848;
    struct memblock_device mem_14849;
    
    mem_14849.references = NULL;
    if (memblock_alloc_device(ctx, &mem_14849, bytes_14846, "mem_14849"))
        return 1;
    
    int call_ret_14958 = futrts__map_transpose_f32(ctx, mem_14849, 0,
                                                   sPositions_mem_14843, 0, 1,
                                                   sizze_14581, s_14578,
                                                   s_14578 * sizze_14581,
                                                   s_14578 * sizze_14581);
    
    assert(call_ret_14958 == 0);
    
    int64_t binop_x_14851 = sext_i32_i64(numS_14585);
    int64_t bytes_14850 = 4 * binop_x_14851;
    struct memblock_device mem_14852;
    
    mem_14852.references = NULL;
    if (memblock_alloc_device(ctx, &mem_14852, bytes_14850, "mem_14852"))
        return 1;
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segmap_14729, 0,
                                            sizeof(s_14578), &s_14578));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segmap_14729, 1,
                                            sizeof(numS_14585), &numS_14585));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segmap_14729, 2,
                                            sizeof(sRadii_mem_14844.mem),
                                            &sRadii_mem_14844.mem));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segmap_14729, 3,
                                            sizeof(mem_14849.mem),
                                            &mem_14849.mem));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segmap_14729, 4,
                                            sizeof(mem_14852.mem),
                                            &mem_14852.mem));
    if (1 * (segmap_usable_groups_14735 * segmap_group_sizze_14732) != 0) {
        const size_t global_work_sizze_14959[1] = {segmap_usable_groups_14735 *
                     segmap_group_sizze_14732};
        const size_t local_work_sizze_14963[1] = {segmap_group_sizze_14732};
        int64_t time_start_14960 = 0, time_end_14961 = 0;
        
        if (ctx->debugging) {
            fprintf(stderr, "Launching %s with global work size [",
                    "segmap_14729");
            fprintf(stderr, "%zu", global_work_sizze_14959[0]);
            fprintf(stderr, "] and local work size [");
            fprintf(stderr, "%zu", local_work_sizze_14963[0]);
            fprintf(stderr, "].\n");
            time_start_14960 = get_wall_time();
        }
        OPENCL_SUCCEED_OR_RETURN(clEnqueueNDRangeKernel(ctx->opencl.queue,
                                                        ctx->segmap_14729, 1,
                                                        NULL,
                                                        global_work_sizze_14959,
                                                        local_work_sizze_14963,
                                                        0, NULL, NULL));
        if (ctx->debugging) {
            OPENCL_SUCCEED_FATAL(clFinish(ctx->opencl.queue));
            time_end_14961 = get_wall_time();
            
            long time_diff_14962 = time_end_14961 - time_start_14960;
            
            ctx->segmap_14729_total_runtime += time_diff_14962;
            ctx->segmap_14729_runs++;
            fprintf(stderr, "kernel %s runtime: %ldus\n", "segmap_14729",
                    time_diff_14962);
        }
    }
    if (memblock_unref_device(ctx, &mem_14849, "mem_14849") != 0)
        return 1;
    
    int32_t segmap_group_sizze_14757;
    
    segmap_group_sizze_14757 = ctx->sizes.mainzisegmap_group_sizze_14756;
    
    int32_t y_14758 = segmap_group_sizze_14757 - 1;
    int32_t x_14759 = numL_14586 + y_14758;
    int32_t segmap_usable_groups_14760 = squot32(x_14759,
                                                 segmap_group_sizze_14757);
    int64_t binop_x_14854 = sext_i32_i64(numL_14586);
    int64_t bytes_14853 = 4 * binop_x_14854;
    struct memblock_device mem_14855;
    
    mem_14855.references = NULL;
    if (memblock_alloc_device(ctx, &mem_14855, bytes_14853, "mem_14855"))
        return 1;
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segmap_14754, 0,
                                            sizeof(sizze_14581), &sizze_14581));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segmap_14754, 1,
                                            sizeof(numS_14585), &numS_14585));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segmap_14754, 2,
                                            sizeof(numL_14586), &numL_14586));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segmap_14754, 3,
                                            sizeof(sPositions_mem_14843.mem),
                                            &sPositions_mem_14843.mem));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segmap_14754, 4,
                                            sizeof(sRadii_mem_14844.mem),
                                            &sRadii_mem_14844.mem));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segmap_14754, 5,
                                            sizeof(mem_14855.mem),
                                            &mem_14855.mem));
    if (1 * (segmap_usable_groups_14760 * segmap_group_sizze_14757) != 0) {
        const size_t global_work_sizze_14964[1] = {segmap_usable_groups_14760 *
                     segmap_group_sizze_14757};
        const size_t local_work_sizze_14968[1] = {segmap_group_sizze_14757};
        int64_t time_start_14965 = 0, time_end_14966 = 0;
        
        if (ctx->debugging) {
            fprintf(stderr, "Launching %s with global work size [",
                    "segmap_14754");
            fprintf(stderr, "%zu", global_work_sizze_14964[0]);
            fprintf(stderr, "] and local work size [");
            fprintf(stderr, "%zu", local_work_sizze_14968[0]);
            fprintf(stderr, "].\n");
            time_start_14965 = get_wall_time();
        }
        OPENCL_SUCCEED_OR_RETURN(clEnqueueNDRangeKernel(ctx->opencl.queue,
                                                        ctx->segmap_14754, 1,
                                                        NULL,
                                                        global_work_sizze_14964,
                                                        local_work_sizze_14968,
                                                        0, NULL, NULL));
        if (ctx->debugging) {
            OPENCL_SUCCEED_FATAL(clFinish(ctx->opencl.queue));
            time_end_14966 = get_wall_time();
            
            long time_diff_14967 = time_end_14966 - time_start_14965;
            
            ctx->segmap_14754_total_runtime += time_diff_14967;
            ctx->segmap_14754_runs++;
            fprintf(stderr, "kernel %s runtime: %ldus\n", "segmap_14754",
                    time_diff_14967);
        }
    }
    
    int32_t segred_group_sizze_14780;
    
    segred_group_sizze_14780 = ctx->sizes.mainzisegred_group_sizze_14779;
    
    int64_t segred_group_sizze_14781 = sext_i32_i64(segred_group_sizze_14780);
    int32_t segred_max_num_groups_14783;
    
    segred_max_num_groups_14783 = ctx->sizes.mainzisegred_max_num_groups_14782;
    
    int64_t segred_max_num_groups_14784 =
            sext_i32_i64(segred_max_num_groups_14783);
    int64_t nest_sizze_14785 = sext_i32_i64(totalS_14611);
    int64_t y_14786 = segred_group_sizze_14781 - 1;
    int64_t x_14787 = nest_sizze_14785 + y_14786;
    int64_t w_div_group_sizze_14788 = squot64(x_14787,
                                              segred_group_sizze_14781);
    int64_t num_groups_maybe_zzero_14789 = smin64(segred_max_num_groups_14784,
                                                  w_div_group_sizze_14788);
    int64_t num_groups_14790 = smax64(1, num_groups_maybe_zzero_14789);
    int32_t num_groups_14792 = sext_i64_i32(num_groups_14790);
    struct memblock_device mem_14859;
    
    mem_14859.references = NULL;
    if (memblock_alloc_device(ctx, &mem_14859, 4, "mem_14859"))
        return 1;
    
    struct memblock_device mem_14861;
    
    mem_14861.references = NULL;
    if (memblock_alloc_device(ctx, &mem_14861, 1, "mem_14861"))
        return 1;
    
    struct memblock_device mem_14864;
    
    mem_14864.references = NULL;
    if (memblock_alloc_device(ctx, &mem_14864, 4, "mem_14864"))
        return 1;
    
    struct memblock_device counter_mem_14900 = ctx->counter_mem_14900;
    struct memblock_device group_res_arr_mem_14902;
    
    group_res_arr_mem_14902.references = NULL;
    if (memblock_alloc_device(ctx, &group_res_arr_mem_14902, sizeof(int32_t) *
                              (segred_group_sizze_14780 * num_groups_14792),
                              "group_res_arr_mem_14902"))
        return 1;
    
    struct memblock_device group_res_arr_mem_14904;
    
    group_res_arr_mem_14904.references = NULL;
    if (memblock_alloc_device(ctx, &group_res_arr_mem_14904, sizeof(int8_t) *
                              (segred_group_sizze_14780 * num_groups_14792),
                              "group_res_arr_mem_14904"))
        return 1;
    
    struct memblock_device group_res_arr_mem_14906;
    
    group_res_arr_mem_14906.references = NULL;
    if (memblock_alloc_device(ctx, &group_res_arr_mem_14906, sizeof(float) *
                              (segred_group_sizze_14780 * num_groups_14792),
                              "group_res_arr_mem_14906"))
        return 1;
    
    int32_t num_threads_14908 = num_groups_14792 * segred_group_sizze_14780;
    
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segred_nonseg_14798, 0,
                                            sizeof(numS_14585), &numS_14585));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segred_nonseg_14798, 1,
                                            sizeof(totalS_14611),
                                            &totalS_14611));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segred_nonseg_14798, 2,
                                            sizeof(num_groups_14792),
                                            &num_groups_14792));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segred_nonseg_14798, 3,
                                            sizeof(mem_14852.mem),
                                            &mem_14852.mem));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segred_nonseg_14798, 4,
                                            sizeof(mem_14855.mem),
                                            &mem_14855.mem));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segred_nonseg_14798, 5,
                                            sizeof(mem_14859.mem),
                                            &mem_14859.mem));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segred_nonseg_14798, 6,
                                            sizeof(mem_14861.mem),
                                            &mem_14861.mem));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segred_nonseg_14798, 7,
                                            sizeof(mem_14864.mem),
                                            &mem_14864.mem));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segred_nonseg_14798, 8,
                                            sizeof(counter_mem_14900.mem),
                                            &counter_mem_14900.mem));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segred_nonseg_14798, 9,
                                            sizeof(group_res_arr_mem_14902.mem),
                                            &group_res_arr_mem_14902.mem));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segred_nonseg_14798, 10,
                                            sizeof(group_res_arr_mem_14904.mem),
                                            &group_res_arr_mem_14904.mem));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->segred_nonseg_14798, 11,
                                            sizeof(group_res_arr_mem_14906.mem),
                                            &group_res_arr_mem_14906.mem));
    if (1 * (num_groups_14792 * segred_group_sizze_14780) != 0) {
        const size_t global_work_sizze_14970[1] = {num_groups_14792 *
                     segred_group_sizze_14780};
        const size_t local_work_sizze_14974[1] = {segred_group_sizze_14780};
        int64_t time_start_14971 = 0, time_end_14972 = 0;
        
        if (ctx->debugging) {
            fprintf(stderr, "Launching %s with global work size [",
                    "segred_nonseg_14798");
            fprintf(stderr, "%zu", global_work_sizze_14970[0]);
            fprintf(stderr, "] and local work size [");
            fprintf(stderr, "%zu", local_work_sizze_14974[0]);
            fprintf(stderr, "].\n");
            time_start_14971 = get_wall_time();
        }
        OPENCL_SUCCEED_OR_RETURN(clEnqueueNDRangeKernel(ctx->opencl.queue,
                                                        ctx->segred_nonseg_14798,
                                                        1, NULL,
                                                        global_work_sizze_14970,
                                                        local_work_sizze_14974,
                                                        0, NULL, NULL));
        if (ctx->debugging) {
            OPENCL_SUCCEED_FATAL(clFinish(ctx->opencl.queue));
            time_end_14972 = get_wall_time();
            
            long time_diff_14973 = time_end_14972 - time_start_14971;
            
            ctx->segred_nonseg_14798_total_runtime += time_diff_14973;
            ctx->segred_nonseg_14798_runs++;
            fprintf(stderr, "kernel %s runtime: %ldus\n", "segred_nonseg_14798",
                    time_diff_14973);
        }
    }
    if (memblock_unref_device(ctx, &mem_14852, "mem_14852") != 0)
        return 1;
    if (memblock_unref_device(ctx, &mem_14855, "mem_14855") != 0)
        return 1;
    if (memblock_unref_device(ctx, &mem_14864, "mem_14864") != 0)
        return 1;
    
    int32_t read_res_14975;
    
    OPENCL_SUCCEED_OR_RETURN(clEnqueueReadBuffer(ctx->opencl.queue,
                                                 mem_14859.mem, CL_TRUE, 0 *
                                                 sizeof(int32_t),
                                                 sizeof(int32_t),
                                                 &read_res_14975, 0, NULL,
                                                 NULL));
    
    int32_t res_14703 = read_res_14975;
    
    if (memblock_unref_device(ctx, &mem_14859, "mem_14859") != 0)
        return 1;
    
    int8_t read_res_14976;
    
    OPENCL_SUCCEED_OR_RETURN(clEnqueueReadBuffer(ctx->opencl.queue,
                                                 mem_14861.mem, CL_TRUE, 0 *
                                                 sizeof(int8_t), sizeof(int8_t),
                                                 &read_res_14976, 0, NULL,
                                                 NULL));
    
    int8_t res_14704 = read_res_14976;
    
    if (memblock_unref_device(ctx, &mem_14861, "mem_14861") != 0)
        return 1;
    
    bool cond_14719 = res_14704 == 1;
    bool cond_14720 = res_14704 == 2;
    struct memblock_device res_mem_14874;
    
    res_mem_14874.references = NULL;
    if (cond_14719) {
        struct memblock_device mem_14866;
        
        mem_14866.references = NULL;
        if (memblock_alloc_device(ctx, &mem_14866, 4, "mem_14866"))
            return 1;
        if (4 * sizeof(int8_t) > 0) {
            OPENCL_SUCCEED_OR_RETURN(clEnqueueCopyBuffer(ctx->opencl.queue,
                                                         sColors_mem_14845.mem,
                                                         mem_14866.mem,
                                                         res_14703 *
                                                         sizze_14582, 0, 4 *
                                                         sizeof(int8_t), 0,
                                                         NULL, NULL));
            if (ctx->debugging)
                OPENCL_SUCCEED_FATAL(clFinish(ctx->opencl.queue));
        }
        if (memblock_set_device(ctx, &res_mem_14874, &mem_14866, "mem_14866") !=
            0)
            return 1;
        if (memblock_unref_device(ctx, &mem_14866, "mem_14866") != 0)
            return 1;
    } else {
        struct memblock_device res_mem_14873;
        
        res_mem_14873.references = NULL;
        if (cond_14720) {
            int32_t j_p_i_t_s_14724 = numS_14585 + res_14703;
            struct memblock_device mem_14869;
            
            mem_14869.references = NULL;
            if (memblock_alloc_device(ctx, &mem_14869, 4, "mem_14869"))
                return 1;
            if (4 * sizeof(int8_t) > 0) {
                OPENCL_SUCCEED_OR_RETURN(clEnqueueCopyBuffer(ctx->opencl.queue,
                                                             sColors_mem_14845.mem,
                                                             mem_14869.mem,
                                                             j_p_i_t_s_14724 *
                                                             sizze_14582, 0, 4 *
                                                             sizeof(int8_t), 0,
                                                             NULL, NULL));
                if (ctx->debugging)
                    OPENCL_SUCCEED_FATAL(clFinish(ctx->opencl.queue));
            }
            if (memblock_set_device(ctx, &res_mem_14873, &mem_14869,
                                    "mem_14869") != 0)
                return 1;
            if (memblock_unref_device(ctx, &mem_14869, "mem_14869") != 0)
                return 1;
        } else {
            struct memblock_device mem_14872;
            
            mem_14872.references = NULL;
            if (memblock_alloc_device(ctx, &mem_14872, 4, "mem_14872"))
                return 1;
            
            int32_t group_sizze_14948;
            
            group_sizze_14948 = ctx->sizes.mainzigroup_sizze_14948;
            
            int32_t num_groups_14949;
            
            num_groups_14949 = squot32(4 + sext_i32_i32(group_sizze_14948) - 1,
                                       sext_i32_i32(group_sizze_14948));
            OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->replicate_14945, 0,
                                                    sizeof(mem_14872.mem),
                                                    &mem_14872.mem));
            if (1 * (num_groups_14949 * group_sizze_14948) != 0) {
                const size_t global_work_sizze_14977[1] = {num_groups_14949 *
                             group_sizze_14948};
                const size_t local_work_sizze_14981[1] = {group_sizze_14948};
                int64_t time_start_14978 = 0, time_end_14979 = 0;
                
                if (ctx->debugging) {
                    fprintf(stderr, "Launching %s with global work size [",
                            "replicate_14945");
                    fprintf(stderr, "%zu", global_work_sizze_14977[0]);
                    fprintf(stderr, "] and local work size [");
                    fprintf(stderr, "%zu", local_work_sizze_14981[0]);
                    fprintf(stderr, "].\n");
                    time_start_14978 = get_wall_time();
                }
                OPENCL_SUCCEED_OR_RETURN(clEnqueueNDRangeKernel(ctx->opencl.queue,
                                                                ctx->replicate_14945,
                                                                1, NULL,
                                                                global_work_sizze_14977,
                                                                local_work_sizze_14981,
                                                                0, NULL, NULL));
                if (ctx->debugging) {
                    OPENCL_SUCCEED_FATAL(clFinish(ctx->opencl.queue));
                    time_end_14979 = get_wall_time();
                    
                    long time_diff_14980 = time_end_14979 - time_start_14978;
                    
                    ctx->replicate_14945_total_runtime += time_diff_14980;
                    ctx->replicate_14945_runs++;
                    fprintf(stderr, "kernel %s runtime: %ldus\n",
                            "replicate_14945", time_diff_14980);
                }
            }
            if (memblock_set_device(ctx, &res_mem_14873, &mem_14872,
                                    "mem_14872") != 0)
                return 1;
            if (memblock_unref_device(ctx, &mem_14872, "mem_14872") != 0)
                return 1;
        }
        if (memblock_set_device(ctx, &res_mem_14874, &res_mem_14873,
                                "res_mem_14873") != 0)
            return 1;
        if (memblock_unref_device(ctx, &res_mem_14873, "res_mem_14873") != 0)
            return 1;
    }
    
    int64_t binop_x_14876 = sext_i32_i64(iota_arg_14637);
    int64_t bytes_14875 = 4 * binop_x_14876;
    struct memblock_device mem_14878;
    
    mem_14878.references = NULL;
    if (memblock_alloc_device(ctx, &mem_14878, bytes_14875, "mem_14878"))
        return 1;
    
    int32_t group_sizze_14953;
    
    group_sizze_14953 = ctx->sizes.mainzigroup_sizze_14953;
    
    int32_t num_groups_14954;
    
    num_groups_14954 = squot32(iota_arg_14637 * 4 +
                               sext_i32_i32(group_sizze_14953) - 1,
                               sext_i32_i32(group_sizze_14953));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->replicate_14950, 0,
                                            sizeof(iota_arg_14637),
                                            &iota_arg_14637));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->replicate_14950, 1,
                                            sizeof(res_mem_14874.mem),
                                            &res_mem_14874.mem));
    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->replicate_14950, 2,
                                            sizeof(mem_14878.mem),
                                            &mem_14878.mem));
    if (1 * (num_groups_14954 * group_sizze_14953) != 0) {
        const size_t global_work_sizze_14982[1] = {num_groups_14954 *
                     group_sizze_14953};
        const size_t local_work_sizze_14986[1] = {group_sizze_14953};
        int64_t time_start_14983 = 0, time_end_14984 = 0;
        
        if (ctx->debugging) {
            fprintf(stderr, "Launching %s with global work size [",
                    "replicate_14950");
            fprintf(stderr, "%zu", global_work_sizze_14982[0]);
            fprintf(stderr, "] and local work size [");
            fprintf(stderr, "%zu", local_work_sizze_14986[0]);
            fprintf(stderr, "].\n");
            time_start_14983 = get_wall_time();
        }
        OPENCL_SUCCEED_OR_RETURN(clEnqueueNDRangeKernel(ctx->opencl.queue,
                                                        ctx->replicate_14950, 1,
                                                        NULL,
                                                        global_work_sizze_14982,
                                                        local_work_sizze_14986,
                                                        0, NULL, NULL));
        if (ctx->debugging) {
            OPENCL_SUCCEED_FATAL(clFinish(ctx->opencl.queue));
            time_end_14984 = get_wall_time();
            
            long time_diff_14985 = time_end_14984 - time_start_14983;
            
            ctx->replicate_14950_total_runtime += time_diff_14985;
            ctx->replicate_14950_runs++;
            fprintf(stderr, "kernel %s runtime: %ldus\n", "replicate_14950",
                    time_diff_14985);
        }
    }
    if (memblock_unref_device(ctx, &res_mem_14874, "res_mem_14874") != 0)
        return 1;
    out_arrsizze_14886 = iota_arg_14637;
    out_arrsizze_14887 = 4;
    if (memblock_set_device(ctx, &out_mem_14885, &mem_14878, "mem_14878") != 0)
        return 1;
    (*out_mem_p_14955).references = NULL;
    if (memblock_set_device(ctx, &*out_mem_p_14955, &out_mem_14885,
                            "out_mem_14885") != 0)
        return 1;
    *out_out_arrsizze_14956 = out_arrsizze_14886;
    *out_out_arrsizze_14957 = out_arrsizze_14887;
    if (memblock_unref_device(ctx, &mem_14878, "mem_14878") != 0)
        return 1;
    if (memblock_unref_device(ctx, &res_mem_14874, "res_mem_14874") != 0)
        return 1;
    if (memblock_unref_device(ctx, &group_res_arr_mem_14906,
                              "group_res_arr_mem_14906") != 0)
        return 1;
    if (memblock_unref_device(ctx, &group_res_arr_mem_14904,
                              "group_res_arr_mem_14904") != 0)
        return 1;
    if (memblock_unref_device(ctx, &group_res_arr_mem_14902,
                              "group_res_arr_mem_14902") != 0)
        return 1;
    if (memblock_unref_device(ctx, &mem_14864, "mem_14864") != 0)
        return 1;
    if (memblock_unref_device(ctx, &mem_14861, "mem_14861") != 0)
        return 1;
    if (memblock_unref_device(ctx, &mem_14859, "mem_14859") != 0)
        return 1;
    if (memblock_unref_device(ctx, &mem_14855, "mem_14855") != 0)
        return 1;
    if (memblock_unref_device(ctx, &mem_14852, "mem_14852") != 0)
        return 1;
    if (memblock_unref_device(ctx, &mem_14849, "mem_14849") != 0)
        return 1;
    if (memblock_unref_device(ctx, &out_mem_14885, "out_mem_14885") != 0)
        return 1;
    return 0;
}
static int futrts__map_transpose_f32(struct futhark_context *ctx,
                                     struct memblock_device destmem_0,
                                     int32_t destoffset_1,
                                     struct memblock_device srcmem_2,
                                     int32_t srcoffset_3, int32_t num_arrays_4,
                                     int32_t x_elems_5, int32_t y_elems_6,
                                     int32_t in_elems_7, int32_t out_elems_8)
{
    if (!(num_arrays_4 == 0 || (x_elems_5 == 0 || y_elems_6 == 0))) {
        int32_t muly_10 = squot32(16, x_elems_5);
        int32_t mulx_9 = squot32(16, y_elems_6);
        
        if (in_elems_7 == out_elems_8 && ((num_arrays_4 == 1 || x_elems_5 *
                                           y_elems_6 == in_elems_7) &&
                                          (x_elems_5 == 1 || y_elems_6 == 1))) {
            if (in_elems_7 * sizeof(float) > 0) {
                OPENCL_SUCCEED_OR_RETURN(clEnqueueCopyBuffer(ctx->opencl.queue,
                                                             srcmem_2.mem,
                                                             destmem_0.mem,
                                                             srcoffset_3,
                                                             destoffset_1,
                                                             in_elems_7 *
                                                             sizeof(float), 0,
                                                             NULL, NULL));
                if (ctx->debugging)
                    OPENCL_SUCCEED_FATAL(clFinish(ctx->opencl.queue));
            }
        } else {
            if (sle32(x_elems_5, 8) && slt32(16, y_elems_6)) {
                OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_width,
                                                        0, sizeof(destoffset_1),
                                                        &destoffset_1));
                OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_width,
                                                        1, sizeof(srcoffset_3),
                                                        &srcoffset_3));
                OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_width,
                                                        2, sizeof(num_arrays_4),
                                                        &num_arrays_4));
                OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_width,
                                                        3, sizeof(x_elems_5),
                                                        &x_elems_5));
                OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_width,
                                                        4, sizeof(y_elems_6),
                                                        &y_elems_6));
                OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_width,
                                                        5, sizeof(in_elems_7),
                                                        &in_elems_7));
                OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_width,
                                                        6, sizeof(out_elems_8),
                                                        &out_elems_8));
                OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_width,
                                                        7, sizeof(mulx_9),
                                                        &mulx_9));
                OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_width,
                                                        8, sizeof(muly_10),
                                                        &muly_10));
                OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_width,
                                                        9,
                                                        sizeof(destmem_0.mem),
                                                        &destmem_0.mem));
                OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_width,
                                                        10,
                                                        sizeof(srcmem_2.mem),
                                                        &srcmem_2.mem));
                if (1 * (squot32(x_elems_5 + 16 - 1, 16) * 16) *
                    (squot32(squot32(y_elems_6 + muly_10 - 1, muly_10) + 16 - 1,
                             16) * 16) * (num_arrays_4 * 1) != 0) {
                    const size_t global_work_sizze_14987[3] =
                                 {squot32(x_elems_5 + 16 - 1, 16) * 16,
                                  squot32(squot32(y_elems_6 + muly_10 - 1,
                                                  muly_10) + 16 - 1, 16) * 16,
                                  num_arrays_4 * 1};
                    const size_t local_work_sizze_14991[3] = {16, 16, 1};
                    int64_t time_start_14988 = 0, time_end_14989 = 0;
                    
                    if (ctx->debugging) {
                        fprintf(stderr, "Launching %s with global work size [",
                                "map_transpose_f32_low_width");
                        fprintf(stderr, "%zu", global_work_sizze_14987[0]);
                        fprintf(stderr, ", ");
                        fprintf(stderr, "%zu", global_work_sizze_14987[1]);
                        fprintf(stderr, ", ");
                        fprintf(stderr, "%zu", global_work_sizze_14987[2]);
                        fprintf(stderr, "] and local work size [");
                        fprintf(stderr, "%zu", local_work_sizze_14991[0]);
                        fprintf(stderr, ", ");
                        fprintf(stderr, "%zu", local_work_sizze_14991[1]);
                        fprintf(stderr, ", ");
                        fprintf(stderr, "%zu", local_work_sizze_14991[2]);
                        fprintf(stderr, "].\n");
                        time_start_14988 = get_wall_time();
                    }
                    OPENCL_SUCCEED_OR_RETURN(clEnqueueNDRangeKernel(ctx->opencl.queue,
                                                                    ctx->map_transpose_f32_low_width,
                                                                    3, NULL,
                                                                    global_work_sizze_14987,
                                                                    local_work_sizze_14991,
                                                                    0, NULL,
                                                                    NULL));
                    if (ctx->debugging) {
                        OPENCL_SUCCEED_FATAL(clFinish(ctx->opencl.queue));
                        time_end_14989 = get_wall_time();
                        
                        long time_diff_14990 = time_end_14989 -
                             time_start_14988;
                        
                        ctx->map_transpose_f32_low_width_total_runtime +=
                            time_diff_14990;
                        ctx->map_transpose_f32_low_width_runs++;
                        fprintf(stderr, "kernel %s runtime: %ldus\n",
                                "map_transpose_f32_low_width", time_diff_14990);
                    }
                }
            } else {
                if (sle32(y_elems_6, 8) && slt32(16, x_elems_5)) {
                    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_height,
                                                            0,
                                                            sizeof(destoffset_1),
                                                            &destoffset_1));
                    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_height,
                                                            1,
                                                            sizeof(srcoffset_3),
                                                            &srcoffset_3));
                    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_height,
                                                            2,
                                                            sizeof(num_arrays_4),
                                                            &num_arrays_4));
                    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_height,
                                                            3,
                                                            sizeof(x_elems_5),
                                                            &x_elems_5));
                    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_height,
                                                            4,
                                                            sizeof(y_elems_6),
                                                            &y_elems_6));
                    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_height,
                                                            5,
                                                            sizeof(in_elems_7),
                                                            &in_elems_7));
                    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_height,
                                                            6,
                                                            sizeof(out_elems_8),
                                                            &out_elems_8));
                    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_height,
                                                            7, sizeof(mulx_9),
                                                            &mulx_9));
                    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_height,
                                                            8, sizeof(muly_10),
                                                            &muly_10));
                    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_height,
                                                            9,
                                                            sizeof(destmem_0.mem),
                                                            &destmem_0.mem));
                    OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_low_height,
                                                            10,
                                                            sizeof(srcmem_2.mem),
                                                            &srcmem_2.mem));
                    if (1 * (squot32(squot32(x_elems_5 + mulx_9 - 1, mulx_9) +
                                     16 - 1, 16) * 16) * (squot32(y_elems_6 +
                                                                  16 - 1, 16) *
                                                          16) * (num_arrays_4 *
                                                                 1) != 0) {
                        const size_t global_work_sizze_14992[3] =
                                     {squot32(squot32(x_elems_5 + mulx_9 - 1,
                                                      mulx_9) + 16 - 1, 16) *
                                      16, squot32(y_elems_6 + 16 - 1, 16) * 16,
                                      num_arrays_4 * 1};
                        const size_t local_work_sizze_14996[3] = {16, 16, 1};
                        int64_t time_start_14993 = 0, time_end_14994 = 0;
                        
                        if (ctx->debugging) {
                            fprintf(stderr,
                                    "Launching %s with global work size [",
                                    "map_transpose_f32_low_height");
                            fprintf(stderr, "%zu", global_work_sizze_14992[0]);
                            fprintf(stderr, ", ");
                            fprintf(stderr, "%zu", global_work_sizze_14992[1]);
                            fprintf(stderr, ", ");
                            fprintf(stderr, "%zu", global_work_sizze_14992[2]);
                            fprintf(stderr, "] and local work size [");
                            fprintf(stderr, "%zu", local_work_sizze_14996[0]);
                            fprintf(stderr, ", ");
                            fprintf(stderr, "%zu", local_work_sizze_14996[1]);
                            fprintf(stderr, ", ");
                            fprintf(stderr, "%zu", local_work_sizze_14996[2]);
                            fprintf(stderr, "].\n");
                            time_start_14993 = get_wall_time();
                        }
                        OPENCL_SUCCEED_OR_RETURN(clEnqueueNDRangeKernel(ctx->opencl.queue,
                                                                        ctx->map_transpose_f32_low_height,
                                                                        3, NULL,
                                                                        global_work_sizze_14992,
                                                                        local_work_sizze_14996,
                                                                        0, NULL,
                                                                        NULL));
                        if (ctx->debugging) {
                            OPENCL_SUCCEED_FATAL(clFinish(ctx->opencl.queue));
                            time_end_14994 = get_wall_time();
                            
                            long time_diff_14995 = time_end_14994 -
                                 time_start_14993;
                            
                            ctx->map_transpose_f32_low_height_total_runtime +=
                                time_diff_14995;
                            ctx->map_transpose_f32_low_height_runs++;
                            fprintf(stderr, "kernel %s runtime: %ldus\n",
                                    "map_transpose_f32_low_height",
                                    time_diff_14995);
                        }
                    }
                } else {
                    if (sle32(x_elems_5, 8) && sle32(y_elems_6, 8)) {
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_small,
                                                                0,
                                                                sizeof(destoffset_1),
                                                                &destoffset_1));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_small,
                                                                1,
                                                                sizeof(srcoffset_3),
                                                                &srcoffset_3));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_small,
                                                                2,
                                                                sizeof(num_arrays_4),
                                                                &num_arrays_4));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_small,
                                                                3,
                                                                sizeof(x_elems_5),
                                                                &x_elems_5));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_small,
                                                                4,
                                                                sizeof(y_elems_6),
                                                                &y_elems_6));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_small,
                                                                5,
                                                                sizeof(in_elems_7),
                                                                &in_elems_7));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_small,
                                                                6,
                                                                sizeof(out_elems_8),
                                                                &out_elems_8));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_small,
                                                                7,
                                                                sizeof(mulx_9),
                                                                &mulx_9));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_small,
                                                                8,
                                                                sizeof(muly_10),
                                                                &muly_10));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_small,
                                                                9,
                                                                sizeof(destmem_0.mem),
                                                                &destmem_0.mem));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32_small,
                                                                10,
                                                                sizeof(srcmem_2.mem),
                                                                &srcmem_2.mem));
                        if (1 * (squot32(num_arrays_4 * x_elems_5 * y_elems_6 +
                                         256 - 1, 256) * 256) != 0) {
                            const size_t global_work_sizze_14997[1] =
                                         {squot32(num_arrays_4 * x_elems_5 *
                                                  y_elems_6 + 256 - 1, 256) *
                                         256};
                            const size_t local_work_sizze_15001[1] = {256};
                            int64_t time_start_14998 = 0, time_end_14999 = 0;
                            
                            if (ctx->debugging) {
                                fprintf(stderr,
                                        "Launching %s with global work size [",
                                        "map_transpose_f32_small");
                                fprintf(stderr, "%zu",
                                        global_work_sizze_14997[0]);
                                fprintf(stderr, "] and local work size [");
                                fprintf(stderr, "%zu",
                                        local_work_sizze_15001[0]);
                                fprintf(stderr, "].\n");
                                time_start_14998 = get_wall_time();
                            }
                            OPENCL_SUCCEED_OR_RETURN(clEnqueueNDRangeKernel(ctx->opencl.queue,
                                                                            ctx->map_transpose_f32_small,
                                                                            1,
                                                                            NULL,
                                                                            global_work_sizze_14997,
                                                                            local_work_sizze_15001,
                                                                            0,
                                                                            NULL,
                                                                            NULL));
                            if (ctx->debugging) {
                                OPENCL_SUCCEED_FATAL(clFinish(ctx->opencl.queue));
                                time_end_14999 = get_wall_time();
                                
                                long time_diff_15000 = time_end_14999 -
                                     time_start_14998;
                                
                                ctx->map_transpose_f32_small_total_runtime +=
                                    time_diff_15000;
                                ctx->map_transpose_f32_small_runs++;
                                fprintf(stderr, "kernel %s runtime: %ldus\n",
                                        "map_transpose_f32_small",
                                        time_diff_15000);
                            }
                        }
                    } else {
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32,
                                                                0,
                                                                sizeof(destoffset_1),
                                                                &destoffset_1));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32,
                                                                1,
                                                                sizeof(srcoffset_3),
                                                                &srcoffset_3));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32,
                                                                2,
                                                                sizeof(num_arrays_4),
                                                                &num_arrays_4));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32,
                                                                3,
                                                                sizeof(x_elems_5),
                                                                &x_elems_5));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32,
                                                                4,
                                                                sizeof(y_elems_6),
                                                                &y_elems_6));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32,
                                                                5,
                                                                sizeof(in_elems_7),
                                                                &in_elems_7));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32,
                                                                6,
                                                                sizeof(out_elems_8),
                                                                &out_elems_8));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32,
                                                                7,
                                                                sizeof(mulx_9),
                                                                &mulx_9));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32,
                                                                8,
                                                                sizeof(muly_10),
                                                                &muly_10));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32,
                                                                9,
                                                                sizeof(destmem_0.mem),
                                                                &destmem_0.mem));
                        OPENCL_SUCCEED_OR_RETURN(clSetKernelArg(ctx->map_transpose_f32,
                                                                10,
                                                                sizeof(srcmem_2.mem),
                                                                &srcmem_2.mem));
                        if (1 * (squot32(x_elems_5 + 32 - 1, 32) * 32) *
                            (squot32(y_elems_6 + 32 - 1, 32) * 8) *
                            (num_arrays_4 * 1) != 0) {
                            const size_t global_work_sizze_15002[3] =
                                         {squot32(x_elems_5 + 32 - 1, 32) * 32,
                                          squot32(y_elems_6 + 32 - 1, 32) * 8,
                                          num_arrays_4 * 1};
                            const size_t local_work_sizze_15006[3] = {32, 8, 1};
                            int64_t time_start_15003 = 0, time_end_15004 = 0;
                            
                            if (ctx->debugging) {
                                fprintf(stderr,
                                        "Launching %s with global work size [",
                                        "map_transpose_f32");
                                fprintf(stderr, "%zu",
                                        global_work_sizze_15002[0]);
                                fprintf(stderr, ", ");
                                fprintf(stderr, "%zu",
                                        global_work_sizze_15002[1]);
                                fprintf(stderr, ", ");
                                fprintf(stderr, "%zu",
                                        global_work_sizze_15002[2]);
                                fprintf(stderr, "] and local work size [");
                                fprintf(stderr, "%zu",
                                        local_work_sizze_15006[0]);
                                fprintf(stderr, ", ");
                                fprintf(stderr, "%zu",
                                        local_work_sizze_15006[1]);
                                fprintf(stderr, ", ");
                                fprintf(stderr, "%zu",
                                        local_work_sizze_15006[2]);
                                fprintf(stderr, "].\n");
                                time_start_15003 = get_wall_time();
                            }
                            OPENCL_SUCCEED_OR_RETURN(clEnqueueNDRangeKernel(ctx->opencl.queue,
                                                                            ctx->map_transpose_f32,
                                                                            3,
                                                                            NULL,
                                                                            global_work_sizze_15002,
                                                                            local_work_sizze_15006,
                                                                            0,
                                                                            NULL,
                                                                            NULL));
                            if (ctx->debugging) {
                                OPENCL_SUCCEED_FATAL(clFinish(ctx->opencl.queue));
                                time_end_15004 = get_wall_time();
                                
                                long time_diff_15005 = time_end_15004 -
                                     time_start_15003;
                                
                                ctx->map_transpose_f32_total_runtime +=
                                    time_diff_15005;
                                ctx->map_transpose_f32_runs++;
                                fprintf(stderr, "kernel %s runtime: %ldus\n",
                                        "map_transpose_f32", time_diff_15005);
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}
struct futhark_u8_2d {
    struct memblock_device mem;
    int64_t shape[2];
} ;
struct futhark_u8_2d *futhark_new_u8_2d(struct futhark_context *ctx,
                                        uint8_t *data, int64_t dim0,
                                        int64_t dim1)
{
    struct futhark_u8_2d *bad = NULL;
    struct futhark_u8_2d *arr =
                         (struct futhark_u8_2d *) malloc(sizeof(struct futhark_u8_2d));
    
    if (arr == NULL)
        return bad;
    lock_lock(&ctx->lock);
    arr->mem.references = NULL;
    if (memblock_alloc_device(ctx, &arr->mem, dim0 * dim1 * sizeof(uint8_t),
                              "arr->mem"))
        return NULL;
    arr->shape[0] = dim0;
    arr->shape[1] = dim1;
    if (dim0 * dim1 * sizeof(uint8_t) > 0)
        OPENCL_SUCCEED_OR_RETURN(clEnqueueWriteBuffer(ctx->opencl.queue,
                                                      arr->mem.mem, CL_TRUE, 0,
                                                      dim0 * dim1 *
                                                      sizeof(uint8_t), data + 0,
                                                      0, NULL, NULL));
    lock_unlock(&ctx->lock);
    return arr;
}
struct futhark_u8_2d *futhark_new_raw_u8_2d(struct futhark_context *ctx,
                                            cl_mem data, int offset,
                                            int64_t dim0, int64_t dim1)
{
    struct futhark_u8_2d *bad = NULL;
    struct futhark_u8_2d *arr =
                         (struct futhark_u8_2d *) malloc(sizeof(struct futhark_u8_2d));
    
    if (arr == NULL)
        return bad;
    lock_lock(&ctx->lock);
    arr->mem.references = NULL;
    if (memblock_alloc_device(ctx, &arr->mem, dim0 * dim1 * sizeof(uint8_t),
                              "arr->mem"))
        return NULL;
    arr->shape[0] = dim0;
    arr->shape[1] = dim1;
    if (dim0 * dim1 * sizeof(uint8_t) > 0) {
        OPENCL_SUCCEED_OR_RETURN(clEnqueueCopyBuffer(ctx->opencl.queue, data,
                                                     arr->mem.mem, offset, 0,
                                                     dim0 * dim1 *
                                                     sizeof(uint8_t), 0, NULL,
                                                     NULL));
        if (ctx->debugging)
            OPENCL_SUCCEED_FATAL(clFinish(ctx->opencl.queue));
    }
    lock_unlock(&ctx->lock);
    return arr;
}
int futhark_free_u8_2d(struct futhark_context *ctx, struct futhark_u8_2d *arr)
{
    lock_lock(&ctx->lock);
    if (memblock_unref_device(ctx, &arr->mem, "arr->mem") != 0)
        return 1;
    lock_unlock(&ctx->lock);
    free(arr);
    return 0;
}
int futhark_values_u8_2d(struct futhark_context *ctx, struct futhark_u8_2d *arr,
                         uint8_t *data)
{
    lock_lock(&ctx->lock);
    if (arr->shape[0] * arr->shape[1] * sizeof(uint8_t) > 0)
        OPENCL_SUCCEED_OR_RETURN(clEnqueueReadBuffer(ctx->opencl.queue,
                                                     arr->mem.mem, CL_TRUE, 0,
                                                     arr->shape[0] *
                                                     arr->shape[1] *
                                                     sizeof(uint8_t), data + 0,
                                                     0, NULL, NULL));
    lock_unlock(&ctx->lock);
    return 0;
}
cl_mem futhark_values_raw_u8_2d(struct futhark_context *ctx,
                                struct futhark_u8_2d *arr)
{
    return arr->mem.mem;
}
int64_t *futhark_shape_u8_2d(struct futhark_context *ctx,
                             struct futhark_u8_2d *arr)
{
    return arr->shape;
}
struct futhark_f32_1d {
    struct memblock_device mem;
    int64_t shape[1];
} ;
struct futhark_f32_1d *futhark_new_f32_1d(struct futhark_context *ctx,
                                          float *data, int64_t dim0)
{
    struct futhark_f32_1d *bad = NULL;
    struct futhark_f32_1d *arr =
                          (struct futhark_f32_1d *) malloc(sizeof(struct futhark_f32_1d));
    
    if (arr == NULL)
        return bad;
    lock_lock(&ctx->lock);
    arr->mem.references = NULL;
    if (memblock_alloc_device(ctx, &arr->mem, dim0 * sizeof(float), "arr->mem"))
        return NULL;
    arr->shape[0] = dim0;
    if (dim0 * sizeof(float) > 0)
        OPENCL_SUCCEED_OR_RETURN(clEnqueueWriteBuffer(ctx->opencl.queue,
                                                      arr->mem.mem, CL_TRUE, 0,
                                                      dim0 * sizeof(float),
                                                      data + 0, 0, NULL, NULL));
    lock_unlock(&ctx->lock);
    return arr;
}
struct futhark_f32_1d *futhark_new_raw_f32_1d(struct futhark_context *ctx,
                                              cl_mem data, int offset,
                                              int64_t dim0)
{
    struct futhark_f32_1d *bad = NULL;
    struct futhark_f32_1d *arr =
                          (struct futhark_f32_1d *) malloc(sizeof(struct futhark_f32_1d));
    
    if (arr == NULL)
        return bad;
    lock_lock(&ctx->lock);
    arr->mem.references = NULL;
    if (memblock_alloc_device(ctx, &arr->mem, dim0 * sizeof(float), "arr->mem"))
        return NULL;
    arr->shape[0] = dim0;
    if (dim0 * sizeof(float) > 0) {
        OPENCL_SUCCEED_OR_RETURN(clEnqueueCopyBuffer(ctx->opencl.queue, data,
                                                     arr->mem.mem, offset, 0,
                                                     dim0 * sizeof(float), 0,
                                                     NULL, NULL));
        if (ctx->debugging)
            OPENCL_SUCCEED_FATAL(clFinish(ctx->opencl.queue));
    }
    lock_unlock(&ctx->lock);
    return arr;
}
int futhark_free_f32_1d(struct futhark_context *ctx, struct futhark_f32_1d *arr)
{
    lock_lock(&ctx->lock);
    if (memblock_unref_device(ctx, &arr->mem, "arr->mem") != 0)
        return 1;
    lock_unlock(&ctx->lock);
    free(arr);
    return 0;
}
int futhark_values_f32_1d(struct futhark_context *ctx,
                          struct futhark_f32_1d *arr, float *data)
{
    lock_lock(&ctx->lock);
    if (arr->shape[0] * sizeof(float) > 0)
        OPENCL_SUCCEED_OR_RETURN(clEnqueueReadBuffer(ctx->opencl.queue,
                                                     arr->mem.mem, CL_TRUE, 0,
                                                     arr->shape[0] *
                                                     sizeof(float), data + 0, 0,
                                                     NULL, NULL));
    lock_unlock(&ctx->lock);
    return 0;
}
cl_mem futhark_values_raw_f32_1d(struct futhark_context *ctx,
                                 struct futhark_f32_1d *arr)
{
    return arr->mem.mem;
}
int64_t *futhark_shape_f32_1d(struct futhark_context *ctx,
                              struct futhark_f32_1d *arr)
{
    return arr->shape;
}
struct futhark_f32_2d {
    struct memblock_device mem;
    int64_t shape[2];
} ;
struct futhark_f32_2d *futhark_new_f32_2d(struct futhark_context *ctx,
                                          float *data, int64_t dim0,
                                          int64_t dim1)
{
    struct futhark_f32_2d *bad = NULL;
    struct futhark_f32_2d *arr =
                          (struct futhark_f32_2d *) malloc(sizeof(struct futhark_f32_2d));
    
    if (arr == NULL)
        return bad;
    lock_lock(&ctx->lock);
    arr->mem.references = NULL;
    if (memblock_alloc_device(ctx, &arr->mem, dim0 * dim1 * sizeof(float),
                              "arr->mem"))
        return NULL;
    arr->shape[0] = dim0;
    arr->shape[1] = dim1;
    if (dim0 * dim1 * sizeof(float) > 0)
        OPENCL_SUCCEED_OR_RETURN(clEnqueueWriteBuffer(ctx->opencl.queue,
                                                      arr->mem.mem, CL_TRUE, 0,
                                                      dim0 * dim1 *
                                                      sizeof(float), data + 0,
                                                      0, NULL, NULL));
    lock_unlock(&ctx->lock);
    return arr;
}
struct futhark_f32_2d *futhark_new_raw_f32_2d(struct futhark_context *ctx,
                                              cl_mem data, int offset,
                                              int64_t dim0, int64_t dim1)
{
    struct futhark_f32_2d *bad = NULL;
    struct futhark_f32_2d *arr =
                          (struct futhark_f32_2d *) malloc(sizeof(struct futhark_f32_2d));
    
    if (arr == NULL)
        return bad;
    lock_lock(&ctx->lock);
    arr->mem.references = NULL;
    if (memblock_alloc_device(ctx, &arr->mem, dim0 * dim1 * sizeof(float),
                              "arr->mem"))
        return NULL;
    arr->shape[0] = dim0;
    arr->shape[1] = dim1;
    if (dim0 * dim1 * sizeof(float) > 0) {
        OPENCL_SUCCEED_OR_RETURN(clEnqueueCopyBuffer(ctx->opencl.queue, data,
                                                     arr->mem.mem, offset, 0,
                                                     dim0 * dim1 *
                                                     sizeof(float), 0, NULL,
                                                     NULL));
        if (ctx->debugging)
            OPENCL_SUCCEED_FATAL(clFinish(ctx->opencl.queue));
    }
    lock_unlock(&ctx->lock);
    return arr;
}
int futhark_free_f32_2d(struct futhark_context *ctx, struct futhark_f32_2d *arr)
{
    lock_lock(&ctx->lock);
    if (memblock_unref_device(ctx, &arr->mem, "arr->mem") != 0)
        return 1;
    lock_unlock(&ctx->lock);
    free(arr);
    return 0;
}
int futhark_values_f32_2d(struct futhark_context *ctx,
                          struct futhark_f32_2d *arr, float *data)
{
    lock_lock(&ctx->lock);
    if (arr->shape[0] * arr->shape[1] * sizeof(float) > 0)
        OPENCL_SUCCEED_OR_RETURN(clEnqueueReadBuffer(ctx->opencl.queue,
                                                     arr->mem.mem, CL_TRUE, 0,
                                                     arr->shape[0] *
                                                     arr->shape[1] *
                                                     sizeof(float), data + 0, 0,
                                                     NULL, NULL));
    lock_unlock(&ctx->lock);
    return 0;
}
cl_mem futhark_values_raw_f32_2d(struct futhark_context *ctx,
                                 struct futhark_f32_2d *arr)
{
    return arr->mem.mem;
}
int64_t *futhark_shape_f32_2d(struct futhark_context *ctx,
                              struct futhark_f32_2d *arr)
{
    return arr->shape;
}
int futhark_entry_main(struct futhark_context *ctx, struct futhark_u8_2d **out0,
                       const int32_t in0, const int32_t in1, const int32_t in2,
                       const int32_t in3, const struct futhark_f32_2d *in4,
                       const struct futhark_f32_1d *in5, const
                       struct futhark_u8_2d *in6)
{
    struct memblock_device sPositions_mem_14843;
    
    sPositions_mem_14843.references = NULL;
    
    struct memblock_device sRadii_mem_14844;
    
    sRadii_mem_14844.references = NULL;
    
    struct memblock_device sColors_mem_14845;
    
    sColors_mem_14845.references = NULL;
    
    int32_t s_14578;
    int32_t s_14579;
    int32_t s_14580;
    int32_t sizze_14581;
    int32_t sizze_14582;
    int32_t width_14583;
    int32_t height_14584;
    int32_t numS_14585;
    int32_t numL_14586;
    struct memblock_device out_mem_14885;
    
    out_mem_14885.references = NULL;
    
    int32_t out_arrsizze_14886;
    int32_t out_arrsizze_14887;
    
    lock_lock(&ctx->lock);
    width_14583 = in0;
    height_14584 = in1;
    numS_14585 = in2;
    numL_14586 = in3;
    sPositions_mem_14843 = in4->mem;
    s_14578 = in4->shape[0];
    sizze_14581 = in4->shape[1];
    sRadii_mem_14844 = in5->mem;
    s_14579 = in5->shape[0];
    sColors_mem_14845 = in6->mem;
    s_14580 = in6->shape[0];
    sizze_14582 = in6->shape[1];
    
    int ret = futrts_main(ctx, &out_mem_14885, &out_arrsizze_14886,
                          &out_arrsizze_14887, sPositions_mem_14843,
                          sRadii_mem_14844, sColors_mem_14845, s_14578, s_14579,
                          s_14580, sizze_14581, sizze_14582, width_14583,
                          height_14584, numS_14585, numL_14586);
    
    if (ret == 0) {
        assert((*out0 =
                (struct futhark_u8_2d *) malloc(sizeof(struct futhark_u8_2d))) !=
            NULL);
        (*out0)->mem = out_mem_14885;
        (*out0)->shape[0] = out_arrsizze_14886;
        (*out0)->shape[1] = out_arrsizze_14887;
    }
    lock_unlock(&ctx->lock);
    return ret;
}
