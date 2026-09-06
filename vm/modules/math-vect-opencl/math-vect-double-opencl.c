/*
 * This file math-vect-double-opencl.c is part of L1vm.
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
#include <stdlib.h>
#include <string.h>
#include <CL/cl.h>

// protos
U1 *stpushb (U1 data, U1 *sp, U1 *sp_bottom);
U1 *stpopb (U1 *data, U1 *sp, U1 *sp_top);
U1 *stpushi (S8 data, U1 *sp, U1 *sp_bottom);
U1 *stpopi (U1 *data, U1 *sp, U1 *sp_top);
U1 *stpushd (F8 data, U1 *sp, U1 *sp_bottom);
U1 *stpopd (U1 *data, U1 *sp, U1 *sp_top);

extern S2 memory_bounds (S8 start, S8 offset_access);

// shared OpenCL infrastructure (defined in the int module)
extern cl_platform_id ocl_platform;
extern cl_device_id ocl_device;
extern cl_context ocl_context;
extern cl_command_queue ocl_queue;
extern size_t ocl_preferred_group_size;
extern S2 ocl_ensure_init (void);
extern cl_mem ocl_get_slot (S4 slot, size_t bytes);

static cl_program ocl_program;
static S8 ocl_double_initialized = 0;

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

static const char *ocl_kernel_source =
"typedef long S8;\n"
"typedef double F8;\n"
"\n"
"__kernel void kern_add_scalar(__global F8 *src, __global F8 *dst, F8 number, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end) dst[i] = src[i] + number;\n"
"}\n"
"\n"
"__kernel void kern_sub_scalar(__global F8 *src, __global F8 *dst, F8 number, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end) dst[i] = src[i] - number;\n"
"}\n"
"\n"
"__kernel void kern_mul_scalar(__global F8 *src, __global F8 *dst, F8 number, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end) dst[i] = src[i] * number;\n"
"}\n"
"\n"
"__kernel void kern_div_scalar(__global F8 *src, __global F8 *dst, F8 number, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end) dst[i] = src[i] / number;\n"
"}\n"
"\n"
"__kernel void kern_add_array(__global F8 *src, __global F8 *src2, __global F8 *dst, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end) dst[i] = src[i] + src2[i];\n"
"}\n"
"\n"
"__kernel void kern_sub_array(__global F8 *src, __global F8 *src2, __global F8 *dst, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end) dst[i] = src[i] - src2[i];\n"
"}\n"
"\n"
"__kernel void kern_mul_array(__global F8 *src, __global F8 *src2, __global F8 *dst, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end) dst[i] = src[i] * src2[i];\n"
"}\n"
"\n"
"__kernel void kern_div_array(__global F8 *src, __global F8 *src2, __global F8 *dst, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end)\n"
"    {\n"
"        if (src2[i] != 0.0) dst[i] = src[i] / src2[i];\n"
"    }\n"
"}\n"
"\n"
"__kernel void kern_min(__global F8 *src, __global F8 *result, S8 start, S8 end)\n"
"{\n"
"    size_t gid = get_global_id(0);\n"
"    size_t i = start + gid;\n"
"    size_t total = end - start + 1;\n"
"    __local F8 local_min[256];\n"
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
"__kernel void kern_max(__global F8 *src, __global F8 *result, S8 start, S8 end)\n"
"{\n"
"    size_t gid = get_global_id(0);\n"
"    size_t i = start + gid;\n"
"    size_t total = end - start + 1;\n"
"    __local F8 local_max[256];\n"
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
"__kernel void kern_sum(__global F8 *src, __global F8 *partial_sums, S8 start, S8 end)\n"
"{\n"
"    size_t gid = get_global_id(0);\n"
"    size_t i = start + gid;\n"
"    size_t lid = get_local_id(0);\n"
"    size_t group_size = get_local_size(0);\n"
"    __local F8 local_sum[256];\n"
"    if (i <= end) local_sum[lid] = src[i];\n"
"    else local_sum[lid] = 0.0;\n"
"    barrier(CLK_LOCAL_MEM_FENCE);\n"
"    for (size_t s = group_size / 2; s > 0; s >>= 1)\n"
"    {\n"
"        if (lid < s) local_sum[lid] += local_sum[lid + s];\n"
"        barrier(CLK_LOCAL_MEM_FENCE);\n"
"    }\n"
"    if (lid == 0) partial_sums[get_group_id(0)] = local_sum[0];\n"
"}\n"
"\n"
"__kernel void kern_search(__global F8 *src, __global S8 *result, F8 search_val, S8 start, S8 end)\n"
"{\n"
"    size_t i = start + get_global_id(0);\n"
"    if (i <= end && src[i] == search_val && result[0] == -1) result[0] = i;\n"
"}\n";

// ============================================================================
// OpenCL infrastructure (uses the shared context/queue of the int module)
// ============================================================================

// compile the double kernels on the shared context
S2 ocl_double_init (void)
{
	cl_int err;
	char build_log[4096];
	size_t log_size;

	ocl_program = clCreateProgramWithSource (ocl_context, 1, &ocl_kernel_source, NULL, &err);
	if (err != CL_SUCCESS)
	{
		printf ("ocl_double_init: ERROR clCreateProgramWithSource failed: %d\n", err);
		return (-1);
	}

	err = clBuildProgram (ocl_program, 1, &ocl_device, NULL, NULL, NULL);
	if (err != CL_SUCCESS)
	{
		clGetProgramBuildInfo (ocl_program, ocl_device, CL_PROGRAM_BUILD_LOG, sizeof (build_log), build_log, &log_size);
		printf ("ocl_double_init: ERROR clBuildProgram failed: %d\nBuild log:\n%s\n", err, build_log);
		clReleaseProgram (ocl_program);
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

	if (err != CL_SUCCESS)
	{
		printf ("ocl_double_init: ERROR clCreateKernel failed: %d\n", err);
		clReleaseProgram (ocl_program);
		return (-1);
	}

	ocl_double_initialized = 1;
	return (0);
}

// release the double kernels and program (shared context/queue stay alive)
S2 ocl_double_shutdown (void)
{
	if (!ocl_double_initialized) return (0);

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
	clReleaseProgram (ocl_program);

	ocl_double_initialized = 0;
	return (0);
}

// ensure OpenCL is initialized (base init first, then double kernels)
static S2 ocl_double_ensure_init (void)
{
	if (ocl_double_initialized) return (0);
	if (ocl_ensure_init () != 0) return (-1);
	return (ocl_double_init ());
}

// ============================================================================
// sort functions (CPU-only, not suitable for GPU) ===========================
// ============================================================================

int compare_double_inc (const void* a, const void* b)
{
    F8 arg1 = *(const F8*) a;
    F8 arg2 = *(const F8*) b;

    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

int compare_double_dec (const void* a, const void* b)
{
    F8 arg1 = *(const F8*) a;
    F8 arg2 = *(const F8*) b;

    if (arg1 > arg2) return -1;
    if (arg1 < arg2) return 1;
    return 0;
}

U1 *mvect_sort_double_inc (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	F8 *src_ptr;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_sort_double_inc: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_sort_double_inc: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_sort_double_inc ERROR: src overflow!\n");
		return (NULL);
	}
	#endif

	src_ptr = (F8 *) &data[array_data_src_ptr];

    qsort (src_ptr, end + 1, sizeof (F8), compare_double_inc);
	return (sp);
}

U1 *mvect_sort_double_dec (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	F8 *src_ptr;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_sort_double_dec: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_sort_double_dec: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_sort_double_dec ERROR: src overflow!\n");
		return (NULL);
	}
	#endif

	src_ptr = (F8 *) &data[array_data_src_ptr];

    qsort (src_ptr, end + 1, sizeof (F8), compare_double_dec);
	return (sp);
}

// ============================================================================
// min/max functions (OpenCL parallel reduction)
// ============================================================================

U1 *mvect_min_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	F8 min ALIGN = 0;
	S8 count ALIGN;
	F8 *src_ptr;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_min_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_min_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_min_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_min_double ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_min_double ERROR: src overflow!\n");
		return (NULL);
	}
	#endif

	src_ptr = (F8 *) &data[array_data_src_ptr + (start * offset)];
	min = *src_ptr;
	count = end - start + 1;

	if (count <= 1)
	{
		sp = stpushd (min, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("mvect_min_double: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	if (ocl_double_ensure_init () != 0)
	{
		printf ("mvect_min_double: ERROR: OpenCL init failed!\n");
		return (NULL);
	}

	cl_int err;
	cl_mem buf_src = clCreateBuffer (ocl_context, CL_MEM_READ_ONLY, count * sizeof (F8), NULL, &err);
	if (err != CL_SUCCESS)
	{
		printf ("mvect_min_double: ERROR clCreateBuffer src: %d\n", err);
		return (NULL);
	}

	cl_mem buf_result = clCreateBuffer (ocl_context, CL_MEM_READ_WRITE, sizeof (F8), NULL, &err);
	if (err != CL_SUCCESS)
	{
		printf ("mvect_min_double: ERROR clCreateBuffer result: %d\n", err);
		clReleaseMemObject (buf_src);
		return (NULL);
	}

	err = clEnqueueWriteBuffer (ocl_queue, buf_src, CL_TRUE, 0, count * sizeof (F8), src_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_min_double: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_result, CL_TRUE, 0, sizeof (F8), &min, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_min_double: ERROR clEnqueueWriteBuffer result: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

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
	if (err != CL_SUCCESS) { printf ("mvect_min_double: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	clFinish (ocl_queue);

	err = clEnqueueReadBuffer (ocl_queue, buf_result, CL_TRUE, 0, sizeof (F8), &min, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_min_double: ERROR clEnqueueReadBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	clReleaseMemObject (buf_src);
	clReleaseMemObject (buf_result);

	sp = stpushd (min, sp, sp_bottom);
	if (sp == NULL)
	{
		printf ("mvect_min_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *mvect_max_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	F8 max ALIGN = 0;
	S8 count ALIGN;
	F8 *src_ptr;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_max_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_max_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_max_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_max_double ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_max_double ERROR: src overflow!\n");
		return (NULL);
	}
	#endif

	src_ptr = (F8 *) &data[array_data_src_ptr + (start * offset)];
	max = *src_ptr;
	count = end - start + 1;

	if (count <= 1)
	{
		sp = stpushd (max, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("mvect_max_double: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	if (ocl_double_ensure_init () != 0)
	{
		printf ("mvect_max_double: ERROR: OpenCL init failed!\n");
		return (NULL);
	}

	cl_int err;
	cl_mem buf_src = clCreateBuffer (ocl_context, CL_MEM_READ_ONLY, count * sizeof (F8), NULL, &err);
	if (err != CL_SUCCESS)
	{
		printf ("mvect_max_double: ERROR clCreateBuffer src: %d\n", err);
		return (NULL);
	}

	cl_mem buf_result = clCreateBuffer (ocl_context, CL_MEM_READ_WRITE, sizeof (F8), NULL, &err);
	if (err != CL_SUCCESS)
	{
		printf ("mvect_max_double: ERROR clCreateBuffer result: %d\n", err);
		clReleaseMemObject (buf_src);
		return (NULL);
	}

	err = clEnqueueWriteBuffer (ocl_queue, buf_src, CL_TRUE, 0, count * sizeof (F8), src_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_max_double: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_result, CL_TRUE, 0, sizeof (F8), &max, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_max_double: ERROR clEnqueueWriteBuffer result: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

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
	if (err != CL_SUCCESS) { printf ("mvect_max_double: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	clFinish (ocl_queue);

	err = clEnqueueReadBuffer (ocl_queue, buf_result, CL_TRUE, 0, sizeof (F8), &max, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_max_double: ERROR clEnqueueReadBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	clReleaseMemObject (buf_src);
	clReleaseMemObject (buf_result);

	sp = stpushd (max, sp, sp_bottom);
	if (sp == NULL)
	{
		printf ("mvect_max_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

// ============================================================================
// average function (OpenCL parallel sum reduction)
// ============================================================================

U1 *mvect_average_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	S8 count ALIGN;
	F8 sum ALIGN = 0.0;
	F8 average ALIGN = 0.0;
	F8 *src_ptr;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_average_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_average_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_average_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_average_double ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_average_double ERROR: src overflow!\n");
		return (NULL);
	}
	#endif

	src_ptr = (F8 *) &data[array_data_src_ptr + (start * offset)];
	count = end - start + 1;

	if (count <= 0)
	{
		printf ("mvect_average_double: ERROR: empty range!\n");
		return (NULL);
	}

	if (count <= 1)
	{
		average = *src_ptr;
		sp = stpushd (average, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("mvect_average_double: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	if (ocl_double_ensure_init () != 0)
	{
		printf ("mvect_average_double: ERROR: OpenCL init failed!\n");
		return (NULL);
	}

	cl_int err;
	cl_mem buf_src = clCreateBuffer (ocl_context, CL_MEM_READ_ONLY, count * sizeof (F8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_average_double: ERROR clCreateBuffer src: %d\n", err); return (NULL); }

	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;

	size_t num_groups = (count + local_size - 1) / local_size;
	cl_mem buf_partial = clCreateBuffer (ocl_context, CL_MEM_READ_WRITE, num_groups * sizeof (F8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_average_double: ERROR clCreateBuffer partial: %d\n", err); clReleaseMemObject (buf_src); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_src, CL_TRUE, 0, count * sizeof (F8), src_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_average_double: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_partial); return (NULL); }

	size_t global_size = num_groups * local_size;

	S8 k_start = 0;
	S8 k_end = count - 1;
	clSetKernelArg (kern_sum, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_sum, 1, sizeof (cl_mem), &buf_partial);
	clSetKernelArg (kern_sum, 2, sizeof (S8), &k_start);
	clSetKernelArg (kern_sum, 3, sizeof (S8), &k_end);

	err = clEnqueueNDRangeKernel (ocl_queue, kern_sum, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_average_double: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_partial); return (NULL); }

	clFinish (ocl_queue);

	F8 *partial_sums = (F8 *) malloc (num_groups * sizeof (F8));
	if (partial_sums == NULL)
	{
		printf ("mvect_average_double: ERROR: malloc failed!\n");
		clReleaseMemObject (buf_src);
		clReleaseMemObject (buf_partial);
		return (NULL);
	}

	err = clEnqueueReadBuffer (ocl_queue, buf_partial, CL_TRUE, 0, num_groups * sizeof (F8), partial_sums, 0, NULL, NULL);
	if (err != CL_SUCCESS)
	{
		printf ("mvect_average_double: ERROR clEnqueueReadBuffer: %d\n", err);
		free (partial_sums);
		clReleaseMemObject (buf_src);
		clReleaseMemObject (buf_partial);
		return (NULL);
	}

	sum = 0.0;
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
		printf ("mvect_average_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

// ============================================================================
// array search (OpenCL parallel search)
// ============================================================================

U1 *mvect_array_search_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	S8 real_ind_start ALIGN;
	S8 real_ind_end ALIGN;
	S8 ret ALIGN = -1;
	F8 search ALIGN = 0;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_array_search_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_array_search_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopd ((U1 *) &search, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_array_search_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		printf ("mvect_array_search_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	real_ind_start = start * offset;
	real_ind_end = end * offset;

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, real_ind_start) != 0)
	{
		printf ("mvect_avray_search_double ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, real_ind_end) != 0)
	{
		printf ("mvect_array_search_double ERROR: src overflow!\n");
		return (NULL);
	}
	#endif

	S8 count = end - start + 1;
	F8 *src_ptr = (F8 *) &data[array_data_src_ptr + (start * offset)];

	if (count <= 0)
	{
		sp = stpushd (ret, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("mvect_array_search_double: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	if (ocl_double_ensure_init () != 0)
	{
		printf ("mvect_array_search_double: ERROR: OpenCL init failed!\n");
		return (NULL);
	}

	cl_int err;
	cl_mem buf_src = clCreateBuffer (ocl_context, CL_MEM_READ_ONLY, count * sizeof (F8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_array_search_double: ERROR clCreateBuffer src: %d\n", err); return (NULL); }

	S8 not_found = -1;
	cl_mem buf_result = clCreateBuffer (ocl_context, CL_MEM_READ_WRITE, sizeof (S8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_array_search_double: ERROR clCreateBuffer result: %d\n", err); clReleaseMemObject (buf_src); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_src, CL_TRUE, 0, count * sizeof (F8), src_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_search_double: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_result, CL_TRUE, 0, sizeof (S8), &not_found, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_search_double: ERROR clEnqueueWriteBuffer result: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	size_t global_size = count;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	S8 k_start = 0;
	S8 k_end = count - 1;
	clSetKernelArg (kern_search, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_search, 1, sizeof (cl_mem), &buf_result);
	clSetKernelArg (kern_search, 2, sizeof (F8), &search);
	clSetKernelArg (kern_search, 3, sizeof (S8), &k_start);
	clSetKernelArg (kern_search, 4, sizeof (S8), &k_end);

	err = clEnqueueNDRangeKernel (ocl_queue, kern_search, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_search_double: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	clFinish (ocl_queue);

	err = clEnqueueReadBuffer (ocl_queue, buf_result, CL_TRUE, 0, sizeof (S8), &ret, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_array_search_double: ERROR clEnqueueReadBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_result); return (NULL); }

	clReleaseMemObject (buf_src);
	clReleaseMemObject (buf_result);

	// convert buffer-relative index to absolute element index
	if (ret != -1) ret = ret + start;

	sp = stpushd (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		printf ("mvect_array_search_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

// ============================================================================
// scalar operations (OpenCL element-wise)
// ============================================================================

U1 *mvect_add_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	F8 number ALIGN = 0.0;

	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_double: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopd ((U1 *) &number, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_double: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_double: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_double: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_double: ERROR: stack corrupt!\n"); return (NULL); }

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0) { printf ("mvect_add_double ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_src_ptr, end * offset) != 0) { printf ("mvect_add_double ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_dst_ptr, start * offset) != 0) { printf ("mvect_add_double ERROR: dst overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0) { printf ("mvect_add_double ERROR: dst overflow!\n"); return (NULL); }
	#endif

	if (ocl_double_ensure_init () != 0) { printf ("mvect_add_double: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = end - start + 1;
	F8 *src_ptr = (F8 *) &data[array_data_src_ptr + (start * offset)];
	F8 *dst_ptr = (F8 *) &data[array_data_dst_ptr + (start * offset)];

	cl_int err;
	cl_mem buf_src = clCreateBuffer (ocl_context, CL_MEM_READ_ONLY, count * sizeof (F8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_add_double: ERROR clCreateBuffer src: %d\n", err); return (NULL); }
	cl_mem buf_dst = clCreateBuffer (ocl_context, CL_MEM_WRITE_ONLY, count * sizeof (F8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_add_double: ERROR clCreateBuffer dst: %d\n", err); clReleaseMemObject (buf_src); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_src, CL_TRUE, 0, count * sizeof (F8), src_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_add_double: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	size_t global_size = count;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	S8 k_start = 0;
	S8 k_end = count - 1;
	clSetKernelArg (kern_add_scalar, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_add_scalar, 1, sizeof (cl_mem), &buf_dst);
	clSetKernelArg (kern_add_scalar, 2, sizeof (F8), &number);
	clSetKernelArg (kern_add_scalar, 3, sizeof (S8), &k_start);
	clSetKernelArg (kern_add_scalar, 4, sizeof (S8), &k_end);

	err = clEnqueueNDRangeKernel (ocl_queue, kern_add_scalar, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_add_double: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	clFinish (ocl_queue);

	err = clEnqueueReadBuffer (ocl_queue, buf_dst, CL_TRUE, 0, count * sizeof (F8), dst_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_add_double: ERROR clEnqueueReadBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	clReleaseMemObject (buf_src);
	clReleaseMemObject (buf_dst);

	return (sp);
}

U1 *mvect_sub_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	F8 number ALIGN = 0.0;

	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_double: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopd ((U1 *) &number, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_double: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_double: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_double: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_double: ERROR: stack corrupt!\n"); return (NULL); }

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0) { printf ("mvect_sub_double ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_src_ptr, end * offset) != 0) { printf ("mvect_sub_double ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_dst_ptr, start * offset) != 0) { printf ("mvect_sub_double ERROR: dst overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0) { printf ("mvect_sub_double ERROR: dst overflow!\n"); return (NULL); }
	#endif

	if (ocl_double_ensure_init () != 0) { printf ("mvect_sub_double: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = end - start + 1;
	F8 *src_ptr = (F8 *) &data[array_data_src_ptr + (start * offset)];
	F8 *dst_ptr = (F8 *) &data[array_data_dst_ptr + (start * offset)];

	cl_int err;
	cl_mem buf_src = clCreateBuffer (ocl_context, CL_MEM_READ_ONLY, count * sizeof (F8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_sub_double: ERROR clCreateBuffer src: %d\n", err); return (NULL); }
	cl_mem buf_dst = clCreateBuffer (ocl_context, CL_MEM_WRITE_ONLY, count * sizeof (F8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_sub_double: ERROR clCreateBuffer dst: %d\n", err); clReleaseMemObject (buf_src); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_src, CL_TRUE, 0, count * sizeof (F8), src_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_sub_double: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	size_t global_size = count;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	S8 k_start = 0;
	S8 k_end = count - 1;
	clSetKernelArg (kern_sub_scalar, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_sub_scalar, 1, sizeof (cl_mem), &buf_dst);
	clSetKernelArg (kern_sub_scalar, 2, sizeof (F8), &number);
	clSetKernelArg (kern_sub_scalar, 3, sizeof (S8), &k_start);
	clSetKernelArg (kern_sub_scalar, 4, sizeof (S8), &k_end);

	err = clEnqueueNDRangeKernel (ocl_queue, kern_sub_scalar, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_sub_double: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	clFinish (ocl_queue);

	err = clEnqueueReadBuffer (ocl_queue, buf_dst, CL_TRUE, 0, count * sizeof (F8), dst_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_sub_double: ERROR clEnqueueReadBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	clReleaseMemObject (buf_src);
	clReleaseMemObject (buf_dst);

	return (sp);
}

U1 *mvect_mul_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	F8 number ALIGN = 0.0;

	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_double: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopd ((U1 *) &number, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_double: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_double: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_double: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_double: ERROR: stack corrupt!\n"); return (NULL); }

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0) { printf ("mvect_mul_double ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_src_ptr, end * offset) != 0) { printf ("mvect_mul_double ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_dst_ptr, start * offset) != 0) { printf ("mvect_mul_double ERROR: dst overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0) { printf ("mvect_mul_double ERROR: dst overflow!\n"); return (NULL); }
	#endif

	if (ocl_double_ensure_init () != 0) { printf ("mvect_mul_double: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = end - start + 1;
	F8 *src_ptr = (F8 *) &data[array_data_src_ptr + (start * offset)];
	F8 *dst_ptr = (F8 *) &data[array_data_dst_ptr + (start * offset)];

	cl_int err;
	cl_mem buf_src = clCreateBuffer (ocl_context, CL_MEM_READ_ONLY, count * sizeof (F8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_mul_double: ERROR clCreateBuffer src: %d\n", err); return (NULL); }
	cl_mem buf_dst = clCreateBuffer (ocl_context, CL_MEM_WRITE_ONLY, count * sizeof (F8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_mul_double: ERROR clCreateBuffer dst: %d\n", err); clReleaseMemObject (buf_src); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_src, CL_TRUE, 0, count * sizeof (F8), src_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_mul_double: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	size_t global_size = count;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	S8 k_start = 0;
	S8 k_end = count - 1;
	clSetKernelArg (kern_mul_scalar, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_mul_scalar, 1, sizeof (cl_mem), &buf_dst);
	clSetKernelArg (kern_mul_scalar, 2, sizeof (F8), &number);
	clSetKernelArg (kern_mul_scalar, 3, sizeof (S8), &k_start);
	clSetKernelArg (kern_mul_scalar, 4, sizeof (S8), &k_end);

	err = clEnqueueNDRangeKernel (ocl_queue, kern_mul_scalar, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_mul_double: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	clFinish (ocl_queue);

	err = clEnqueueReadBuffer (ocl_queue, buf_dst, CL_TRUE, 0, count * sizeof (F8), dst_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_mul_double: ERROR clEnqueueReadBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	clReleaseMemObject (buf_src);
	clReleaseMemObject (buf_dst);

	return (sp);
}

U1 *mvect_div_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	F8 number ALIGN = 0.0;

	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_double: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopd ((U1 *) &number, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_double: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_double: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_double: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_double: ERROR: stack corrupt!\n"); return (NULL); }

	if (number == 0.0)
	{
		printf ("mvect_div_double: ERROR division by zero!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0) { printf ("mvect_div_double ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_src_ptr, end * offset) != 0) { printf ("mvect_div_double ERROR: src overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_dst_ptr, start * offset) != 0) { printf ("mvect_div_double ERROR: dst overflow!\n"); return (NULL); }
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0) { printf ("mvect_div_double ERROR: dst overflow!\n"); return (NULL); }
	#endif

	if (ocl_double_ensure_init () != 0) { printf ("mvect_div_double: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = end - start + 1;
	F8 *src_ptr = (F8 *) &data[array_data_src_ptr + (start * offset)];
	F8 *dst_ptr = (F8 *) &data[array_data_dst_ptr + (start * offset)];

	cl_int err;
	cl_mem buf_src = clCreateBuffer (ocl_context, CL_MEM_READ_ONLY, count * sizeof (F8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_div_double: ERROR clCreateBuffer src: %d\n", err); return (NULL); }
	cl_mem buf_dst = clCreateBuffer (ocl_context, CL_MEM_WRITE_ONLY, count * sizeof (F8), NULL, &err);
	if (err != CL_SUCCESS) { printf ("mvect_div_double: ERROR clCreateBuffer dst: %d\n", err); clReleaseMemObject (buf_src); return (NULL); }

	err = clEnqueueWriteBuffer (ocl_queue, buf_src, CL_TRUE, 0, count * sizeof (F8), src_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_div_double: ERROR clEnqueueWriteBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	size_t global_size = count;
	size_t local_size = ocl_preferred_group_size;
	if (local_size > 256) local_size = 256;
	if (global_size % local_size != 0) global_size = ((global_size / local_size) + 1) * local_size;

	S8 k_start = 0;
	S8 k_end = count - 1;
	clSetKernelArg (kern_div_scalar, 0, sizeof (cl_mem), &buf_src);
	clSetKernelArg (kern_div_scalar, 1, sizeof (cl_mem), &buf_dst);
	clSetKernelArg (kern_div_scalar, 2, sizeof (F8), &number);
	clSetKernelArg (kern_div_scalar, 3, sizeof (S8), &k_start);
	clSetKernelArg (kern_div_scalar, 4, sizeof (S8), &k_end);

	err = clEnqueueNDRangeKernel (ocl_queue, kern_div_scalar, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_div_double: ERROR clEnqueueNDRangeKernel: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	clFinish (ocl_queue);

	err = clEnqueueReadBuffer (ocl_queue, buf_dst, CL_TRUE, 0, count * sizeof (F8), dst_ptr, 0, NULL, NULL);
	if (err != CL_SUCCESS) { printf ("mvect_div_double: ERROR clEnqueueReadBuffer: %d\n", err); clReleaseMemObject (buf_src); clReleaseMemObject (buf_dst); return (NULL); }

	clReleaseMemObject (buf_src);
	clReleaseMemObject (buf_dst);

	return (sp);
}

// ============================================================================
// array operations (OpenCL element-wise, three slots)
// ============================================================================

U1 *mvect_add_double_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 slot_src ALIGN;
	S8 slot_src2 ALIGN;
	S8 slot_dst ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;

	sp = stpopi ((U1 *) &slot_dst, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &slot_src2, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &slot_src, sp, sp_top);
	if (sp == NULL) { printf ("mvect_add_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	if (ocl_double_ensure_init () != 0) { printf ("mvect_add_double_array: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = end - start + 1;
	if (count <= 0) { printf ("mvect_add_double_array: ERROR: count <= 0!\n"); return (NULL); }
	size_t nbytes = count * sizeof (F8);

	cl_mem buf_src = ocl_get_slot ((S4) slot_src, nbytes);
	if (buf_src == NULL) { printf ("mvect_add_double_array: ERROR: no slot buffer for source slot!\n"); return (NULL); }
	cl_mem buf_src2 = ocl_get_slot ((S4) slot_src2, nbytes);
	if (buf_src2 == NULL) { printf ("mvect_add_double_array: ERROR: no slot buffer for source2 slot!\n"); return (NULL); }
	cl_mem buf_dst = ocl_get_slot ((S4) slot_dst, nbytes);
	if (buf_dst == NULL) { printf ("mvect_add_double_array: ERROR: no slot buffer for destination slot!\n"); return (NULL); }

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
	if (err != CL_SUCCESS) { printf ("mvect_add_double_array: ERROR clEnqueueNDRangeKernel: %d\n", err); return (NULL); }

	return (sp);
}

U1 *mvect_sub_double_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 slot_src ALIGN;
	S8 slot_src2 ALIGN;
	S8 slot_dst ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;

	sp = stpopi ((U1 *) &slot_dst, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &slot_src2, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &slot_src, sp, sp_top);
	if (sp == NULL) { printf ("mvect_sub_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	if (ocl_double_ensure_init () != 0) { printf ("mvect_sub_double_array: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = end - start + 1;
	if (count <= 0) { printf ("mvect_sub_double_array: ERROR: count <= 0!\n"); return (NULL); }
	size_t nbytes = count * sizeof (F8);

	cl_mem buf_src = ocl_get_slot ((S4) slot_src, nbytes);
	if (buf_src == NULL) { printf ("mvect_sub_double_array: ERROR: no slot buffer for source slot!\n"); return (NULL); }
	cl_mem buf_src2 = ocl_get_slot ((S4) slot_src2, nbytes);
	if (buf_src2 == NULL) { printf ("mvect_sub_double_array: ERROR: no slot buffer for source2 slot!\n"); return (NULL); }
	cl_mem buf_dst = ocl_get_slot ((S4) slot_dst, nbytes);
	if (buf_dst == NULL) { printf ("mvect_sub_double_array: ERROR: no slot buffer for destination slot!\n"); return (NULL); }

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
	if (err != CL_SUCCESS) { printf ("mvect_sub_double_array: ERROR clEnqueueNDRangeKernel: %d\n", err); return (NULL); }

	return (sp);
}

U1 *mvect_mul_double_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 slot_src ALIGN;
	S8 slot_src2 ALIGN;
	S8 slot_dst ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;

	sp = stpopi ((U1 *) &slot_dst, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &slot_src2, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &slot_src, sp, sp_top);
	if (sp == NULL) { printf ("mvect_mul_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	if (ocl_double_ensure_init () != 0) { printf ("mvect_mul_double_array: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = end - start + 1;
	if (count <= 0) { printf ("mvect_mul_double_array: ERROR: count <= 0!\n"); return (NULL); }
	size_t nbytes = count * sizeof (F8);

	cl_mem buf_src = ocl_get_slot ((S4) slot_src, nbytes);
	if (buf_src == NULL) { printf ("mvect_mul_double_array: ERROR: no slot buffer for source slot!\n"); return (NULL); }
	cl_mem buf_src2 = ocl_get_slot ((S4) slot_src2, nbytes);
	if (buf_src2 == NULL) { printf ("mvect_mul_double_array: ERROR: no slot buffer for source2 slot!\n"); return (NULL); }
	cl_mem buf_dst = ocl_get_slot ((S4) slot_dst, nbytes);
	if (buf_dst == NULL) { printf ("mvect_mul_double_array: ERROR: no slot buffer for destination slot!\n"); return (NULL); }

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
	if (err != CL_SUCCESS) { printf ("mvect_mul_double_array: ERROR clEnqueueNDRangeKernel: %d\n", err); return (NULL); }

	return (sp);
}

U1 *mvect_div_double_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 slot_src ALIGN;
	S8 slot_src2 ALIGN;
	S8 slot_dst ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;

	sp = stpopi ((U1 *) &slot_dst, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &slot_src2, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	sp = stpopi ((U1 *) &slot_src, sp, sp_top);
	if (sp == NULL) { printf ("mvect_div_double_array: ERROR: stack corrupt!\n"); return (NULL); }

	if (ocl_double_ensure_init () != 0) { printf ("mvect_div_double_array: ERROR: OpenCL init failed!\n"); return (NULL); }

	S8 count = end - start + 1;
	if (count <= 0) { printf ("mvect_div_double_array: ERROR: count <= 0!\n"); return (NULL); }
	size_t nbytes = count * sizeof (F8);

	cl_mem buf_src = ocl_get_slot ((S4) slot_src, nbytes);
	if (buf_src == NULL) { printf ("mvect_div_double_array: ERROR: no slot buffer for source slot!\n"); return (NULL); }
	cl_mem buf_src2 = ocl_get_slot ((S4) slot_src2, nbytes);
	if (buf_src2 == NULL) { printf ("mvect_div_double_array: ERROR: no slot buffer for source2 slot!\n"); return (NULL); }
	cl_mem buf_dst = ocl_get_slot ((S4) slot_dst, nbytes);
	if (buf_dst == NULL) { printf ("mvect_div_double_array: ERROR: no slot buffer for destination slot!\n"); return (NULL); }

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
	if (err != CL_SUCCESS) { printf ("mvect_div_double_array: ERROR clEnqueueNDRangeKernel: %d\n", err); return (NULL); }

	return (sp);
}
