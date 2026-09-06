/*
 * This file math-vect-int-opencl.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2026
 *
 * L1vm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * L1vm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with L1vm.  If not, see <http://www.gnu.org/licenses/>.
 */

// Created with help by open code Big Pickle AI.

#include "../../include/global.h"
#include <math.h>
#include <time.h>
#include "../../include/stack.h"
#include <CL/cl.h>

// protos

extern S2 memory_bounds (S8 start, S8 offset_access);

S2 ocl_init (void);
S2 ocl_double_init (void);
S2 ocl_double_shutdown (void);


struct data_info data_info[MAXDATAINFO];
S8 data_info_ind;

S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
	memcpy (&data_info, &data_info_orig, sizeof (data_info_orig));
	data_info_ind = data_info_ind_orig;

	ocl_init ();
    ocl_double_init ();

	return (0);
}

// ============================================================================
// OpenCL infrastructure
// ============================================================================

cl_platform_id ocl_platform;
cl_device_id ocl_device;
cl_context ocl_context;
cl_command_queue ocl_queue;
static cl_program ocl_program;
static S8 ocl_initialized = 0;
size_t ocl_preferred_group_size = 64;

// kernel objects
static cl_kernel kern_add_scalar;
static cl_kernel kern_sub_scalar;
static cl_kernel kern_mul_scalar;
static cl_kernel kern_div_scalar;
static cl_kernel kern_add_array;
static cl_kernel kern_sub_array;
static cl_kernel kern_mul_array;
static cl_kernel kern_div_array;
static cl_kernel kern_min;
static cl_kernel kern_max;
static cl_kernel kern_sum;
static cl_kernel kern_search;
static cl_kernel kern_clear;
static cl_kernel kern_copy;
static cl_kernel kern_init_byte;
static cl_kernel kern_init_int64;

// ============================================================================
// GPU array slots: persistent device buffers, shared with the double module.
// An array is copied into a slot with oclmath_push, the math kernels work
// on the slots, and the result is copied back with oclmath_pull.
// ============================================================================

#define MAXSLOTS 64

struct ocl_slot
{
	cl_mem buf;
	size_t bytes;
};

struct ocl_slot ocl_slots[MAXSLOTS];

// get (and if needed (re)allocate) the persistent device buffer for a slot
cl_mem ocl_get_slot (S4 slot, size_t bytes)
{
	cl_int err;
	cl_mem buf;

	if (slot < 0 || slot >= MAXSLOTS)
	{
		printf ("ocl_get_slot: ERROR: slot out of range!\n");
		return (NULL);
	}

	if (ocl_slots[slot].buf != NULL && bytes <= ocl_slots[slot].bytes)
	{
		return (ocl_slots[slot].buf);
	}

	if (ocl_slots[slot].buf != NULL)
	{
		clReleaseMemObject (ocl_slots[slot].buf);
		ocl_slots[slot].buf = NULL;
		ocl_slots[slot].bytes = 0;
	}

	buf = clCreateBuffer (ocl_context, CL_MEM_READ_WRITE, bytes, NULL, &err);
	if (err != CL_SUCCESS)
	{
		printf ("ocl_get_slot: ERROR clCreateBuffer: %d\n", err);
		return (NULL);
	}

	ocl_slots[slot].buf = buf;
	ocl_slots[slot].bytes = bytes;
	return (buf);
}

// release all persistent slot buffers
void ocl_delete_slots (void)
{
	S4 slot;
	for (slot = 0; slot < MAXSLOTS; slot++)
	{
		if (ocl_slots[slot].buf != NULL)
		{
			clReleaseMemObject (ocl_slots[slot].buf);
			ocl_slots[slot].buf = NULL;
			ocl_slots[slot].bytes = 0;
		}
	}
}

static const char *ocl_kernel_source =
"typedef long S8;\n"
"\n"
"__kernel void kern_add_scalar(__global S8 *src, __global S8 *dst, S8 number, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end) dst[i] = src[i] + number;\n"
"}\n"
"\n"
"__kernel void kern_sub_scalar(__global S8 *src, __global S8 *dst, S8 number, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end) dst[i] = src[i] - number;\n"
"}\n"
"\n"
"__kernel void kern_mul_scalar(__global S8 *src, __global S8 *dst, S8 number, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end) dst[i] = src[i] * number;\n"
"}\n"
"\n"
"__kernel void kern_div_scalar(__global S8 *src, __global S8 *dst, S8 number, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end) dst[i] = src[i] / number;\n"
"}\n"
"\n"
"__kernel void kern_add_array(__global S8 *src, __global S8 *src2, __global S8 *dst, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end) dst[i] = src[i] + src2[i];\n"
"}\n"
"\n"
"__kernel void kern_sub_array(__global S8 *src, __global S8 *src2, __global S8 *dst, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end) dst[i] = src[i] - src2[i];\n"
"}\n"
"\n"
"__kernel void kern_mul_array(__global S8 *src, __global S8 *src2, __global S8 *dst, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end) dst[i] = src[i] * src2[i];\n"
"}\n"
"\n"
"__kernel void kern_div_array(__global S8 *src, __global S8 *src2, __global S8 *dst, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end)\n"
"    {\n"
"        if (src2[i] != 0) dst[i] = src[i] / src2[i];\n"
"    }\n"
"}\n"
"\n"
"__kernel void kern_min(__global S8 *src, __global S8 *result, S8 start, S8 end)\n"
"{\n"
"    size_t gid = get_global_id(0);\n"
"    size_t i = start + gid;\n"
"    size_t total = end - start + 1;\n"
"    __local S8 local_min[256];\n"
"    size_t lid = get_local_id(0);\n"
"    size_t group_size = get_local_size(0);\n"
"    if (i <= end) local_min[lid] = src[i];\n"
"    else local_min[lid] = result[0];\n"
"    barrier(CLK_LOCAL_MEM_FENCE);\n"
"    for (size_t s = group_size / 2; s > 0; s >>= 1)\n"
"    {\n"
"        if (lid < s && (gid + s) < total) { if (local_min[lid + s] < local_min[lid]) local_min[lid] = local_min[lid + s]; }\n"
"        barrier(CLK_LOCAL_MEM_FENCE);\n"
"    }\n"
"    if (lid == 0) { if (local_min[0] < result[0]) result[0] = local_min[0]; }\n"
"}\n"
"\n"
"__kernel void kern_max(__global S8 *src, __global S8 *result, S8 start, S8 end)\n"
"{\n"
"    size_t gid = get_global_id(0);\n"
"    size_t i = start + gid;\n"
"    size_t total = end - start + 1;\n"
"    __local S8 local_max[256];\n"
"    size_t lid = get_local_id(0);\n"
"    size_t group_size = get_local_size(0);\n"
"    if (i <= end) local_max[lid] = src[i];\n"
"    else local_max[lid] = result[0];\n"
"    barrier(CLK_LOCAL_MEM_FENCE);\n"
"    for (size_t s = group_size / 2; s > 0; s >>= 1)\n"
"    {\n"
"        if (lid < s && (gid + s) < total) { if (local_max[lid + s] > local_max[lid]) local_max[lid] = local_max[lid + s]; }\n"
"        barrier(CLK_LOCAL_MEM_FENCE);\n"
"    }\n"
"    if (lid == 0) { if (local_max[0] > result[0]) result[0] = local_max[0]; }\n"
"}\n"
"\n"
"__kernel void kern_sum(__global S8 *src, __global S8 *partial_sums, S8 start, S8 end)\n"
"{\n"
"    size_t gid = get_global_id(0);\n"
"    size_t i = start + gid;\n"
"    size_t lid = get_local_id(0);\n"
"    size_t group_size = get_local_size(0);\n"
"    __local S8 local_sum[256];\n"
"    if (i <= end) local_sum[lid] = src[i];\n"
"    else local_sum[lid] = 0;\n"
"    barrier(CLK_LOCAL_MEM_FENCE);\n"
"    for (size_t s = group_size / 2; s > 0; s >>= 1)\n"
"    {\n"
"        if (lid < s) local_sum[lid] += local_sum[lid + s];\n"
"        barrier(CLK_LOCAL_MEM_FENCE);\n"
"    }\n"
"    if (lid == 0) partial_sums[get_group_id(0)] = local_sum[0];\n"
"}\n"
"\n"
"__kernel void kern_search(__global S8 *src, __global S8 *result, S8 search_val, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end && src[i] == search_val && result[0] == -1) result[0] = i;\n"
"}\n"
"\n"
"__kernel void kern_clear(__global unsigned char *data, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end) data[i] = 0;\n"
"}\n"
"\n"
"__kernel void kern_copy(__global unsigned char *src, __global unsigned char *dst, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end) dst[i] = src[i];\n"
"}\n"
"\n"
"__kernel void kern_init_byte(__global unsigned char *data, unsigned char value, S8 start, S8 end, S8 step)\n"
"{\n"
"    size_t i = start + get_global_id(0) * step;\n"
"    if (i < end) data[i] = value;\n"
"}\n"
"\n"
"__kernel void kern_init_int64(__global S8 *data, S8 value, S8 start, S8 end, S8 step)\n"
"{\n"
"    size_t i = start + get_global_id(0) * step;\n"
"    if (i < end) data[i] = value;\n"
"}\n";

