#ifdef cl_clang_storage_class_specifiers
#pragma OPENCL EXTENSION cl_clang_storage_class_specifiers : enable
#endif
#pragma OPENCL EXTENSION cl_khr_byte_addressable_store : enable
__kernel void dummy_kernel(__global unsigned char *dummy, int n)
{
    const int thread_gid = get_global_id(0);

    if (thread_gid >= n)
        return;
}
typedef char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long int64_t;
typedef uchar uint8_t;
typedef ushort uint16_t;
typedef uint uint32_t;
typedef ulong uint64_t;
#define ALIGNED_LOCAL_MEMORY(m,size) __local int64_t m[((size + 7) & ~7)/8]
#ifdef cl_nv_pragma_unroll
static inline void mem_fence_global()
{
    asm(\"membar.gl;\");
}
#else
static inline void mem_fence_global()
{
    mem_fence(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);
}
#endif
static inline void mem_fence_local()
{
    mem_fence(CLK_LOCAL_MEM_FENCE);
}
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
__kernel void map_transpose_f32(int32_t destoffset_1, int32_t srcoffset_3,
                                int32_t num_arrays_4, int32_t x_elems_5,
                                int32_t y_elems_6, int32_t in_elems_7,
                                int32_t out_elems_8, int32_t mulx_9,
                                int32_t muly_10, __global
                                unsigned char *destmem_0, __global
                                unsigned char *srcmem_2)
{
    const int block_dim0 = 0;
    const int block_dim1 = 1;
    const int block_dim2 = 2;

    ALIGNED_LOCAL_MEMORY(block_11_backing_0, 4224);

    __local char *block_11;

    block_11 = (__local char *) block_11_backing_0;

    int32_t get_global_id_0_37;

    get_global_id_0_37 = get_global_id(0);

    int32_t get_local_id_0_38;

    get_local_id_0_38 = get_local_id(0);

    int32_t get_local_id_1_39;

    get_local_id_1_39 = get_local_id(1);

    int32_t get_group_id_0_40;

    get_group_id_0_40 = get_group_id(0);

    int32_t get_group_id_1_41;

    get_group_id_1_41 = get_group_id(1);

    int32_t get_group_id_2_42;

    get_group_id_2_42 = get_group_id(2);

    int32_t our_array_offset_30 = get_group_id_2_42 * x_elems_5 * y_elems_6;
    int32_t odata_offset_33 = squot32(destoffset_1, 4) + our_array_offset_30;
    int32_t idata_offset_34 = squot32(srcoffset_3, 4) + our_array_offset_30;
    int32_t x_index_31 = get_global_id_0_37;
    int32_t y_index_32 = get_group_id_1_41 * 32 + get_local_id_1_39;

    if (slt32(x_index_31, x_elems_5)) {
        for (int32_t j_43 = 0; j_43 < 4; j_43++) {
            int32_t index_in_35 = (y_index_32 + j_43 * 8) * x_elems_5 +
                    x_index_31;

            if (slt32(y_index_32 + j_43 * 8, y_elems_6) && slt32(index_in_35,
                                                                 in_elems_7)) {
                ((__local float *) block_11)[(get_local_id_1_39 + j_43 * 8) *
                                             33 + get_local_id_0_38] =
                    ((__global float *) srcmem_2)[idata_offset_34 +
                                                  index_in_35];
            }
        }
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    x_index_31 = get_group_id_1_41 * 32 + get_local_id_0_38;
    y_index_32 = get_group_id_0_40 * 32 + get_local_id_1_39;
    if (slt32(x_index_31, y_elems_6)) {
        for (int32_t j_43 = 0; j_43 < 4; j_43++) {
            int32_t index_out_36 = (y_index_32 + j_43 * 8) * y_elems_6 +
                    x_index_31;

            if (slt32(y_index_32 + j_43 * 8, x_elems_5) && slt32(index_out_36,
                                                                 out_elems_8)) {
                ((__global float *) destmem_0)[odata_offset_33 + index_out_36] =
                    ((__local float *) block_11)[get_local_id_0_38 * 33 +
                                                 get_local_id_1_39 + j_43 * 8];
            }
        }
    }
}
__kernel void map_transpose_f32_low_height(int32_t destoffset_1,
                                           int32_t srcoffset_3,
                                           int32_t num_arrays_4,
                                           int32_t x_elems_5, int32_t y_elems_6,
                                           int32_t in_elems_7,
                                           int32_t out_elems_8, int32_t mulx_9,
                                           int32_t muly_10, __global
                                           unsigned char *destmem_0, __global
                                           unsigned char *srcmem_2)
{
    const int block_dim0 = 0;
    const int block_dim1 = 1;
    const int block_dim2 = 2;

    ALIGNED_LOCAL_MEMORY(block_11_backing_0, 1088);

    __local char *block_11;

    block_11 = (__local char *) block_11_backing_0;

    int32_t get_global_id_0_37;

    get_global_id_0_37 = get_global_id(0);

    int32_t get_local_id_0_38;

    get_local_id_0_38 = get_local_id(0);

    int32_t get_local_id_1_39;

    get_local_id_1_39 = get_local_id(1);

    int32_t get_group_id_0_40;

    get_group_id_0_40 = get_group_id(0);

    int32_t get_group_id_1_41;

    get_group_id_1_41 = get_group_id(1);

    int32_t get_group_id_2_42;

    get_group_id_2_42 = get_group_id(2);

    int32_t our_array_offset_30 = get_group_id_2_42 * x_elems_5 * y_elems_6;
    int32_t odata_offset_33 = squot32(destoffset_1, 4) + our_array_offset_30;
    int32_t idata_offset_34 = squot32(srcoffset_3, 4) + our_array_offset_30;
    int32_t x_index_31 = get_group_id_0_40 * 16 * mulx_9 + get_local_id_0_38 +
            srem32(get_local_id_1_39, mulx_9) * 16;
    int32_t y_index_32 = get_group_id_1_41 * 16 + squot32(get_local_id_1_39,
                                                          mulx_9);
    int32_t index_in_35 = y_index_32 * x_elems_5 + x_index_31;

    if (slt32(x_index_31, x_elems_5) && (slt32(y_index_32, y_elems_6) &&
                                         slt32(index_in_35, in_elems_7))) {
        ((__local float *) block_11)[get_local_id_1_39 * 17 +
                                     get_local_id_0_38] = ((__global
                                                            float *) srcmem_2)[idata_offset_34 +
                                                                               index_in_35];
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    x_index_31 = get_group_id_1_41 * 16 + squot32(get_local_id_0_38, mulx_9);
    y_index_32 = get_group_id_0_40 * 16 * mulx_9 + get_local_id_1_39 +
        srem32(get_local_id_0_38, mulx_9) * 16;

    int32_t index_out_36 = y_index_32 * y_elems_6 + x_index_31;

    if (slt32(x_index_31, y_elems_6) && (slt32(y_index_32, x_elems_5) &&
                                         slt32(index_out_36, out_elems_8))) {
        ((__global float *) destmem_0)[odata_offset_33 + index_out_36] =
            ((__local float *) block_11)[get_local_id_0_38 * 17 +
                                         get_local_id_1_39];
    }
}
__kernel void map_transpose_f32_low_width(int32_t destoffset_1,
                                          int32_t srcoffset_3,
                                          int32_t num_arrays_4,
                                          int32_t x_elems_5, int32_t y_elems_6,
                                          int32_t in_elems_7,
                                          int32_t out_elems_8, int32_t mulx_9,
                                          int32_t muly_10, __global
                                          unsigned char *destmem_0, __global
                                          unsigned char *srcmem_2)
{
    const int block_dim0 = 0;
    const int block_dim1 = 1;
    const int block_dim2 = 2;

    ALIGNED_LOCAL_MEMORY(block_11_backing_0, 1088);

    __local char *block_11;

    block_11 = (__local char *) block_11_backing_0;

    int32_t get_global_id_0_37;

    get_global_id_0_37 = get_global_id(0);

    int32_t get_local_id_0_38;

    get_local_id_0_38 = get_local_id(0);

    int32_t get_local_id_1_39;

    get_local_id_1_39 = get_local_id(1);

    int32_t get_group_id_0_40;

    get_group_id_0_40 = get_group_id(0);

    int32_t get_group_id_1_41;

    get_group_id_1_41 = get_group_id(1);

    int32_t get_group_id_2_42;

    get_group_id_2_42 = get_group_id(2);

    int32_t our_array_offset_30 = get_group_id_2_42 * x_elems_5 * y_elems_6;
    int32_t odata_offset_33 = squot32(destoffset_1, 4) + our_array_offset_30;
    int32_t idata_offset_34 = squot32(srcoffset_3, 4) + our_array_offset_30;
    int32_t x_index_31 = get_group_id_0_40 * 16 + squot32(get_local_id_0_38,
                                                          muly_10);
    int32_t y_index_32 = get_group_id_1_41 * 16 * muly_10 + get_local_id_1_39 +
            srem32(get_local_id_0_38, muly_10) * 16;
    int32_t index_in_35 = y_index_32 * x_elems_5 + x_index_31;

    if (slt32(x_index_31, x_elems_5) && (slt32(y_index_32, y_elems_6) &&
                                         slt32(index_in_35, in_elems_7))) {
        ((__local float *) block_11)[get_local_id_1_39 * 17 +
                                     get_local_id_0_38] = ((__global
                                                            float *) srcmem_2)[idata_offset_34 +
                                                                               index_in_35];
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    x_index_31 = get_group_id_1_41 * 16 * muly_10 + get_local_id_0_38 +
        srem32(get_local_id_1_39, muly_10) * 16;
    y_index_32 = get_group_id_0_40 * 16 + squot32(get_local_id_1_39, muly_10);

    int32_t index_out_36 = y_index_32 * y_elems_6 + x_index_31;

    if (slt32(x_index_31, y_elems_6) && (slt32(y_index_32, x_elems_5) &&
                                         slt32(index_out_36, out_elems_8))) {
        ((__global float *) destmem_0)[odata_offset_33 + index_out_36] =
            ((__local float *) block_11)[get_local_id_0_38 * 17 +
                                         get_local_id_1_39];
    }
}
__kernel void map_transpose_f32_small(int32_t destoffset_1, int32_t srcoffset_3,
                                      int32_t num_arrays_4, int32_t x_elems_5,
                                      int32_t y_elems_6, int32_t in_elems_7,
                                      int32_t out_elems_8, int32_t mulx_9,
                                      int32_t muly_10, __global
                                      unsigned char *destmem_0, __global
                                      unsigned char *srcmem_2)
{
    const int block_dim0 = 0;
    const int block_dim1 = 1;
    const int block_dim2 = 2;

    ALIGNED_LOCAL_MEMORY(block_11_backing_0, 1);

    __local char *block_11;

    block_11 = (__local char *) block_11_backing_0;

    int32_t get_global_id_0_37;

    get_global_id_0_37 = get_global_id(0);

    int32_t get_local_id_0_38;

    get_local_id_0_38 = get_local_id(0);

    int32_t get_local_id_1_39;

    get_local_id_1_39 = get_local_id(1);

    int32_t get_group_id_0_40;

    get_group_id_0_40 = get_group_id(0);

    int32_t get_group_id_1_41;

    get_group_id_1_41 = get_group_id(1);

    int32_t get_group_id_2_42;

    get_group_id_2_42 = get_group_id(2);

    int32_t our_array_offset_30 = squot32(get_global_id_0_37, y_elems_6 *
                                          x_elems_5) * (y_elems_6 * x_elems_5);
    int32_t x_index_31 = squot32(srem32(get_global_id_0_37, y_elems_6 *
                                        x_elems_5), y_elems_6);
    int32_t y_index_32 = srem32(get_global_id_0_37, y_elems_6);
    int32_t odata_offset_33 = squot32(destoffset_1, 4) + our_array_offset_30;
    int32_t idata_offset_34 = squot32(srcoffset_3, 4) + our_array_offset_30;
    int32_t index_in_35 = y_index_32 * x_elems_5 + x_index_31;
    int32_t index_out_36 = x_index_31 * y_elems_6 + y_index_32;

    if (slt32(get_global_id_0_37, in_elems_7)) {
        ((__global float *) destmem_0)[odata_offset_33 + index_out_36] =
            ((__global float *) srcmem_2)[idata_offset_34 + index_in_35];
    }
}
__kernel void replicate_12957(int32_t sizze_12582, __global
                              unsigned char *mem_12893)
{
    const int block_dim0 = 0;
    const int block_dim1 = 1;
    const int block_dim2 = 2;
    int32_t replicate_gtid_12957;
    int32_t replicate_ltid_12958;
    int32_t replicate_gid_12959;

    replicate_gtid_12957 = get_global_id(0);
    replicate_ltid_12958 = get_local_id(0);
    replicate_gid_12959 = get_group_id(0);
    if (slt32(replicate_gtid_12957, sizze_12582 * 4)) {
        ((__global int8_t *) mem_12893)[squot32(replicate_gtid_12957, 4) * 4 +
                                        (replicate_gtid_12957 -
                                         squot32(replicate_gtid_12957, 4) *
                                         4)] = 0;
    }
}
__kernel void segmap_12725(int32_t s_12545, int32_t sizze_12548,
                           int32_t sizze_12549, int32_t numS_12552,
                           int32_t iota_arg_12581, int32_t num_groups_12740,
                           __global unsigned char *sPositions_mem_12887,
                           __global unsigned char *sRadii_mem_12888, __global
                           unsigned char *sColors_mem_12889, __global
                           unsigned char *mem_12916, __global
                           unsigned char *mem_12936, __global
                           unsigned char *mem_12939, __global
                           unsigned char *mem_12943)
{
    const int32_t segmap_group_sizze_12728 = mainzisegmap_group_sizze_12727;
    const int block_dim0 = 0;
    const int block_dim1 = 1;
    const int block_dim2 = 2;
    int32_t global_tid_12977;
    int32_t local_tid_12978;
    int32_t group_sizze_12981;
    int32_t wave_sizze_12980;
    int32_t group_tid_12979;

    global_tid_12977 = get_global_id(0);
    local_tid_12978 = get_local_id(0);
    group_sizze_12981 = get_local_size(0);
    wave_sizze_12980 = LOCKSTEP_WIDTH;
    group_tid_12979 = get_group_id(0);

    int32_t phys_tid_12725 = global_tid_12977;
    int32_t phys_group_id_12982;

    phys_group_id_12982 = get_group_id(0);
    for (int32_t i_12983 = 0; i_12983 < squot32(squot32(iota_arg_12581 +
                                                        segmap_group_sizze_12728 -
                                                        1,
                                                        segmap_group_sizze_12728) -
                                                phys_group_id_12982 +
                                                num_groups_12740 - 1,
                                                num_groups_12740); i_12983++) {
        int32_t virt_group_id_12984 = phys_group_id_12982 + i_12983 *
                num_groups_12740;
        int32_t gtid_12724 = virt_group_id_12984 * segmap_group_sizze_12728 +
                local_tid_12978;

        if (slt32(gtid_12724, iota_arg_12581)) {
            int32_t res_12742;
            int8_t res_12743;
            float res_12744;
            int32_t redout_12853;
            int8_t redout_12854;
            float redout_12855;

            redout_12853 = 0;
            redout_12854 = 0;
            redout_12855 = 100.0F;
            for (int32_t i_12856 = 0; i_12856 < s_12545; i_12856++) {
                float x_12756 = ((__global float *) sRadii_mem_12888)[i_12856];
                int32_t x_12757 = i_12856;
                bool index_concat_cmp_12873 = sle32(numS_12552, i_12856);
                int8_t index_concat_branch_12877;

                if (index_concat_cmp_12873) {
                    int8_t index_concat_12875 = 2;

                    index_concat_branch_12877 = index_concat_12875;
                } else {
                    int8_t index_concat_12876 = 1;

                    index_concat_branch_12877 = index_concat_12876;
                }

                int8_t x_12758 = index_concat_branch_12877;
                float res_12759;
                float res_12760;
                float redout_12857;
                float redout_12858;

                redout_12857 = 0.0F;
                redout_12858 = 0.0F;
                for (int32_t i_12859 = 0; i_12859 < 3; i_12859++) {
                    float x_12767 = ((__global
                                      float *) sPositions_mem_12887)[i_12856 *
                                                                     sizze_12548 +
                                                                     i_12859];
                    float x_12768 = ((__global float *) mem_12916)[i_12859 *
                                                                   iota_arg_12581 +
                                                                   gtid_12724];
                    float res_12769 = x_12767 * x_12768;
                    float res_12770 = x_12767 * x_12767;
                    float res_12763 = res_12769 + redout_12857;
                    float res_12766 = res_12770 + redout_12858;
                    float redout_tmp_12988 = res_12763;
                    float redout_tmp_12989;

                    redout_tmp_12989 = res_12766;
                    redout_12857 = redout_tmp_12988;
                    redout_12858 = redout_tmp_12989;
                }
                res_12759 = redout_12857;
                res_12760 = redout_12858;

                float y_12771 = x_12756 * x_12756;
                float c_12772 = res_12760 - y_12771;
                float x_12773 = res_12759 * res_12759;
                float disc_12774 = x_12773 - c_12772;
                bool cond_12775 = disc_12774 < 0.0F;
                float res_12776;

                if (cond_12775) {
                    res_12776 = 100.0F;
                } else {
                    float res_12777;

                    res_12777 = futrts_sqrt32(disc_12774);

                    float t_12778 = res_12759 - res_12777;
                    bool cond_12779 = 0.0F < t_12778;
                    float res_12780;

                    if (cond_12779) {
                        res_12780 = t_12778;
                    } else {
                        res_12780 = 100.0F;
                    }
                    res_12776 = res_12780;
                }

                bool cond_12751 = res_12776 < redout_12855;
                int32_t res_12752;

                if (cond_12751) {
                    res_12752 = x_12757;
                } else {
                    res_12752 = redout_12853;
                }

                int8_t res_12753;

                if (cond_12751) {
                    res_12753 = x_12758;
                } else {
                    res_12753 = redout_12854;
                }

                float res_12754;

                if (cond_12751) {
                    res_12754 = res_12776;
                } else {
                    res_12754 = redout_12855;
                }

                int32_t redout_tmp_12985 = res_12752;
                int8_t redout_tmp_12986 = res_12753;
                float redout_tmp_12987;

                redout_tmp_12987 = res_12754;
                redout_12853 = redout_tmp_12985;
                redout_12854 = redout_tmp_12986;
                redout_12855 = redout_tmp_12987;
            }
            res_12742 = redout_12853;
            res_12743 = redout_12854;
            res_12744 = redout_12855;

            bool cond_12781 = res_12743 == 1;
            bool cond_12782 = res_12743 == 2;
            __private char *mem_12925;
            __private char mem_12925_backing_0[12];

            mem_12925 = mem_12925_backing_0;

            __private char *mem_12927;
            __private char mem_12927_backing_1[4];

            mem_12927 = mem_12927_backing_1;
            if (cond_12781) {
                for (int32_t i_12990 = 0; i_12990 < 4; i_12990++) {
                    ((__global int8_t *) mem_12943)[phys_tid_12725 + i_12990 *
                                                    (num_groups_12740 *
                                                     segmap_group_sizze_12728)] =
                        ((__global int8_t *) sColors_mem_12889)[res_12742 *
                                                                sizze_12549 +
                                                                i_12990];
                }
            } else {
                if (cond_12782) {
                    for (int32_t i_12991 = 0; i_12991 < 4; i_12991++) {
                        ((__global int8_t *) mem_12939)[phys_tid_12725 +
                                                        i_12991 *
                                                        (num_groups_12740 *
                                                         segmap_group_sizze_12728)] =
                            ((__global int8_t *) sColors_mem_12889)[res_12742 *
                                                                    sizze_12549 +
                                                                    i_12991];
                    }
                } else {
                    for (int32_t i_12862 = 0; i_12862 < 3; i_12862++) {
                        float x_12788 = ((__global float *) mem_12916)[i_12862 *
                                                                       iota_arg_12581 +
                                                                       gtid_12724];
                        float res_12789 = (float) fabs(x_12788);
                        float x_12790 = 0.5F * res_12789;
                        float res_12791 = 0.3F + x_12790;

                        ((__private float *) mem_12925)[i_12862] = res_12791;
                    }
                    for (int32_t i_12866 = 0; i_12866 < 4; i_12866++) {
                        bool index_concat_cmp_12882 = sle32(3, i_12866);
                        float index_concat_branch_12886;

                        if (index_concat_cmp_12882) {
                            index_concat_branch_12886 = 1.0F;
                        } else {
                            float index_concat_12885 = ((__private
                                                         float *) mem_12925)[i_12866];

                            index_concat_branch_12886 = index_concat_12885;
                        }

                        float f32_arg_12796 = 255.0F *
                              index_concat_branch_12886;
                        int8_t unsign_arg_12797 = fptoui_f32_i8(f32_arg_12796);

                        ((__private int8_t *) mem_12927)[i_12866] =
                            unsign_arg_12797;
                    }
                    for (int32_t i_12994 = 0; i_12994 < 4; i_12994++) {
                        ((__global int8_t *) mem_12939)[phys_tid_12725 +
                                                        i_12994 *
                                                        (num_groups_12740 *
                                                         segmap_group_sizze_12728)] =
                            ((__private int8_t *) mem_12927)[i_12994];
                    }
                }
                for (int32_t i_12995 = 0; i_12995 < 4; i_12995++) {
                    ((__global int8_t *) mem_12943)[phys_tid_12725 + i_12995 *
                                                    (num_groups_12740 *
                                                     segmap_group_sizze_12728)] =
                        ((__global int8_t *) mem_12939)[phys_tid_12725 +
                                                        i_12995 *
                                                        (num_groups_12740 *
                                                         segmap_group_sizze_12728)];
                }
            }
            for (int32_t i_12996 = 0; i_12996 < 4; i_12996++) {
                ((__global int8_t *) mem_12936)[gtid_12724 * 4 + i_12996] =
                    ((__global int8_t *) mem_12943)[phys_tid_12725 + i_12996 *
                                                    (num_groups_12740 *
                                                     segmap_group_sizze_12728)];
            }
        }
    }
}
__kernel void segmap_12802(int32_t iota_arg_12581, __global
                           unsigned char *mem_12903, __global
                           unsigned char *mem_12907, __global
                           unsigned char *mem_12912)
{
    const int32_t segmap_group_sizze_12806 = mainzisegmap_group_sizze_12805;
    const int block_dim0 = 0;
    const int block_dim1 = 1;
    const int block_dim2 = 2;
    int32_t global_tid_12972;
    int32_t local_tid_12973;
    int32_t group_sizze_12976;
    int32_t wave_sizze_12975;
    int32_t group_tid_12974;

    global_tid_12972 = get_global_id(0);
    local_tid_12973 = get_local_id(0);
    group_sizze_12976 = get_local_size(0);
    wave_sizze_12975 = LOCKSTEP_WIDTH;
    group_tid_12974 = get_group_id(0);

    int32_t phys_tid_12802 = global_tid_12972;
    int32_t gtid_12800 = squot32(group_tid_12974 * segmap_group_sizze_12806 +
                                 local_tid_12973, 3);
    int32_t gtid_12801;

    gtid_12801 = group_tid_12974 * segmap_group_sizze_12806 + local_tid_12973 -
        squot32(group_tid_12974 * segmap_group_sizze_12806 + local_tid_12973,
                3) * 3;
    if (slt32(gtid_12800, iota_arg_12581) && slt32(gtid_12801, 3)) {
        float res_12810 = ((__global float *) mem_12903)[gtid_12800];
        float x_12811 = ((__global float *) mem_12907)[gtid_12800 * 3 +
                                                       gtid_12801];
        float res_12812 = x_12811 / res_12810;

        ((__global float *) mem_12912)[gtid_12800 * 3 + gtid_12801] = res_12812;
    }
}
__kernel void segmap_12814(int32_t width_12550, int32_t iota_arg_12581,
                           float res_12633, int32_t y_12634, int32_t x_12635,
                           int32_t num_groups_12829, __global
                           unsigned char *mem_12900, __global
                           unsigned char *mem_12903)
{
    const int32_t segmap_group_sizze_12817 = mainzisegmap_group_sizze_12816;
    const int block_dim0 = 0;
    const int block_dim1 = 1;
    const int block_dim2 = 2;
    int32_t global_tid_12962;
    int32_t local_tid_12963;
    int32_t group_sizze_12966;
    int32_t wave_sizze_12965;
    int32_t group_tid_12964;

    global_tid_12962 = get_global_id(0);
    local_tid_12963 = get_local_id(0);
    group_sizze_12966 = get_local_size(0);
    wave_sizze_12965 = LOCKSTEP_WIDTH;
    group_tid_12964 = get_group_id(0);

    int32_t phys_tid_12814 = global_tid_12962;
    int32_t phys_group_id_12967;

    phys_group_id_12967 = get_group_id(0);
    for (int32_t i_12968 = 0; i_12968 < squot32(squot32(iota_arg_12581 +
                                                        segmap_group_sizze_12817 -
                                                        1,
                                                        segmap_group_sizze_12817) -
                                                phys_group_id_12967 +
                                                num_groups_12829 - 1,
                                                num_groups_12829); i_12968++) {
        int32_t virt_group_id_12969 = phys_group_id_12967 + i_12968 *
                num_groups_12829;
        int32_t gtid_12813 = virt_group_id_12969 * segmap_group_sizze_12817 +
                local_tid_12963;

        if (slt32(gtid_12813, iota_arg_12581)) {
            int32_t arr_elem_12831 = srem32(gtid_12813, width_12550);
            int32_t arr_elem_12832 = squot32(gtid_12813, width_12550);
            int32_t r32_arg_12833 = arr_elem_12831 - y_12634;
            float res_12834 = sitofp_i32_f32(r32_arg_12833);
            int32_t r32_arg_12835 = x_12635 - arr_elem_12832;
            float res_12836 = sitofp_i32_f32(r32_arg_12835);
            __private char *mem_12896;
            __private char mem_12896_backing_0[12];

            mem_12896 = mem_12896_backing_0;
            ((__private float *) mem_12896)[0] = res_12633;
            ((__private float *) mem_12896)[1] = res_12834;
            ((__private float *) mem_12896)[2] = res_12836;

            float res_12838;
            float redout_12851 = 0.0F;

            for (int32_t i_12852 = 0; i_12852 < 3; i_12852++) {
                float x_12842 = ((__private float *) mem_12896)[i_12852];
                float res_12843 = x_12842 * x_12842;
                float res_12841 = res_12843 + redout_12851;
                float redout_tmp_12970 = res_12841;

                redout_12851 = redout_tmp_12970;
            }
            res_12838 = redout_12851;

            float res_12844;

            res_12844 = futrts_sqrt32(res_12838);
            for (int32_t i_12971 = 0; i_12971 < 3; i_12971++) {
                ((__global float *) mem_12900)[gtid_12813 + i_12971 *
                                               iota_arg_12581] = ((__private
                                                                   float *) mem_12896)[i_12971];
            }
            ((__global float *) mem_12903)[gtid_12813] = res_12844;
        }
    }
}
