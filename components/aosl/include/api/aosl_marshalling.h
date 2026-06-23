/***************************************************************************
 * Module		:		Data Marshalling
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_MARSHALLING_H__
#define __AOSL_MARSHALLING_H__

#include <api/aosl_types.h>
#include <api/aosl_defs.h>
#include <api/aosl_psb.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
	AOSL_TYPE_VOID,
	AOSL_TYPE_INT8,
	AOSL_TYPE_INT16,
	AOSL_TYPE_INT32,
	AOSL_TYPE_INT64,
	AOSL_TYPE_FLOAT,
	AOSL_TYPE_DOUBLE,
	AOSL_TYPE_V4_IPADDR,
	AOSL_TYPE_V6_IPADDR,
	AOSL_TYPE_POINTER,
	AOSL_TYPE_REFERENCE,
	AOSL_TYPE_STRING,
	AOSL_TYPE_STRUCT,
	AOSL_TYPE_FIXED_BYTES,
	AOSL_TYPE_VAR_BYTES,
	AOSL_TYPE_BYTES_WITH_NIL,
	AOSL_TYPE_DYNAMIC_BYTES,
	AOSL_TYPE_DYNAMIC_STRING,
	AOSL_TYPE_FIXED_ARRAY,
	AOSL_TYPE_VAR_ARRAY,
	AOSL_TYPE_DYNAMIC_ARRAY,
} aosl_type_id;

/** @brief Get the relative address of a struct field (offset from base). */
#define aosl_rela_addr(type, field) (&((type *)0)->field)
/** @brief Get the base address of a struct from a pointer to one of its fields. */
#define aosl_base_addr(ptr, type, field) \
	((type *)((uintptr_t)(ptr) - (uintptr_t)(&((type *)0)->field)))

typedef struct {
	/**
	 * For almost all cases, the count of an array will not
	 * exceed a 16 bit unsigned integer, so we just employ
	 * a 16 bit unsigned in the marshalling packet is OK.
	 * For those cases that the size of array exceed a 16
	 * bit integer, I think the system designer should split
	 * the packet into severals to resolve the problem.
	 **/
	uint16_t count;
	uint16_t allocated;
	void *values;
} aosl_dynamic_array_t;

/**
 * @brief Initialize a dynamic array to empty state.
 * @param [out] arr  pointer to the dynamic array
 **/
extern __aosl_api__ void aosl_dynamic_array_init (aosl_dynamic_array_t *arr);

/**
 * @brief Initialize a dynamic array with existing data (no copy, takes ownership).
 * @param [out] arr   pointer to the dynamic array
 * @param [in]  data  the data buffer
 * @param [in]  len   the data length in bytes
 **/
extern __aosl_api__ void aosl_dynamic_array_init_with (aosl_dynamic_array_t *arr, void *data, size_t len);

/**
 * @brief Append elements to a dynamic array.
 * @param [in,out] arr        pointer to the dynamic array
 * @param [in]     elems      pointer to the elements to add
 * @param [in]     elem_size  the size of each element in bytes
 * @param [in]     nelems     the number of elements to add
 * @return           0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_dynamic_array_add_elems (aosl_dynamic_array_t *arr, const void *elems, size_t elem_size, size_t nelems);

/**
 * @brief Transfer ownership of data from src to dst dynamic array.
 * After this call, src is empty and dst holds the data.
 * @param [out]    dst  the destination dynamic array
 * @param [in,out] src  the source dynamic array
 **/
extern __aosl_api__ void aosl_dynamic_array_take (aosl_dynamic_array_t *dst, aosl_dynamic_array_t *src);

/**
 * @brief Check if a dynamic array is empty.
 * @param [in] arr  pointer to the dynamic array
 * @return     non-zero if empty, 0 if not
 **/
extern __aosl_api__ int aosl_dynamic_array_is_empty (const aosl_dynamic_array_t *arr);

typedef aosl_dynamic_array_t aosl_dynamic_bytes_t;
typedef aosl_dynamic_bytes_t aosl_dynamic_string_t;

/** @brief Initialize a dynamic bytes buffer to empty state. */
#define aosl_dynamic_bytes_init(v) aosl_dynamic_array_init (v)
/** @brief Initialize a dynamic bytes buffer with existing data (takes ownership). */
#define aosl_dynamic_bytes_init_with(v, buf, len) aosl_dynamic_array_init_with (v, buf, len)
/** @brief Initialize a dynamic string to empty state. */
#define aosl_dynamic_string_init(s) aosl_dynamic_array_init (s)
/** @brief Initialize a dynamic string with existing data (takes ownership). */
#define aosl_dynamic_string_init_with(v, buf, len) aosl_dynamic_array_init_with (v, buf, len)