// initialize OpenCL device, context, queue, and compile kernels
S2 ocl_init (void)
{
	cl_int err;
	char build_log[4096];
	size_t log_size;

	err = clGetPlatformIDs (1, &ocl_platform, NULL);
	if (err != CL_SUCCESS)
	{
		printf ("ocl_init: ERROR clGetPlatformIDs failed: %d\n", err);
		return (-1);
	}

	err = clGetDeviceIDs (ocl_platform, CL_DEVICE_TYPE_GPU, 1, &ocl_device, NULL);
	if (err != CL_SUCCESS)
	{
		printf ("ocl_init: ERROR clGetDeviceIDs failed: %d\n", err);
		return (-1);
	}

	clGetDeviceInfo (ocl_device, CL_DEVICE_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof (size_t), &ocl_preferred_group_size, NULL);
	if (ocl_preferred_group_size == 0 || ocl_preferred_group_size > 256)
	{
		ocl_preferred_group_size = 64;
	}

	ocl_context = clCreateContext (NULL, 1, &ocl_device, NULL, NULL, &err);
	if (err != CL_SUCCESS)
	{
		printf ("ocl_init: ERROR clCreateContext failed: %d\n", err);
		return (-1);
	}

	ocl_queue = clCreateCommandQueueWithProperties (ocl_context, ocl_device, NULL, &err);
	if (err != CL_SUCCESS)
	{
		printf ("ocl_init: ERROR clCreateCommandQueueWithProperties failed: %d\n", err);
		clReleaseContext (ocl_context);
		return (-1);
	}

	ocl_program = clCreateProgramWithSource (ocl_context, 1, &ocl_kernel_source, NULL, &err);
	if (err != CL_SUCCESS)
	{
		printf ("ocl_init: ERROR clCreateProgramWithSource failed: %d\n", err);
		clReleaseCommandQueue (ocl_queue);
		clReleaseContext (ocl_context);
		return (-1);
	}

	err = clBuildProgram (ocl_program, 1, &ocl_device, NULL, NULL, NULL);
	if (err != CL_SUCCESS)
	{
		clGetProgramBuildInfo (ocl_program, ocl_device, CL_PROGRAM_BUILD_LOG, sizeof (build_log), build_log, &log_size);
		printf ("ocl_init: ERROR clBuildProgram failed: %d\nBuild log:\n%s\n", err, build_log);
		clReleaseProgram (ocl_program);
		clReleaseCommandQueue (ocl_queue);
		clReleaseContext (ocl_context);
		return (-1);
	}

	kern_add_scalar = clCreateKernel (ocl_program, "kern_add_scalar", &err);
	kern_sub_scalar = clCreateKernel (ocl_program, "kern_sub_scalar", &err);
	kern_mul_scalar = clCreateKernel (ocl_program, "kern_mul_scalar", &err);
	kern_div_scalar = clCreateKernel (ocl_program, "kern_div_scalar", &err);
	kern_add_array = clCreateKernel (ocl_program, "kern_add_array", &err);
	kern_sub_array = clCreateKernel (ocl_program, "kern_sub_array", &err);
	kern_mul_array = clCreateKernel (ocl_program, "kern_mul_array", &err);
	kern_div_array = clCreateKernel (ocl_program, "kern_div_array", &err);
	kern_min = clCreateKernel (ocl_program, "kern_min", &err);
	kern_max = clCreateKernel (ocl_program, "kern_max", &err);
	kern_sum = clCreateKernel (ocl_program, "kern_sum", &err);
	kern_search = clCreateKernel (ocl_program, "kern_search", &err);
	kern_clear = clCreateKernel (ocl_program, "kern_clear", &err);
	kern_copy = clCreateKernel (ocl_program, "kern_copy", &err);
	kern_init_byte = clCreateKernel (ocl_program, "kern_init_byte", &err);
	kern_init_int64 = clCreateKernel (ocl_program, "kern_init_int64", &err);

	if (err != CL_SUCCESS)
	{
		printf ("ocl_init: ERROR clCreateKernel failed: %d\n", err);
		clReleaseProgram (ocl_program);
		clReleaseCommandQueue (ocl_queue);
		clReleaseContext (ocl_context);
		return (-1);
	}

	ocl_initialized = 1;
	//printf ("ocl_init: OpenCL initialized successfully\n");
	return (0);
}

