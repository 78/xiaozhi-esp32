/***************************************************************************
 * Module		:		Data Marshalling base object
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#include <api/aosl_marshalling.h>

#ifndef _WIN32
#define __export_data_in_so__ __export_in_so__
#else
#define __export_data_in_so__
#endif

__export_data_in_so__ const aosl_type_info_t aosl_void_type = { .type_id = AOSL_TYPE_VOID, .obj_addr = NULL, };
__export_data_in_so__ const aosl_type_info_t aosl_int16_type = { .type_id = AOSL_TYPE_INT16, .obj_addr = NULL, };
__export_data_in_so__ const aosl_type_info_t aosl_int32_type = { .type_id = AOSL_TYPE_INT32, .obj_addr = NULL, };
__export_data_in_so__ const aosl_type_info_t aosl_int64_type = { .type_id = AOSL_TYPE_INT64, .obj_addr = NULL, };
__export_data_in_so__ const aosl_type_info_t aosl_float_type = { .type_id = AOSL_TYPE_FLOAT, .obj_addr = NULL, };
__export_data_in_so__ const aosl_type_info_t aosl_double_type = { .type_id = AOSL_TYPE_DOUBLE, .obj_addr = NULL, };
__export_data_in_so__ const aosl_type_info_t aosl_v4_ipaddr_type = { .type_id = AOSL_TYPE_V4_IPADDR, .obj_addr = NULL, };
__export_data_in_so__ const aosl_type_info_t aosl_v6_ipaddr_type = { .type_id = AOSL_TYPE_V6_IPADDR, .obj_addr = NULL, };

__export_data_in_so__ const aosl_type_info_t aosl_dynamic_int16_array_type = {
	.type_id = AOSL_TYPE_DYNAMIC_ARRAY,
	.obj_addr = NULL,
	.child = &aosl_int16_type,
};

__export_data_in_so__ const aosl_type_info_t aosl_dynamic_int32_array_type = {
	.type_id = AOSL_TYPE_DYNAMIC_ARRAY,
	.obj_addr = NULL,
	.child = &aosl_int32_type,
};

__export_data_in_so__ const aosl_type_info_t aosl_dynamic_int64_array_type = {
	.type_id = AOSL_TYPE_DYNAMIC_ARRAY,
	.obj_addr = NULL,
	.child = &aosl_int64_type,
};

__export_data_in_so__ const aosl_type_info_t aosl_dynamic_float_array_type = {
	.type_id = AOSL_TYPE_DYNAMIC_ARRAY,
	.obj_addr = NULL,
	.child = &aosl_float_type,
};

__export_data_in_so__ const aosl_type_info_t aosl_dynamic_double_array_type = {
	.type_id = AOSL_TYPE_DYNAMIC_ARRAY,
	.obj_addr = NULL,
	.child = &aosl_double_type,
};

__export_data_in_so__ const aosl_type_info_t aosl_dynamic_v4_ipaddr_array_type = {
	.type_id = AOSL_TYPE_DYNAMIC_ARRAY,
	.obj_addr = NULL,
	.child = &aosl_v4_ipaddr_type,
};

__export_data_in_so__ const aosl_type_info_t aosl_dynamic_v6_ipaddr_array_type = {
	.type_id = AOSL_TYPE_DYNAMIC_ARRAY,
	.obj_addr = NULL,
	.child = &aosl_v6_ipaddr_type,
};

/**
 * The nasty Microsoft DLL mechanism could not do the dynamic relocation
 * in place for the shared object exported data objects, instead Microsoft
 * just uses the relative '__imp_*' objects in the import library, such as
 * mov     rax, QWORD PTR __imp__k
 * mov     QWORD PTR _k_p$[ebp], rax
 * mov     rax, QWORD PTR __imp__k
 * mov     eax, QWORD PTR [rax]
 * mov     DWORD PTR _k_v$[rsp], eax
 * So, we could not obtain the address of a dll imported data object when
 * initializing a static data variable, because the it is not a constant.
 * So, for Windows platform, we define these data objects as static.
 **/
__export_data_in_so__ const aosl_type_info_t aosl_dynamic_bytes_type = { .type_id = AOSL_TYPE_DYNAMIC_BYTES, .obj_addr = NULL, };
__export_data_in_so__ const aosl_type_info_t aosl_dynamic_string_type = { .type_id = AOSL_TYPE_DYNAMIC_STRING, .obj_addr = NULL, };