/** @brief Get a typed pointer to the underlying data of a dynamic array. */
#define aosl_dynamic_array_data(arr, T) ((T *)(arr).values)
/** @brief Get a uint8_t pointer to the underlying data of a dynamic bytes buffer. */
#define aosl_dynamic_bytes_data(arr) aosl_dynamic_array_data (arr, uint8_t)
/** @brief Get a char pointer to the underlying C string of a dynamic string. */
#define aosl_dynamic_string_c_str(dyn_str) aosl_dynamic_array_data (dyn_str, char)

/**
 * @brief Append raw data to a dynamic bytes buffer.
 * @param [in,out] dyn_bytes  pointer to the dynamic bytes
 * @param [in]     data       the data to append
 * @param [in]     len        the data length in bytes
 * @return           0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_dynamic_bytes_add_data (aosl_dynamic_bytes_t *dyn_bytes, const void *data, size_t len);

/**
 * @brief Replace the content of a dynamic bytes buffer with new data.
 * @param [in,out] dyn_bytes  pointer to the dynamic bytes
 * @param [in]     data       the data to copy
 * @param [in]     len        the data length in bytes
 * @return           0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_dynamic_bytes_copy_data (aosl_dynamic_bytes_t *dyn_bytes, const void *data, size_t len);

/**
 * @brief Concatenate a string to a dynamic string.
 * @param [in,out] dyn_str  pointer to the dynamic string
 * @param [in]     str      the null-terminated string to append
 * @return         0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_dynamic_string_strcat (aosl_dynamic_string_t *dyn_str, const char *str);

/**
 * @brief Replace the content of a dynamic string with a new string.
 * @param [in,out] dyn_str  pointer to the dynamic string
 * @param [in]     str      the null-terminated string to copy
 * @return         0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_dynamic_string_strcpy (aosl_dynamic_string_t *dyn_str, const char *str);

/**
 * @brief Copy a dynamic string into a fixed-size buffer.
 * @param [out] buf      the output buffer
 * @param [in]  buf_sz   the buffer size in bytes
 * @param [in]  dyn_str  pointer to the dynamic string
 * @return         pointer to buf on success
 **/
extern __aosl_api__ const char *aosl_dynamic_string_strcpy_out (char *buf, size_t buf_sz, const aosl_dynamic_string_t *dyn_str);

/** @brief Transfer ownership of a dynamic bytes buffer from src to dst. */
#define aosl_dynamic_bytes_take(dst, src) aosl_dynamic_array_take (dst, src)
/** @brief Transfer ownership of a dynamic string from src to dst. */
#define aosl_dynamic_string_take(dst, src) aosl_dynamic_array_take (dst, src)

/** @brief Check if a dynamic bytes buffer is empty. */
#define aosl_bytes_is_empty(v) aosl_dynamic_array_is_empty (v)
/** @brief Check if a dynamic string is empty. */
#define aosl_dynamic_string_is_empty(v) aosl_dynamic_array_is_empty (v)

/**
 * @brief Deep copy a dynamic bytes buffer.
 * @param [out] dst  the destination dynamic bytes
 * @param [in]  src  the source dynamic bytes
 * @return     0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_dynamic_bytes_copy (aosl_dynamic_bytes_t *dst, const aosl_dynamic_bytes_t *src);
/** @brief Deep copy a dynamic string. */
#define aosl_dynamic_string_copy(dst, src) aosl_dynamic_bytes_copy (dst, src)

/**
 * @brief Compare two dynamic bytes buffers for equality.
 * @param [in] b1  the first dynamic bytes
 * @param [in] b2  the second dynamic bytes
 * @return    0 if equal, <0 if b1 < b2, >0 if b1 > b2
 **/
extern __aosl_api__ int aosl_dynamic_bytes_compare (const aosl_dynamic_bytes_t *b1, const aosl_dynamic_bytes_t *b2);
/** @brief Compare two dynamic strings for equality. */
#define aosl_dynamic_string_compare(s1, s2) aosl_dynamic_bytes_compare (s1, s2)


typedef struct _____type_info {
	aosl_type_id type_id;
	uint32_t obj_size;
	uint32_t array_size;
	uint16_t *count_var_addr;
	void *obj_addr;
	int8_t (*is_have)(const void *obj_addr);
	const struct _____type_info *child;
} aosl_type_info_t;