// release all OpenCL resources
S2 ocl_shutdown (void)
{
	if (!ocl_initialized) return (0);

	clReleaseKernel (kern_add_scalar);
	clReleaseKernel (kern_sub_scalar);
	clReleaseKernel (kern_mul_scalar);
	clReleaseKernel (kern_div_scalar);
	clReleaseKernel (kern_add_array);
	clReleaseKernel (kern_sub_array);
	clReleaseKernel (kern_mul_array);
	clReleaseKernel (kern_div_array);
	clReleaseKernel (kern_min);
	clReleaseKernel (kern_max);
	clReleaseKernel (kern_sum);
	clReleaseKernel (kern_search);
	clReleaseKernel (kern_clear);
	clReleaseKernel (kern_copy);
	clReleaseKernel (kern_init_byte);
	clReleaseKernel (kern_init_int64);
	clReleaseProgram (ocl_program);
	clReleaseCommandQueue (ocl_queue);
	clReleaseContext (ocl_context);

	ocl_delete_slots ();

	ocl_initialized = 0;
	return (0);
}

// ensure OpenCL is initialized
S2 ocl_ensure_init (void)
{
	if (ocl_initialized) return (0);
	return (ocl_init ());
}


U1 *mvect_opencl_shutdown (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	ocl_shutdown ();
	ocl_double_shutdown ();

	return (sp);
}

// ============================================================================
// oclmath_push / oclmath_pull: copy arrays between VM heap and GPU slots
//
// push: (src_ptr, slot, start, end)  -> upload VM array range into a slot
// pull: (slot, dst_ptr, start, end)  -> download a slot range into a VM array
//
// The slots hold raw 8-byte elements (int64 and double have the same byte
// size), so no type information is needed here.
// ============================================================================

U1 *oclmath_push (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 slot ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("oclmath_push: ERROR: stack corrupt!\n"); return (NULL); }
	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("oclmath_push: ERROR: stack corrupt!\n"); return (NULL); }
	sp = stpopi ((U1 *) &slot, sp, sp_top);
	if (sp == NULL) { printf ("oclmath_push: ERROR: stack corrupt!\n"); return (NULL); }
	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL) { printf ("oclmath_push: ERROR: stack corrupt!\n"); return (NULL); }

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0) { printf ("oclmath_push ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_src_ptr, end * offset) != 0) { printf ("oclmath_push ERROR: src overflow!\n"); return (NULL); }
	#endif

	if (ocl_ensure_init () != 0) { printf ("oclmath_push: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = end - start + 1;
	if (count <= 0) { printf ("oclmath_push: ERROR: count <= 0!\n"); return (NULL); }
	size_t nbytes = count * sizeof (S8);

	cl_mem buf = ocl_get_slot ((S4) slot, nbytes);
	if (buf == NULL) { printf ("oclmath_push: ERROR: no slot buffer!\n"); return (NULL); }

	cl_int err = clEnqueueWriteBuffer (ocl_queue, buf, CL_FALSE, 0, nbytes, &data[array_data_src_ptr + (start * offset)], 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("oclmath_push: ERROR clEnqueueWriteBuffer: %d\n", err); return (NULL); }

	return (sp);
}

U1 *oclmath_pull (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 slot ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("oclmath_pull: ERROR: stack corrupt!\n"); return (NULL); }
	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("oclmath_pull: ERROR: stack corrupt!\n"); return (NULL); }
	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL) { printf ("oclmath_pull: ERROR: stack corrupt!\n"); return (NULL); }
	sp = stpopi ((U1 *) &slot, sp, sp_top);
	if (sp == NULL) { printf ("oclmath_pull: ERROR: stack corrupt!\n"); return (NULL); }

	#if BOUNDSCHECK
	if (memory_bounds (array_data_dst_ptr, start * offset) != 0) { printf ("oclmath_pull ERROR: dst overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0) { printf ("oclmath_pull ERROR: dst overflow!\n"); return (NULL); }
	#endif

	if (ocl_ensure_init () != 0) { printf ("oclmath_pull: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = end - start + 1;
	if (count <= 0) { printf ("oclmath_pull: ERROR: count <= 0!\n"); return (NULL); }
	size_t nbytes = count * sizeof (S8);

	cl_mem buf = ocl_get_slot ((S4) slot, nbytes);
	if (buf == NULL) { printf ("oclmath_pull: ERROR: no slot buffer!\n"); return (NULL); }

	cl_int err = clEnqueueReadBuffer (ocl_queue, buf, CL_TRUE, 0, nbytes, &data[array_data_dst_ptr + (start * offset)], 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("oclmath_pull: ERROR clEnqueueReadBuffer: %d\n", err); return (NULL); }

	return (sp);
}

// ============================================================================
// sort functions (CPU-only, not suitable for GPU) ===========================
// ============================================================================

int compare_int_inc (const void* a, const void* b)
{
    S8 arg1 = *(const S8*) a;
    S8 arg2 = *(const S8*) b;

    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

int compare_int_dec (const void* a, const void* b)
{
    S8 arg1 = *(const S8*) a;
    S8 arg2 = *(const S8*) b;

    if (arg1 > arg2) return -1;
    if (arg1 < arg2) return 1;
    return 0;
}

U1 *mvect_sort_int_inc (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	S8 *src_ptr;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_sort_int_inc: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_sort_int_inc: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_sort_int_inc ERROR: src overflow!\n");
		return (NULL);
	}
	#endif

	src_ptr = (S8 *) &data[array_data_src_ptr];

    qsort (src_ptr, end + 1, sizeof (S8), compare_int_inc);
	return (sp);
}

U1 *mvect_sort_int_dec (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	S8 *src_ptr;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_sort_int_dec: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_sort_int_dec: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_sort_int_dec ERROR: src overflow!\n");
		return (NULL);
	}
	#endif

	src_ptr = (S8 *) &data[array_data_src_ptr];

    qsort (src_ptr, end + 1, sizeof (S8), compare_int_dec);
	return (sp);
}

// ============================================================================
// min/max functions (OpenCL parallel reduction)
// ============================================================================

U1 *mvect_min_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	S8 min ALIGN = 0;
	S8 count ALIGN;
	S8 *src_ptr;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_min_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_min_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_min_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_min_int ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_min_int ERROR: src overflow!\n");
		return (NULL);
	}
	#endif

	src_ptr = (S8 *) &data[array_data_src_ptr + (start * offset)];
	min = *src_ptr;
	count = end - start + 1;

	if (count <= 1)
	{
		sp = stpushi (min, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("mvect_min_int: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	if (ocl_ensure_init () != 0)
	{
		printf ("mvect_min_int: ERROR: OpenCL init failed!\n");
		return (NULL);
	}

	cl_int err;
	cl_mem buf_src = clCreateBuffer (ocl_context, CL_MEM_READ_ONLY, count * sizeof (S8), NULL, &err);
	if (err != CL_SUCCESS)
	{
		printf ("mvect_min_int: ERROR clCreateBuffer src: %d\n", err);
		return (NULL);
	}

	cl_mem buf_result = clCreateBuffer (ocl_context, CL_MEM_READ_WRITE, sizeof (S8), NULL, &err);
	if (err != CL_SUCCESS)
	{
		printf ("mvect_min_int: ERROR clCreateBuffer result: %d\n", err);
		clReleaseMemObject (buf_src);
		return (NULL);
	}

	err = clEnqueueWriteBuffer (ocl_queue, buf_src, CL_TRUE, 0, count * sizeof (S8), src_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_min_int: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_result, CL_TRUE, 0, sizeof (S8), &min, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_min_int: ERROR clEnqueueWriteBuffer result: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	size_t global_size = count;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	S8 k_start = 0;
	S8 k_end = count - 1;
	clSetKernelArg (kern_min, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_min, 1, sizeof (cl_mem), &buf_result);
	clSetKernelArg (kern_min, 2, sizeof (S8), &k_start);
	clSetKernelArg (kern_min, 3, sizeof (S8), &k_end);

	err = clEnqueueNDRangeKernel (ocl_queue, kern_min, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_min_int: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	clFinish (ocl_queue);

	err = clEnqueueReadBuffer (ocl_queue, buf_result, CL_TRUE, 0, sizeof (S8), &min, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_min_int: ERROR clEnqueueReadBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	clReleaseMemObject (buf_src);
	clReleaseMemObject (buf_result);

	sp = stpushi (min, sp, sp_bottom);
	if (sp == NULL)
	{
		printf ("mvect_min_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *mvect_max_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	S8 max ALIGN = 0;
	S8 count ALIGN;
	S8 *src_ptr;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_max_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_max_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_max_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_max_int ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_max_int ERROR: src overflow!\n");
		return (NULL);
	}
	#endif

	src_ptr = (S8 *) &data[array_data_src_ptr + (start * offset)];
	max = *src_ptr;
	count = end - start + 1;

	if (count <= 1)
	{
		sp = stpushi (max, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("mvect_max_int: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	if (ocl_ensure_init () != 0)
	{
		printf ("mvect_max_int: ERROR: OpenCL init failed!\n");
		return (NULL);
	}

	cl_int err;
	cl_mem buf_src = clCreateBuffer (ocl_context, CL_MEM_READ_ONLY, count * sizeof (S8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_max_int: ERROR clCreateBuffer src: %d\n", err); return (NULL); }

	cl_mem buf_result = clCreateBuffer (ocl_context, CL_MEM_READ_WRITE, sizeof (S8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_max_int: ERROR clCreateBuffer result: %d\n", err); clReleaseMemObject (buf_src); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_src, CL_TRUE, 0, count * sizeof (S8), src_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_max_int: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_result, CL_TRUE, 0, sizeof (S8), &max, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_max_int: ERROR clEnqueueWriteBuffer result: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	size_t global_size = count;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	S8 k_start = 0;
	S8 k_end = count - 1;
	clSetKernelArg (kern_max, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_max, 1, sizeof (cl_mem), &buf_result);
	clSetKernelArg (kern_max, 2, sizeof (S8), &k_start);
	clSetKernelArg (kern_max, 3, sizeof (S8), &k_end);

	err = clEnqueueNDRangeKernel (ocl_queue, kern_max, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_max_int: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	clFinish (ocl_queue);

	err = clEnqueueReadBuffer (ocl_queue, buf_result, CL_TRUE, 0, sizeof (S8), &max, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_max_int: ERROR clEnqueueReadBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	clReleaseMemObject (buf_src);
	clReleaseMemObject (buf_result);

	sp = stpushi (max, sp, sp_bottom);
	if (sp == NULL)
	{
		printf ("mvect_max_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

// ============================================================================
// average function (OpenCL parallel sum reduction)
// ============================================================================

U1 *mvect_average_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	S8 sum ALIGN = 0;
	S8 count ALIGN;
	F8 average ALIGN = 0.0;
	S8 *src_ptr;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_average_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_average_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_average_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_average_int ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_average_int ERROR: src overflow!\n");
		return (NULL);
	}
	#endif

	src_ptr = (S8 *) &data[array_data_src_ptr + (start * offset)];
	count = end - start + 1;

	if (count <= 0)
	{
		printf ("mvect_average_int: ERROR: empty range!\n");
		return (NULL);
	}

	if (count <= 1)
	{
		average = (F8) *src_ptr;
		sp = stpushd (average, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("mvect_average_int: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	if (ocl_ensure_init () != 0)
	{
		printf ("mvect_average_int: ERROR: OpenCL init failed!\n");
		return (NULL);
	}

	cl_int err;
	cl_mem buf_src = clCreateBuffer (ocl_context, CL_MEM_READ_ONLY, count * sizeof (S8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_average_int: ERROR clCreateBuffer src: %d\n", err); return (NULL); }

	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;

	size_t num_groups = (count + local_size - 1) / local_size;
	cl_mem buf_partial = clCreateBuffer (ocl_context, CL_MEM_READ_WRITE, num_groups * sizeof (S8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_average_int: ERROR clCreateBuffer partial: %d\n", err); clReleaseMemObject (buf_src); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_src, CL_TRUE, 0, count * sizeof (S8), src_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_average_int: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_partial); return (NULL); }

	size_t global_size = num_groups * local_size;

	S8 k_start = 0;
	S8 k_end = count - 1;
	clSetKernelArg (kern_sum, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_sum, 1, sizeof (cl_mem), &buf_partial);
	clSetKernelArg (kern_sum, 2, sizeof (S8), &k_start);
	clSetKernelArg (kern_sum, 3, sizeof (S8), &k_end);

	err = clEnqueueNDRangeKernel (ocl_queue, kern_sum, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_average_int: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_partial); return (NULL); }

	clFinish (ocl_queue);

	S8 *partial_sums = (S8 *) malloc (num_groups * sizeof (S8));
	if (partial_sums == NULL)
	{
		printf ("mvect_average_int: ERROR: malloc failed!\n");
		clReleaseMemObject (buf_src);
		clReleaseMemObject (buf_partial);
		return (NULL);
	}

	err = clEnqueueReadBuffer (ocl_queue, buf_partial, CL_TRUE, 0, num_groups * sizeof (S8), partial_sums, 0, NULL, NULL);
	if (err != CL_SUCCESS)
	{
		printf ("mvect_average_int: ERROR clEnqueueReadBuffer: %d\n", err);
		free (partial_sums);
		clReleaseMemObject (buf_src);
		clReleaseMemObject (buf_partial);
		return (NULL);
	}

	sum = 0;
	for (S8 g = 0; g < (S8) num_groups; g++)
	{
		sum += partial_sums[g];
	}

	free (partial_sums);
	clReleaseMemObject (buf_src);
	clReleaseMemObject (buf_partial);

	average = (F8) sum / count;

	sp = stpushd (average, sp, sp_bottom);
	if (sp == NULL)
	{
		printf ("mvect_average_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

// ============================================================================
// array copy (OpenCL)
// ============================================================================

U1 *mvect_array_copy (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN;
	S8 real_ind_start ALIGN;
	S8 real_ind_end ALIGN;

	sp = stpopi ((U1 *) &offset, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_array_copy: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_array_copy: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_array_copy: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_array_copy: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_array_copy: ERROR: stack corrupt!\n");
		return (NULL);
	}

	real_ind_start = start * offset;
	real_ind_end = end * offset;

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, real_ind_start) != 0)
	{
		printf ("mvect_avray_copy ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, real_ind_end) != 0)
	{
		printf ("mvect_array_copy ERROR: src overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_dst_ptr, real_ind_start) != 0)
	{
		printf ("mvect_avray_copy ERROR: dst overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_dst_ptr, real_ind_end) != 0)
	{
		printf ("mvect_array_copy ERROR: dst overflow!\n");
		return (NULL);
	}
	#endif

	if (ocl_ensure_init () != 0)
	{
		printf ("mvect_array_copy: ERROR: OpenCL init failed!\n");
		return (NULL);
	}

	size_t num_bytes = (real_ind_end - real_ind_start + 1);

	cl_int err;
	cl_mem buf_src = clCreateBuffer (ocl_context, CL_MEM_READ_ONLY, num_bytes, NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_array_copy: ERROR clCreateBuffer src: %d\n", err); return (NULL); }

	cl_mem buf_dst = clCreateBuffer (ocl_context, CL_MEM_WRITE_ONLY, num_bytes, NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_array_copy: ERROR clCreateBuffer dst: %d\n", err); clReleaseMemObject (buf_src); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_src, CL_TRUE, 0, num_bytes, &data[array_data_src_ptr + real_ind_start], 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_copy: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	S8 zero_start = 0;
	S8 zero_end = num_bytes - 1;

	clSetKernelArg (kern_copy, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_copy, 1, sizeof (cl_mem), &buf_dst);
	clSetKernelArg (kern_copy, 2, sizeof (S8), &zero_start);
	clSetKernelArg (kern_copy, 3, sizeof (S8), &zero_end);

	size_t global_size = num_bytes;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	err = clEnqueueNDRangeKernel (ocl_queue, kern_copy, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_copy: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	clFinish (ocl_queue);

	err = clEnqueueReadBuffer (ocl_queue, buf_dst, CL_TRUE, 0, num_bytes, &data[array_data_dst_ptr + real_ind_start], 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_copy: ERROR clEnqueueReadBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	clReleaseMemObject (buf_src);
	clReleaseMemObject (buf_dst);

	return (sp);
}

// ============================================================================
// array clear (OpenCL)
// ============================================================================

U1 *mvect_array_clear (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN;
	S8 real_ind_start ALIGN;
	S8 real_ind_end ALIGN;

	sp = stpopi ((U1 *) &offset, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_array_clear: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_array_clear: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_array_clear: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_array_clear: ERROR: stack corrupt!\n");
		return (NULL);
	}

	real_ind_start = start * offset;
	real_ind_end = end * offset;

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, real_ind_start) != 0)
	{
		printf ("mvect_avray_clear ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, real_ind_end) != 0)
	{
		printf ("mvect_array_clear ERROR: src overflow!\n");
		return (NULL);
	}
	#endif

	if (ocl_ensure_init () != 0)
	{
		printf ("mvect_array_clear: ERROR: OpenCL init failed!\n");
		return (NULL);
	}

	size_t num_bytes = (real_ind_end - real_ind_start + 1);
	S8 zero_start = 0;
	S8 zero_end = num_bytes - 1;

	cl_int err;
	cl_mem buf_data = clCreateBuffer (ocl_context, CL_MEM_READ_WRITE, num_bytes, NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_array_clear: ERROR clCreateBuffer: %d\n", err); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_data, CL_TRUE, 0, num_bytes, &data[array_data_src_ptr + real_ind_start], 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_clear: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_data); return (NULL); }

	clSetKernelArg (kern_clear, 0, sizeof (cl_mem), &buf_data);
	clSetKernelArg (kern_clear, 1, sizeof (S8), &zero_start);
	clSetKernelArg (kern_clear, 2, sizeof (S8), &zero_end);

	size_t global_size = num_bytes;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	err = clEnqueueNDRangeKernel (ocl_queue, kern_clear, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_clear: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_data); return (NULL); }

	clFinish (ocl_queue);

	err = clEnqueueReadBuffer (ocl_queue, buf_data, CL_TRUE, 0, num_bytes, &data[array_data_src_ptr + real_ind_start], 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_clear: ERROR clEnqueueReadBuffer: %d\n", err); clReleaseMemObject (buf_data); return (NULL); }

	clReleaseMemObject (buf_data);

	return (sp);
}

// ============================================================================
// array search (OpenCL parallel search)
// ============================================================================

U1 *mvect_array_search_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	S8 real_ind_start ALIGN;
	S8 real_ind_end ALIGN;
	S8 ret ALIGN = -1;
	S8 search ALIGN = 0;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_array_search_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_array_search_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &search, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_array_search_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_array_search_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	real_ind_start = start * offset;
	real_ind_end = end * offset;

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, real_ind_start) != 0)
	{
		printf ("mvect_avray_search_int ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, real_ind_end) != 0)
	{
		printf ("mvect_array_search_int ERROR: src overflow!\n");
		return (NULL);
	}
	#endif

	S8 count = end - start + 1;
	S8 *src_ptr = (S8 *) &data[array_data_src_ptr + (start * offset)];

	if (count <= 0)
	{
		sp = stpushi (ret, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("mvect_array_search_int: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	if (ocl_ensure_init () != 0)
	{
		printf ("mvect_array_search_int: ERROR: OpenCL init failed!\n");
		return (NULL);
	}

	cl_int err;
	cl_mem buf_src = clCreateBuffer (ocl_context, CL_MEM_READ_ONLY, count * sizeof (S8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_array_search_int: ERROR clCreateBuffer src: %d\n", err); return (NULL); }

	S8 not_found = -1;
	cl_mem buf_result = clCreateBuffer (ocl_context, CL_MEM_READ_WRITE, sizeof (S8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_array_search_int: ERROR clCreateBuffer result: %d\n", err); clReleaseMemObject (buf_src); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_src, CL_TRUE, 0, count * sizeof (S8), src_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_search_int: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_result, CL_TRUE, 0, sizeof (S8), &not_found, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_search_int: ERROR clEnqueueWriteBuffer result: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	size_t global_size = count;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	S8 k_start = 0;
	S8 k_end = count - 1;
	clSetKernelArg (kern_search, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_search, 1, sizeof (cl_mem), &buf_result);
	clSetKernelArg (kern_search, 2, sizeof (S8), &search);
	clSetKernelArg (kern_search, 3, sizeof (S8), &k_start);
	clSetKernelArg (kern_search, 4, sizeof (S8), &k_end);

	err = clEnqueueNDRangeKernel (ocl_queue, kern_search, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_search_int: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	clFinish (ocl_queue);

	err = clEnqueueReadBuffer (ocl_queue, buf_result, CL_TRUE, 0, sizeof (S8), &ret, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_search_int: ERROR clEnqueueReadBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	clReleaseMemObject (buf_src);
	clReleaseMemObject (buf_result);

	// convert buffer-relative index to absolute element index
	if (ret != -1) ret = ret + start;

	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		printf ("mvect_array_search_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

// ============================================================================
// scalar operations (OpenCL element-wise)
// ============================================================================

U1 *mvect_add_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	S8 number ALIGN = 0;

	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_int: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &number, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_int: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_int: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_int: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_int: ERROR: stack corrupt!\n"); return (NULL); }

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0) { printf ("mvect_add_int ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_src_ptr, end * offset) != 0) { printf ("mvect_add_int ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_dst_ptr, start * offset) != 0) { printf ("mvect_add_int ERROR: dst overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0) { printf ("mvect_add_int ERROR: dst overflow!\n"); return (NULL); }
	#endif

	if (ocl_ensure_init () != 0) { printf ("mvect_add_int: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = end - start + 1;
	S8 *src_ptr = (S8 *) &data[array_data_src_ptr + (start * offset)];
	S8 *dst_ptr = (S8 *) &data[array_data_dst_ptr + (start * offset)];

	cl_int err;
	cl_mem buf_src = clCreateBuffer (ocl_context, CL_MEM_READ_ONLY, count * sizeof (S8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_add_int: ERROR clCreateBuffer src: %d\n", err); return (NULL); }
	cl_mem buf_dst = clCreateBuffer (ocl_context, CL_MEM_WRITE_ONLY, count * sizeof (S8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_add_int: ERROR clCreateBuffer dst: %d\n", err); clReleaseMemObject (buf_src); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_src, CL_TRUE, 0, count * sizeof (S8), src_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_add_int: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	size_t global_size = count;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	S8 k_start = 0;
	S8 k_end = count - 1;
	clSetKernelArg (kern_add_scalar, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_add_scalar, 1, sizeof (cl_mem), &buf_dst);
	clSetKernelArg (kern_add_scalar, 2, sizeof (S8), &number);
	clSetKernelArg (kern_add_scalar, 3, sizeof (S8), &k_start);
	clSetKernelArg (kern_add_scalar, 4, sizeof (S8), &k_end);

	err = clEnqueueNDRangeKernel (ocl_queue, kern_add_scalar, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_add_int: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	clFinish (ocl_queue);

	err = clEnqueueReadBuffer (ocl_queue, buf_dst, CL_TRUE, 0, count * sizeof (S8), dst_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_add_int: ERROR clEnqueueReadBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	clReleaseMemObject (buf_src);
	clReleaseMemObject (buf_dst);

	return (sp);
}

U1 *mvect_sub_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	S8 number ALIGN = 0;

	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_int: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &number, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_int: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_int: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_int: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_int: ERROR: stack corrupt!\n"); return (NULL); }

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0) { printf ("mvect_sub_int ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_src_ptr, end * offset) != 0) { printf ("mvect_sub_int ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_dst_ptr, start * offset) != 0) { printf ("mvect_sub_int ERROR: dst overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0) { printf ("mvect_sub_int ERROR: dst overflow!\n"); return (NULL); }
	#endif

	if (ocl_ensure_init () != 0) { printf ("mvect_sub_int: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = end - start + 1;
	S8 *src_ptr = (S8 *) &data[array_data_src_ptr + (start * offset)];
	S8 *dst_ptr = (S8 *) &data[array_data_dst_ptr + (start * offset)];

	cl_int err;
	cl_mem buf_src = clCreateBuffer (ocl_context, CL_MEM_READ_ONLY, count * sizeof (S8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_sub_int: ERROR clCreateBuffer src: %d\n", err); return (NULL); }
	cl_mem buf_dst = clCreateBuffer (ocl_context, CL_MEM_WRITE_ONLY, count * sizeof (S8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_sub_int: ERROR clCreateBuffer dst: %d\n", err); clReleaseMemObject (buf_src); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_src, CL_TRUE, 0, count * sizeof (S8), src_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_sub_int: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	size_t global_size = count;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	S8 k_start = 0;
	S8 k_end = count - 1;
	clSetKernelArg (kern_sub_scalar, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_sub_scalar, 1, sizeof (cl_mem), &buf_dst);
	clSetKernelArg (kern_sub_scalar, 2, sizeof (S8), &number);
	clSetKernelArg (kern_sub_scalar, 3, sizeof (S8), &k_start);
	clSetKernelArg (kern_sub_scalar, 4, sizeof (S8), &k_end);

	err = clEnqueueNDRangeKernel (ocl_queue, kern_sub_scalar, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_sub_int: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	clFinish (ocl_queue);

	err = clEnqueueReadBuffer (ocl_queue, buf_dst, CL_TRUE, 0, count * sizeof (S8), dst_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_sub_int: ERROR clEnqueueReadBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	clReleaseMemObject (buf_src);
	clReleaseMemObject (buf_dst);

	return (sp);
}

U1 *mvect_mul_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	S8 number ALIGN = 0;

	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_int: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &number, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_int: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_int: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_int: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_int: ERROR: stack corrupt!\n"); return (NULL); }

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0) { printf ("mvect_mul_int ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_src_ptr, end * offset) != 0) { printf ("mvect_mul_int ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_dst_ptr, start * offset) != 0) { printf ("mvect_mul_int ERROR: dst overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0) { printf ("mvect_mul_int ERROR: dst overflow!\n"); return (NULL); }
	#endif

	if (ocl_ensure_init () != 0) { printf ("mvect_mul_int: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = end - start + 1;
	S8 *src_ptr = (S8 *) &data[array_data_src_ptr + (start * offset)];
	S8 *dst_ptr = (S8 *) &data[array_data_dst_ptr + (start * offset)];

	cl_int err;
	cl_mem buf_src = clCreateBuffer (ocl_context, CL_MEM_READ_ONLY, count * sizeof (S8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_mul_int: ERROR clCreateBuffer src: %d\n", err); return (NULL); }
	cl_mem buf_dst = clCreateBuffer (ocl_context, CL_MEM_WRITE_ONLY, count * sizeof (S8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_mul_int: ERROR clCreateBuffer dst: %d\n", err); clReleaseMemObject (buf_src); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_src, CL_TRUE, 0, count * sizeof (S8), src_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_mul_int: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	size_t global_size = count;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	S8 k_start = 0;
	S8 k_end = count - 1;
	clSetKernelArg (kern_mul_scalar, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_mul_scalar, 1, sizeof (cl_mem), &buf_dst);
	clSetKernelArg (kern_mul_scalar, 2, sizeof (S8), &number);
	clSetKernelArg (kern_mul_scalar, 3, sizeof (S8), &k_start);
	clSetKernelArg (kern_mul_scalar, 4, sizeof (S8), &k_end);

	err = clEnqueueNDRangeKernel (ocl_queue, kern_mul_scalar, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_mul_int: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	clFinish (ocl_queue);

	err = clEnqueueReadBuffer (ocl_queue, buf_dst, CL_TRUE, 0, count * sizeof (S8), dst_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_mul_int: ERROR clEnqueueReadBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	clReleaseMemObject (buf_src);
	clReleaseMemObject (buf_dst);

	return (sp);
}

U1 *mvect_div_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	S8 number ALIGN = 0;

	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_int: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &number, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_int: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_int: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_int: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_int: ERROR: stack corrupt!\n"); return (NULL); }

	if (number == 0)
	{
		printf ("mvect_div_int: ERROR division by zero!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0) { printf ("mvect_div_int ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_src_ptr, end * offset) != 0) { printf ("mvect_div_int ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_dst_ptr, start * offset) != 0) { printf ("mvect_div_int ERROR: dst overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0) { printf ("mvect_div_int ERROR: dst overflow!\n"); return (NULL); }
	#endif

	if (ocl_ensure_init () != 0) { printf ("mvect_div_int: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = end - start + 1;
	S8 *src_ptr = (S8 *) &data[array_data_src_ptr + (start * offset)];
	S8 *dst_ptr = (S8 *) &data[array_data_dst_ptr + (start * offset)];

	cl_int err;
	cl_mem buf_src = clCreateBuffer (ocl_context, CL_MEM_READ_ONLY, count * sizeof (S8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_div_int: ERROR clCreateBuffer src: %d\n", err); return (NULL); }
	cl_mem buf_dst = clCreateBuffer (ocl_context, CL_MEM_WRITE_ONLY, count * sizeof (S8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_div_int: ERROR clCreateBuffer dst: %d\n", err); clReleaseMemObject (buf_src); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_src, CL_TRUE, 0, count * sizeof (S8), src_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_div_int: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	size_t global_size = count;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	S8 k_start = 0;
	S8 k_end = count - 1;
	clSetKernelArg (kern_div_scalar, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_div_scalar, 1, sizeof (cl_mem), &buf_dst);
	clSetKernelArg (kern_div_scalar, 2, sizeof (S8), &number);
	clSetKernelArg (kern_div_scalar, 3, sizeof (S8), &k_start);
	clSetKernelArg (kern_div_scalar, 4, sizeof (S8), &k_end);

	err = clEnqueueNDRangeKernel (ocl_queue, kern_div_scalar, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_div_int: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	clFinish (ocl_queue);

	err = clEnqueueReadBuffer (ocl_queue, buf_dst, CL_TRUE, 0, count * sizeof (S8), dst_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_div_int: ERROR clEnqueueReadBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	clReleaseMemObject (buf_src);
	clReleaseMemObject (buf_dst);

	return (sp);
}

// ============================================================================
// array operations (OpenCL element-wise)
// ============================================================================

U1 *mvect_add_int_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 slot_src ALIGN;
	S8 slot_src2 ALIGN;
	S8 slot_dst ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;

	sp = stpopi ((U1 *) &slot_dst, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &slot_src2, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &slot_src, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	if (ocl_ensure_init () != 0) { printf ("mvect_add_int_array: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = end - start + 1;
	if (count <= 0) { printf ("mvect_add_int_array: ERROR: count <= 0!\n"); return (NULL); }
	size_t nbytes = count * sizeof (S8);

	cl_mem buf_src = ocl_get_slot ((S4) slot_src, nbytes);
	if (buf_src == NULL) { printf ("mvect_add_int_array: ERROR: no slot buffer for source slot!\n"); return (NULL); }
	cl_mem buf_src2 = ocl_get_slot ((S4) slot_src2, nbytes);
	if (buf_src2 == NULL) { printf ("mvect_add_int_array: ERROR: no slot buffer for source2 slot!\n"); return (NULL); }
	cl_mem buf_dst = ocl_get_slot ((S4) slot_dst, nbytes);
	if (buf_dst == NULL) { printf ("mvect_add_int_array: ERROR: no slot buffer for destination slot!\n"); return (NULL); }

	size_t global_size = count;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	S8 k_start = 0;
	S8 k_end = count - 1;
	clSetKernelArg (kern_add_array, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_add_array, 1, sizeof (cl_mem), &buf_src2);
	clSetKernelArg (kern_add_array, 2, sizeof (cl_mem), &buf_dst);
	clSetKernelArg (kern_add_array, 3, sizeof (S8), &k_start);
	clSetKernelArg (kern_add_array, 4, sizeof (S8), &k_end);

	cl_int err = clEnqueueNDRangeKernel (ocl_queue, kern_add_array, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_add_int_array: ERROR clEnqueueNDRangeKernel: %d\n", err); return (NULL); }

	return (sp);
}

U1 *mvect_sub_int_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 slot_src ALIGN;
	S8 slot_src2 ALIGN;
	S8 slot_dst ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;

	sp = stpopi ((U1 *) &slot_dst, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &slot_src2, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &slot_src, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	if (ocl_ensure_init () != 0) { printf ("mvect_sub_int_array: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = end - start + 1;
	if (count <= 0) { printf ("mvect_sub_int_array: ERROR: count <= 0!\n"); return (NULL); }
	size_t nbytes = count * sizeof (S8);

	cl_mem buf_src = ocl_get_slot ((S4) slot_src, nbytes);
	if (buf_src == NULL) { printf ("mvect_sub_int_array: ERROR: no slot buffer for source slot!\n"); return (NULL); }
	cl_mem buf_src2 = ocl_get_slot ((S4) slot_src2, nbytes);
	if (buf_src2 == NULL) { printf ("mvect_sub_int_array: ERROR: no slot buffer for source2 slot!\n"); return (NULL); }
	cl_mem buf_dst = ocl_get_slot ((S4) slot_dst, nbytes);
	if (buf_dst == NULL) { printf ("mvect_sub_int_array: ERROR: no slot buffer for destination slot!\n"); return (NULL); }

	size_t global_size = count;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	S8 k_start = 0;
	S8 k_end = count - 1;
	clSetKernelArg (kern_sub_array, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_sub_array, 1, sizeof (cl_mem), &buf_src2);
	clSetKernelArg (kern_sub_array, 2, sizeof (cl_mem), &buf_dst);
	clSetKernelArg (kern_sub_array, 3, sizeof (S8), &k_start);
	clSetKernelArg (kern_sub_array, 4, sizeof (S8), &k_end);

	cl_int err = clEnqueueNDRangeKernel (ocl_queue, kern_sub_array, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_sub_int_array: ERROR clEnqueueNDRangeKernel: %d\n", err); return (NULL); }

	return (sp);
}

U1 *mvect_mul_int_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 slot_src ALIGN;
	S8 slot_src2 ALIGN;
	S8 slot_dst ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;

	sp = stpopi ((U1 *) &slot_dst, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &slot_src2, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &slot_src, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	if (ocl_ensure_init () != 0) { printf ("mvect_mul_int_array: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = end - start + 1;
	if (count <= 0) { printf ("mvect_mul_int_array: ERROR: count <= 0!\n"); return (NULL); }
	size_t nbytes = count * sizeof (S8);

	cl_mem buf_src = ocl_get_slot ((S4) slot_src, nbytes);
	if (buf_src == NULL) { printf ("mvect_mul_int_array: ERROR: no slot buffer for source slot!\n"); return (NULL); }
	cl_mem buf_src2 = ocl_get_slot ((S4) slot_src2, nbytes);
	if (buf_src2 == NULL) { printf ("mvect_mul_int_array: ERROR: no slot buffer for source2 slot!\n"); return (NULL); }
	cl_mem buf_dst = ocl_get_slot ((S4) slot_dst, nbytes);
	if (buf_dst == NULL) { printf ("mvect_mul_int_array: ERROR: no slot buffer for destination slot!\n"); return (NULL); }

	size_t global_size = count;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	S8 k_start = 0;
	S8 k_end = count - 1;
	clSetKernelArg (kern_mul_array, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_mul_array, 1, sizeof (cl_mem), &buf_src2);
	clSetKernelArg (kern_mul_array, 2, sizeof (cl_mem), &buf_dst);
	clSetKernelArg (kern_mul_array, 3, sizeof (S8), &k_start);
	clSetKernelArg (kern_mul_array, 4, sizeof (S8), &k_end);

	cl_int err = clEnqueueNDRangeKernel (ocl_queue, kern_mul_array, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_mul_int_array: ERROR clEnqueueNDRangeKernel: %d\n", err); return (NULL); }

	return (sp);
}

U1 *mvect_div_int_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 slot_src ALIGN;
	S8 slot_src2 ALIGN;
	S8 slot_dst ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;

	sp = stpopi ((U1 *) &slot_dst, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &slot_src2, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &slot_src, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_int_array: ERROR: stack corrupt!\n"); return (NULL); }

	if (ocl_ensure_init () != 0) { printf ("mvect_div_int_array: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = end - start + 1;
	if (count <= 0) { printf ("mvect_div_int_array: ERROR: count <= 0!\n"); return (NULL); }
	size_t nbytes = count * sizeof (S8);

	cl_mem buf_src = ocl_get_slot ((S4) slot_src, nbytes);
	if (buf_src == NULL) { printf ("mvect_div_int_array: ERROR: no slot buffer for source slot!\n"); return (NULL); }
	cl_mem buf_src2 = ocl_get_slot ((S4) slot_src2, nbytes);
	if (buf_src2 == NULL) { printf ("mvect_div_int_array: ERROR: no slot buffer for source2 slot!\n"); return (NULL); }
	cl_mem buf_dst = ocl_get_slot ((S4) slot_dst, nbytes);
	if (buf_dst == NULL) { printf ("mvect_div_int_array: ERROR: no slot buffer for destination slot!\n"); return (NULL); }

	size_t global_size = count;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	S8 k_start = 0;
	S8 k_end = count - 1;
	clSetKernelArg (kern_div_array, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_div_array, 1, sizeof (cl_mem), &buf_src2);
	clSetKernelArg (kern_div_array, 2, sizeof (cl_mem), &buf_dst);
	clSetKernelArg (kern_div_array, 3, sizeof (S8), &k_start);
	clSetKernelArg (kern_div_array, 4, sizeof (S8), &k_end);

	cl_int err = clEnqueueNDRangeKernel (ocl_queue, kern_div_array, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_div_int_array: ERROR clEnqueueNDRangeKernel: %d\n", err); return (NULL); }

	return (sp);
}

// ============================================================================
// array init functions (OpenCL)
// ============================================================================

U1 *mvect_array_init_byte (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 step ALIGN;
	S8 value ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL) { printf ("mvect_array_init_byte: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &step, sp, sp_top);
	if (sp == NULL) { printf ("mvect_array_init_byte: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("mvect_array_init_byte: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("mvect_array_init_byte: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL) { printf ("mvect_aray_init_byte: ERROR: stack corrupt!\n"); return (NULL); }

    #if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start) != 0) { printf ("mvect_array_init_byte: ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_src_ptr, end - 1) != 0) { printf ("mvect_array_init_byte: ERROR: src overflow!\n"); return (NULL); }
    #endif

	if (ocl_ensure_init () != 0) { printf ("mvect_array_init_byte: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = (end - start + step - 1) / step;
	if (count <= 0) return (sp);

	cl_int err;
	cl_mem buf_data = clCreateBuffer (ocl_context, CL_MEM_READ_WRITE, end - start, NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_array_init_byte: ERROR clCreateBuffer: %d\n", err); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_data, CL_TRUE, 0, end - start, &data[array_data_src_ptr + start], 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_init_byte: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_data); return (NULL); }

	U1 byte_val = (U1) value;

	size_t global_size = count;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	clSetKernelArg (kern_init_byte, 0, sizeof (cl_mem), &buf_data);
	clSetKernelArg (kern_init_byte, 1, sizeof (U1), &byte_val);
	clSetKernelArg (kern_init_byte, 2, sizeof (S8), &start);
	clSetKernelArg (kern_init_byte, 3, sizeof (S8), &end);
	clSetKernelArg (kern_init_byte, 4, sizeof (S8), &step);

	err = clEnqueueNDRangeKernel (ocl_queue, kern_init_byte, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_init_byte: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_data); return (NULL); }

	clFinish (ocl_queue);

	err = clEnqueueReadBuffer (ocl_queue, buf_data, CL_TRUE, 0, end - start, &data[array_data_src_ptr + start], 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_init_byte: ERROR clEnqueueReadBuffer: %d\n", err); clReleaseMemObject (buf_data); return (NULL); }

	clReleaseMemObject (buf_data);

	return (sp);
}

U1 *mvect_array_init_int64 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 step ALIGN;
	S8 value ALIGN;
	S8 offset ALIGN = 8;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL) { printf ("mvect_array_init_int64: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &step, sp, sp_top);
	if (sp == NULL) { printf ("mvect_array_init_int64: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("mvect_array_init_int64: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("mvect_array_init_int64: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL) { printf ("mvect_aray_init_int64: ERROR: stack corrupt!\n"); return (NULL); }

    #if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0) { printf ("mvect_array_init_int64: ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_src_ptr, (end - 1) * offset) != 0) { printf ("mvect_array_init_int64: ERROR: src overflow!\n"); return (NULL); }
    #endif

	if (ocl_ensure_init () != 0) { printf ("mvect_array_init_int64: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = (end - start + step - 1) / step;
	if (count <= 0) return (sp);

	S8 num_bytes = (end - start) * offset;
	S8 *src_ptr = (S8 *) &data[array_data_src_ptr + (start * offset)];

	cl_int err;
	cl_mem buf_data = clCreateBuffer (ocl_context, CL_MEM_READ_WRITE, num_bytes, NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_array_init_int64: ERROR clCreateBuffer: %d\n", err); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_data, CL_TRUE, 0, num_bytes, src_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_init_int64: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_data); return (NULL); }

	size_t global_size = count;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	clSetKernelArg (kern_init_int64, 0, sizeof (cl_mem), &buf_data);
	clSetKernelArg (kern_init_int64, 1, sizeof (S8), &value);
	clSetKernelArg (kern_init_int64, 2, sizeof (S8), &start);
	clSetKernelArg (kern_init_int64, 3, sizeof (S8), &end);
	clSetKernelArg (kern_init_int64, 4, sizeof (S8), &step);

	err = clEnqueueNDRangeKernel (ocl_queue, kern_init_int64, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_init_int64: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_data); return (NULL); }

	clFinish (ocl_queue);

	err = clEnqueueReadBuffer (ocl_queue, buf_data, CL_TRUE, 0, num_bytes, src_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_init_int64: ERROR clEnqueueReadBuffer: %d\n", err); clReleaseMemObject (buf_data); return (NULL); }

	clReleaseMemObject (buf_data);

	return (sp);
}