#define AOSL_TYPE_STRUCT_END { .type_id = AOSL_TYPE_VOID }



/**
 * @brief Initialize a typed object's fields to default values based on type info.
 * @param [in]  type         the type descriptor
 * @param [out] typed_obj_p  pointer to the object to initialize
 **/
extern __aosl_api__ void aosl_init_typed_obj (const aosl_type_info_t *type, void *typed_obj_p);

/**
 * @brief Marshal (serialize) a typed object into a PSB.
 * @param [in]  type         the type descriptor
 * @param [in]  typed_obj_p  pointer to the object to marshal
 * @param [out] psb          the PSB to write into
 * @return             the number of bytes written, or <0 on failure
 **/
extern __aosl_api__ isize_t aosl_marshal (const aosl_type_info_t *type, const void *typed_obj_p, aosl_psb_t *psb);

/**
 * @brief Unmarshal (deserialize) a typed object from a PSB.
 * @param [in]  type         the type descriptor
 * @param [out] typed_obj_p  pointer to the object to fill
 * @param [in]  psb          the PSB to read from
 * @return             the number of bytes consumed, or <0 on failure
 **/
extern __aosl_api__ isize_t aosl_unmarshal (const aosl_type_info_t *type, void *typed_obj_p, const aosl_psb_t *psb);

/**
 * @brief Finalize a typed object, freeing any dynamically allocated fields.
 * @param [in] type         the type descriptor
 * @param [in] typed_obj_p  pointer to the object to finalize
 **/
extern __aosl_api__ void aosl_fini_typed_obj (const aosl_type_info_t *type, const void *typed_obj_p);


/**
 * @brief Encode a 16-bit integer to network byte order for marshalling.
 * @param [in] v  the value to encode
 * @return   the encoded value
 **/
extern __aosl_api__ uint16_t aosl_encode_int16 (uint16_t v);

/**
 * @brief Encode a 32-bit integer to network byte order for marshalling.
 * @param [in] v  the value to encode
 * @return   the encoded value
 **/
extern __aosl_api__ uint32_t aosl_encode_int32 (uint32_t v);

/**
 * @brief Encode a 64-bit integer to network byte order for marshalling.
 * @param [in] v  the value to encode
 * @return   the encoded value
 **/
extern __aosl_api__ uint64_t aosl_encode_int64 (uint64_t v);

/**
 * @brief Decode a 16-bit integer from network byte order.
 * @param [in] v  the value to decode
 * @return   the decoded value
 **/
extern __aosl_api__ uint16_t aosl_decode_int16 (uint16_t v);

/**
 * @brief Decode a 32-bit integer from network byte order.
 * @param [in] v  the value to decode
 * @return   the decoded value
 **/
extern __aosl_api__ uint32_t aosl_decode_int32 (uint32_t v);

/**
 * @brief Decode a 64-bit integer from network byte order.
 * @param [in] v  the value to decode
 * @return   the decoded value
 **/
extern __aosl_api__ uint64_t aosl_decode_int64 (uint64_t v);


extern const aosl_type_info_t aosl_void_type;
extern const aosl_type_info_t aosl_int16_type;
extern const aosl_type_info_t aosl_int32_type;
extern const aosl_type_info_t aosl_int64_type;
extern const aosl_type_info_t aosl_float_type;
extern const aosl_type_info_t aosl_double_type;
extern const aosl_type_info_t aosl_v4_ipaddr_type;
extern const aosl_type_info_t aosl_v6_ipaddr_type;
extern const aosl_type_info_t aosl_dynamic_bytes_type;
extern const aosl_type_info_t aosl_dynamic_string_type;
extern const aosl_type_info_t aosl_dynamic_int16_array_type;
extern const aosl_type_info_t aosl_dynamic_int32_array_type;
extern const aosl_type_info_t aosl_dynamic_int64_array_type;
extern const aosl_type_info_t aosl_dynamic_float_array_type;
extern const aosl_type_info_t aosl_dynamic_double_array_type;
extern const aosl_type_info_t aosl_dynamic_v4_ipaddr_array_type;
extern const aosl_type_info_t aosl_dynamic_v6_ipaddr_array_type;

#define aosl_dynamic_bytes_fini(bytes) aosl_fini_typed_obj (&aosl_dynamic_bytes_type, bytes)
#define aosl_dynamic_string_fini(dyn_str) aosl_fini_typed_obj (&aosl_dynamic_string_type, dyn_str)



#ifdef __cplusplus
}
#endif


#endif /* __AOSL_MARSHALLING_H__ */