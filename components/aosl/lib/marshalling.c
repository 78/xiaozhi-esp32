/***************************************************************************
 * Module		:		Data Marshalling
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#include <stdio.h>
#include <string.h>

#include <kernel/kernel.h>
#include <kernel/types.h>
#include <api/aosl_mm.h>
#include <api/aosl_socket.h>
#include <kernel/byteorder/generic.h>
#include <kernel/psbuff.h>
#include <api/aosl_marshalling.h>


#define DYNAMIC_ARRAY_MAX_COUNT 10240 /* I think 10K elements is enough */
#define DYNAMIC_ARRAY_GROW_STRIDE 4
#define DYNAMIC_BYTES_GROW_STRIDE 16

__export_in_so__ void aosl_dynamic_array_init (aosl_dynamic_array_t *arr)
{
	arr->count = 0;
	arr->allocated = 0;
	arr->values = NULL;
}

__export_in_so__ void aosl_dynamic_array_init_with (aosl_dynamic_array_t *arr, void *data, size_t len)
{
	arr->count = (uint16_t)len;
	arr->allocated = (uint16_t)len;
	arr->values = data;
}

__export_in_so__ int aosl_dynamic_array_add_elems (aosl_dynamic_array_t *arr, const void *elems, size_t elem_size, size_t nelems)
{
	uint16_t allocated;

	if (arr->count >= DYNAMIC_ARRAY_MAX_COUNT)
		return -AOSL_E2BIG;

	if (nelems >= DYNAMIC_ARRAY_MAX_COUNT)
		return -AOSL_E2BIG;

	if (nelems + arr->count >= DYNAMIC_ARRAY_MAX_COUNT)
		return -AOSL_E2BIG;

	allocated = arr->allocated;
	while (allocated < (uint16_t)(arr->count + nelems))
		allocated += DYNAMIC_ARRAY_GROW_STRIDE;

	if (allocated > arr->allocated) {
		void *new_values = aosl_malloc (elem_size * allocated);
		if (new_values == NULL)
			return -AOSL_ENOMEM;

		if (arr->count > 0)
			memcpy (new_values, arr->values, elem_size * arr->count);
		
		if (arr->allocated > 0)
			aosl_free (arr->values);

		arr->values = new_values;
		arr->allocated = allocated;
	}

	if (nelems > 0) {
		memcpy ((uint8_t *)arr->values + (elem_size * arr->count), elems, elem_size * nelems);
		arr->count += nelems;
	}

	return 0;
}

__export_in_so__ void aosl_dynamic_array_take (aosl_dynamic_array_t *dst, aosl_dynamic_array_t *src)
{
	if (dst->allocated > 0)
		aosl_free (dst->values);

	dst->count = src->count;
	dst->allocated = src->allocated;
	dst->values = src->values;

	src->count = 0;
	src->allocated = 0;
	src->values = NULL;
}

__export_in_so__ int aosl_dynamic_array_is_empty (const aosl_dynamic_array_t *arr)
{
	return (int)(arr->count == 0);
}

__export_in_so__ int aosl_dynamic_bytes_copy (aosl_dynamic_bytes_t *dst, const aosl_dynamic_bytes_t *src)
{
	void *values = NULL;

	if (src->allocated > 0) {
		values = aosl_malloc (src->allocated);
		if (values == NULL)
			return -1;
		
		memcpy (values, src->values, src->count);
		if (src->allocated > src->count)
			*((char *)values + src->count) = '\0';
	}

	if (dst->allocated > 0)
		aosl_free (dst->values);

	dst->count = src->count;
	dst->allocated = src->allocated;
	dst->values = values;
	return 0;
}

__export_in_so__ int aosl_dynamic_bytes_compare (const aosl_dynamic_bytes_t *b1, const aosl_dynamic_bytes_t *b2)
{
	if (b1->count != b2->count)
		return (int)(b1->count - b2->count);

	return memcmp (b1->values, b2->values, (size_t)b1->count);
}

__export_in_so__ int aosl_dynamic_bytes_add_data (aosl_dynamic_bytes_t *dyn_bytes, const void *data, size_t len)
{
	return aosl_dynamic_array_add_elems ((aosl_dynamic_array_t *)dyn_bytes, data, 1, len);
}

__export_in_so__ int aosl_dynamic_bytes_copy_data (aosl_dynamic_bytes_t *dyn_bytes, const void *data, size_t len)
{
	dyn_bytes->count = 0;
	return aosl_dynamic_array_add_elems ((aosl_dynamic_array_t *)dyn_bytes, data, 1, len);
}

__export_in_so__ int aosl_dynamic_string_strcat (aosl_dynamic_string_t *dyn_str, const char *str)
{
	uint16_t allocated;
	size_t len = strlen (str);

	if (dyn_str->count >= DYNAMIC_ARRAY_MAX_COUNT)
		return -AOSL_E2BIG;

	if (len >= DYNAMIC_ARRAY_MAX_COUNT)
		return -AOSL_E2BIG;

	if (len + dyn_str->count >= DYNAMIC_ARRAY_MAX_COUNT)
		return -AOSL_E2BIG;

	allocated = dyn_str->allocated;
	while (allocated < (uint16_t)(dyn_str->count + len + 1 /* the last '\0' */))
		allocated += DYNAMIC_BYTES_GROW_STRIDE;

	if (allocated > dyn_str->allocated) {
		void *new_values = aosl_malloc (allocated);
		if (new_values == NULL)
			return -AOSL_ENOMEM;

		if (dyn_str->count > 0)
			memcpy (new_values, dyn_str->values, dyn_str->count);
		
		if (dyn_str->allocated > 0)
			aosl_free (dyn_str->values);

		dyn_str->values = new_values;
		dyn_str->allocated = allocated;
	}

	if (len > 0) {
		memcpy ((uint8_t *)dyn_str->values + dyn_str->count, str, len);
		dyn_str->count += len;
	}

	*((char *)dyn_str->values + dyn_str->count) = '\0';
	return 0;
}

__export_in_so__ int aosl_dynamic_string_strcpy (aosl_dynamic_string_t *dyn_str, const char *str)
{
	dyn_str->count = 0; /* just set the current count to 0 and doing a strcat */
	return aosl_dynamic_string_strcat (dyn_str, str);
}

__export_in_so__ const char *aosl_dynamic_string_strcpy_out (char *buf, size_t buf_sz, const aosl_dynamic_string_t *dyn_str)
{
	size_t n = dyn_str->count;
	if (n >= buf_sz)
		n = buf_sz - 1;

	memcpy (buf, dyn_str->values, n);
	buf [n] = '\0';
	return buf;
}

#define __encode_int8 /* nothing */
#define __decode_int8

#ifdef __K_LITTLE_ENDIAN
#define __encode_int16 aosl_cpu_to_le16
#define __decode_int16 aosl_le16_to_cpu
#define __encode_int32 aosl_cpu_to_le32
#define __decode_int32 aosl_le32_to_cpu
#define __encode_int64 aosl_cpu_to_le64
#define __decode_int64 aosl_le64_to_cpu
#define __encode_ipv4 /*bswap_in_addr*/
#define __encode_ipv6 /*bswap_in6_addr*/
#define __decode_ipv4 /*bswap_in_addr*/
#define __decode_ipv6 /*bswap_in6_addr*/
#else /* for case of !__K_LITTLE_ENDIAN */
#define __encode_int16 aosl_cpu_to_be16
#define __decode_int16 aosl_be16_to_cpu
#define __encode_int32 aosl_cpu_to_be32
#define __decode_int32 aosl_be32_to_cpu
#define __encode_int64 aosl_cpu_to_be64
#define __decode_int64 aosl_be64_to_cpu
#define __encode_ipv4
#define __encode_ipv6
#define __decode_ipv4
#define __decode_ipv6
#endif


static int get_type_size (const aosl_type_info_t *type, size_t *obj_size_p)
{
	size_t obj_size;

	switch (type->type_id) {
	case AOSL_TYPE_VOID:
		obj_size = 0;
		break;
	case AOSL_TYPE_INT8:
		obj_size = 1;
		break;
	case AOSL_TYPE_INT16:
		obj_size = 2;
		break;
	case AOSL_TYPE_INT32:
		obj_size = 4;
		break;
	case AOSL_TYPE_INT64:
		obj_size = 8;
		break;
	case AOSL_TYPE_V4_IPADDR:
		obj_size = sizeof (aosl_in_addr_t);
		break;
	case AOSL_TYPE_V6_IPADDR:
		obj_size = sizeof (aosl_in6_addr_t);
		break;
	case AOSL_TYPE_FLOAT:
		obj_size = 4;
		break;
	case AOSL_TYPE_DOUBLE:
		obj_size = 8;
		break;
	case AOSL_TYPE_POINTER:
		obj_size = sizeof (void *);
		break;
	case AOSL_TYPE_REFERENCE:
		if (get_type_size (type->child, &obj_size) < 0)
			goto __einval;
		break;
	case AOSL_TYPE_STRING:
		obj_size = sizeof (char *);
		break;
	case AOSL_TYPE_STRUCT:
		obj_size = type->obj_size;
		break;
	case AOSL_TYPE_FIXED_BYTES:
		obj_size = type->array_size;
		break;
	case AOSL_TYPE_VAR_BYTES:
		obj_size = type->array_size;
		break;
	case AOSL_TYPE_BYTES_WITH_NIL:
		obj_size = type->array_size;
		break;
	case AOSL_TYPE_DYNAMIC_BYTES:
		obj_size = sizeof (aosl_dynamic_bytes_t);
		break;
	case AOSL_TYPE_DYNAMIC_STRING:
		obj_size = sizeof (aosl_dynamic_string_t);
		break;
	case AOSL_TYPE_FIXED_ARRAY:
		if (get_type_size (type->child, &obj_size) < 0)
			goto __einval;

		obj_size *= type->array_size;
		break;
	case AOSL_TYPE_VAR_ARRAY:
		if (get_type_size (type->child, &obj_size) < 0)
			goto __einval;

		obj_size *= type->array_size;
		break;
	case AOSL_TYPE_DYNAMIC_ARRAY:
		obj_size = sizeof (aosl_dynamic_array_t);
		break;
	default:
		/* Unknown type */
		goto __einval;
	}

	if (obj_size_p != NULL)
		*obj_size_p = obj_size;

	return 0;

__einval:
	return -AOSL_EINVAL;
}

static isize_t _____marshal (const aosl_type_info_t *type, const void *typed_obj_p, aosl_psb_t **psb_p)
{
	isize_t ret = 0;
	uint16_t val, i;
	const void *this_obj_addr = NULL;
	uint8_t bool_val;
	void *pointer_val;
	aosl_type_info_t helper_type;
	size_t obj_size;
	size_t len = 0;
	aosl_psb_t *psb = *psb_p;


#define __CHECK_AND_ENCODE_BASETYPE(var_type, __var_addr, pkt_type, fn) do { \
		void *dst = psb_put ((struct ps_buff *)psb, sizeof (pkt_type)); \
		pkt_type ____var = fn (*(const var_type *)(__var_addr)); \
		if (IS_ERR (dst)) { \
			size_t needed = sizeof (pkt_type); \
			uint8_t *helper_p = (uint8_t *)&____var; \
			for (;;) { \
				struct ps_buff *next; \
				size_t copy = psb_tailroom ((const struct ps_buff *)psb); \
				\
				if (copy > needed) \
					copy = needed; \
				memcpy (psb_put ((struct ps_buff *)psb, copy), helper_p, copy); \
				helper_p += copy; \
				needed -= copy; \
				if (needed == 0) \
					break; \
				\
				next = (struct ps_buff *)psb->next; \
				if (!next) { \
					next = alloc_psb (DFLT_MAX_PSB_PKT); \
					if (IS_ERR (next)) { \
						ret = PTR_ERR (next); \
						goto __err; \
					} \
					psb->next = (aosl_psb_t *)next; \
				} \
				psb = (aosl_psb_t *)next; \
			} \
		} else { \
			/* Do not use *(pkt_type *)(dst) = XXX for considering the unaligned cases. */ \
			memcpy ((dst), &____var, sizeof (pkt_type)); \
		} \
		len += sizeof (pkt_type); \
	} while (0)

#define CHECK_AND_ENCODE_BASETYPE(type, fn) \
__CHECK_AND_ENCODE_BASETYPE (type, this_obj_addr, type, fn)

#define __CHECK_AND_ENCODE_BYTES(src_addr, __n) do { \
		const uint8_t *__src = src_addr; \
		size_t n = (size_t)(__n); \
		while (n > 0) { \
			struct ps_buff *next; \
			size_t copy = psb_tailroom ((const struct ps_buff *)psb); \
			\
			if (copy > n) \
				copy = n; \
			memcpy (psb_put ((struct ps_buff *)psb, copy), __src, copy); \
			__src += copy; \
			n -= copy; \
			len += copy; \
			if (n == 0) \
				break; \
			next = (struct ps_buff *)psb->next; \
			if (!next) { \
				next = alloc_psb (DFLT_MAX_PSB_PKT); \
				if (IS_ERR (next)) { \
					ret = PTR_ERR (next); \
					goto __err; \
				} \
				psb->next = (aosl_psb_t *)next; \
			} \
			psb = (aosl_psb_t *)next; \
		} \
	} while (0)

#define CHECK_AND_ENCODE_BYTES(__n) __CHECK_AND_ENCODE_BYTES (this_obj_addr, __n)


	this_obj_addr = (uint8_t *)typed_obj_p + (uintptr_t)type->obj_addr;

	if (type->is_have && !type->is_have(this_obj_addr)) {
		goto __marshal_out;
	}

	switch (type->type_id) {
	case AOSL_TYPE_VOID:
		/* Do nothing */
		break;
	case AOSL_TYPE_INT8:
		CHECK_AND_ENCODE_BASETYPE (uint8_t, __encode_int8);
		break;
	case AOSL_TYPE_INT16:
		CHECK_AND_ENCODE_BASETYPE (uint16_t, __encode_int16);
		break;
	case AOSL_TYPE_INT32:
		CHECK_AND_ENCODE_BASETYPE (uint32_t, __encode_int32);
		break;
	case AOSL_TYPE_INT64:
		CHECK_AND_ENCODE_BASETYPE (uint64_t, __encode_int64);
		break;
	case AOSL_TYPE_FLOAT:
		CHECK_AND_ENCODE_BASETYPE (uint32_t, __encode_int32);
		break;
	case AOSL_TYPE_DOUBLE:
		CHECK_AND_ENCODE_BASETYPE (uint64_t, __encode_int64);
		break;
	case AOSL_TYPE_V4_IPADDR:
		CHECK_AND_ENCODE_BASETYPE (aosl_in_addr_t, __encode_ipv4);
		break;
	case AOSL_TYPE_V6_IPADDR:
		CHECK_AND_ENCODE_BASETYPE (aosl_in6_addr_t, __encode_ipv6);
		break;
	case AOSL_TYPE_POINTER:
		/* a 'logic bool' value is one byte value */
		pointer_val = *(void **)this_obj_addr;
		bool_val = (uint8_t)(pointer_val != NULL);
		helper_type.type_id = AOSL_TYPE_INT8;
		helper_type.obj_addr = &bool_val;

		ret = _____marshal (&helper_type, NULL, &psb);
		if (ret < 0)
			goto __err;

		len += ret;

		if (bool_val) {
			ret = _____marshal (type->child, pointer_val, &psb);
			if (ret < 0)
				goto __err;

			len += ret;
		}
		break;
	case AOSL_TYPE_REFERENCE:
		ret = _____marshal (type->child, this_obj_addr, &psb);
		if (ret < 0)
			goto __err;

		len += ret;
		break;
	case AOSL_TYPE_STRING:
		/* a 'logic bool' value is one byte value */
		pointer_val = *(char **)this_obj_addr;
		bool_val = (uint8_t)(pointer_val != NULL);
		helper_type.type_id = AOSL_TYPE_INT8;
		helper_type.obj_addr = &bool_val;

		ret = _____marshal (&helper_type, NULL, &psb);
		if (ret < 0)
			goto __err;

		len += ret;

		if (bool_val) {
			val = strlen ((char *)pointer_val) + 1;
			__CHECK_AND_ENCODE_BYTES (pointer_val, val);
		}
		break;
	case AOSL_TYPE_STRUCT:
		type = type->child;
		while (type->type_id != AOSL_TYPE_VOID) {
			ret = _____marshal (type, this_obj_addr, &psb);
			if (ret < 0)
				goto __err;

			len += ret;
			type++;
		}
		break;
	case AOSL_TYPE_FIXED_BYTES:
		CHECK_AND_ENCODE_BYTES (type->array_size);
		break;
	case AOSL_TYPE_VAR_BYTES:
		val = *(uint16_t *)((uint8_t *)typed_obj_p + (uintptr_t)type->count_var_addr);
		__CHECK_AND_ENCODE_BASETYPE (uint16_t, &val, uint16_t, __encode_int16);
		CHECK_AND_ENCODE_BYTES (val);
		break;
	case AOSL_TYPE_BYTES_WITH_NIL:
		val = strlen ((char *)this_obj_addr) + 1;
		/* for string, we'd better do this checking */
		if (val > type->array_size) {
			ret = -AOSL_EINVAL;
			goto __err;
		}

		CHECK_AND_ENCODE_BYTES (val);
		break;
	case AOSL_TYPE_DYNAMIC_BYTES:
	/* Fall through: the dynamic string has the same encoding mechanism with dynamic bytes. */
	case AOSL_TYPE_DYNAMIC_STRING:
		val = ((aosl_dynamic_array_t *)this_obj_addr)->count;
		__CHECK_AND_ENCODE_BASETYPE (uint16_t, &val, uint16_t, __encode_int16);
		if (val > 0) {
			pointer_val = ((aosl_dynamic_array_t *)this_obj_addr)->values;
			__CHECK_AND_ENCODE_BYTES (pointer_val, val);
		}
		break;
	case AOSL_TYPE_FIXED_ARRAY:
		if (get_type_size (type->child, &obj_size) < 0)
			goto __err;

		for (i = 0; i < type->array_size; i++) {
			ret = _____marshal (type->child, (uint8_t *)this_obj_addr + (obj_size * i), &psb);
			if (ret < 0)
				goto __err;

			len += ret;
		}
		break;
	case AOSL_TYPE_VAR_ARRAY:
		if (get_type_size (type->child, &obj_size) < 0)
			goto __err;

		val = *(uint16_t *)((uint8_t *)typed_obj_p + (uintptr_t)type->count_var_addr);
		__CHECK_AND_ENCODE_BASETYPE (uint16_t, &val, uint16_t, __encode_int16);
		for (i = 0; i < val; i++) {
			ret = _____marshal (type->child, (uint8_t *)this_obj_addr + (obj_size * i), &psb);
			if (ret < 0)
				goto __err;

			len += ret;
		}
		break;
	case AOSL_TYPE_DYNAMIC_ARRAY:
		val = ((aosl_dynamic_array_t *)this_obj_addr)->count;
		__CHECK_AND_ENCODE_BASETYPE (uint16_t, &val, uint16_t, __encode_int16);
		if (val > 0) {
			ret = get_type_size (type->child, &obj_size);
			if (ret < 0)
				goto __err;

			pointer_val = ((aosl_dynamic_array_t *)this_obj_addr)->values;
			for (i = 0; i < val; i++) {
				ret = _____marshal (type->child, (uint8_t *)pointer_val + (obj_size * i), &psb);
				if (ret < 0)
					goto __err;

				len += ret;
			}
		}
		break;
	default:
		ret = -AOSL_EINVAL;
		goto __err;
	}

__marshal_out:
	*psb_p = psb;
	return len;

__err:
	return ret;
}

/*
 * The marshaller wants a variable's address, even you just want to pass a simple number,
 * you also need to define a corresponding number variable, and then pass it's address
 * to this encoder function.
 * Surely, maybe you want to just pass the value for a simple number case, but this is
 * a common function for encapsulating all kinds of data types, especially it will
 * invoke itself recursively, and we can only determine the address of a variable for
 * many cases in the runtime (consider a struct's member case).
 * Furthermore, we usually use an address for function parameters in C except the simple
 * number cases (char, short, int, intptr_t, float, doube), for a struct, an array, a string,
 * we will pass their address, right ?
 * So, please pass the address of any data's corresponding variable as 'typed_obj_p' arg
 * to this function instead of their simple value!
 */
static isize_t smart_marshal (const aosl_type_info_t *obj_type, const void *obj_addr, aosl_psb_t *psb)
{
	return _____marshal (obj_type, obj_addr, &psb);
}

static __inline__ isize_t SAFE_STRSIZE (const aosl_psb_t *psb)
{
	size_t __l = 0;
	size_t __i = 0;
	const struct ps_buff *b = (const struct ps_buff *)psb;
	const uint8_t *__src = psb_data (b);
	for (;;) {
		if (__src + __i - (uint8_t *)b->data >= (int)b->len) {
			b = b->next;
			if (b == NULL)
				return -AOSL_EMSGSIZE;

			__src = psb_data (b);
			__i = 0;
		}

		if (__src [__i] == '\0')
			break;

		__l++;
		__i++;
	}

	__l++; /* index ==> len(including the terminating '\0') */
	return (isize_t)__l;
}

isize_t _____unmarshal (const aosl_type_info_t *type, void *typed_obj_p, const aosl_psb_t **psb_p)
{
	isize_t ret = 0;
	uint16_t val, i;
	uint8_t bool_val;
	void *pointer_val;
	aosl_type_info_t helper_type;
	void* this_obj_addr = NULL;
	size_t obj_size;
	size_t len = 0;
	const aosl_psb_t *psb = *psb_p;

#define __CHECK_AND_DECODE_BASETYPE(var_type, __var_addr, pkt_type, fn, __peek) do { \
		const void *src; \
		const int ____$peek = (int)(__peek); \
		pkt_type __pkt_var; \
		if (likely (!____$peek)) \
			src = psb_get ((struct ps_buff *)psb, sizeof (pkt_type)); \
		else \
			src = psb_peek ((const struct ps_buff *)psb, sizeof (pkt_type)); \
		if (IS_ERR (src)) { \
			size_t needed = sizeof (pkt_type); \
			uint8_t *helper_p = (uint8_t *)&__pkt_var; \
			struct ps_buff *b = (struct ps_buff *)psb; \
			src = psb_data (b); \
			for (;;) { \
				size_t copy = psb_len (b) - ((const uint8_t *)src - (const uint8_t *)psb_data (b)); \
				if (copy > needed) \
					copy = needed; \
				\
				memcpy (helper_p, src, copy); \
				if (likely (!____$peek)) \
					psb_pull (b, copy); \
				src = (const uint8_t *)src + copy; \
				helper_p += copy; \
				needed -= copy; \
				if (needed == 0) { \
					*(var_type *)(__var_addr) = fn (__pkt_var); \
					if (!____$peek) \
						psb = (aosl_psb_t *)b; \
					break; \
				} \
				b = b->next; \
				if (b == NULL) { \
					ret = -AOSL_EMSGSIZE; \
					goto __err; \
				} \
				src = psb_data (b); \
			} \
		} else { \
			/* Do not use *(pkt_type *)(src) for considering the unaligned cases. */ \
			memcpy (&__pkt_var, src, sizeof (pkt_type)); \
			*(var_type *)(__var_addr) = fn (__pkt_var); \
		} \
		if (!____$peek) \
			len += sizeof (pkt_type); \
	} while (0)

#define CHECK_AND_DECODE_BASETYPE(type, fn) \
	__CHECK_AND_DECODE_BASETYPE (type, this_obj_addr, type, fn, 0)

#define __CHECK_AND_DECODE_BYTES(dst_addr, __n) do { \
		uint8_t *__dst = dst_addr; \
		size_t n = (size_t)(__n); \
		for (;;) { \
			size_t copy = psb_len ((const struct ps_buff *)psb); \
			if (copy > n) \
				copy = n; \
			memcpy (__dst, psb_get ((struct ps_buff *)psb, copy), copy); \
			__dst += copy; \
			n -= copy; \
			len += copy; \
			if (n == 0) \
				break; \
			psb = psb->next; \
			if (psb == NULL) { \
				ret = -AOSL_EMSGSIZE; \
				goto __err; \
			} \
		} \
	} while (0)

#define CHECK_AND_DECODE_BYTES(__n) __CHECK_AND_DECODE_BYTES (this_obj_addr, __n)


	this_obj_addr = (uint8_t *)typed_obj_p + (uintptr_t)type->obj_addr;

	if (type->is_have && !type->is_have(this_obj_addr)) {
		goto __unmarshal_out;
	}

	switch (type->type_id) {
	case AOSL_TYPE_VOID:
		/* Do nothing */
		break;
	case AOSL_TYPE_INT8:
		CHECK_AND_DECODE_BASETYPE (uint8_t, __decode_int8);
		break;
	case AOSL_TYPE_INT16:
		CHECK_AND_DECODE_BASETYPE (uint16_t, __decode_int16);
		break;
	case AOSL_TYPE_INT32:
		CHECK_AND_DECODE_BASETYPE (uint32_t, __decode_int32);
		break;
	case AOSL_TYPE_INT64:
		CHECK_AND_DECODE_BASETYPE (uint64_t, __decode_int64);
		break;
	case AOSL_TYPE_FLOAT:
		CHECK_AND_DECODE_BASETYPE (uint32_t, __decode_int32);
		break;
	case AOSL_TYPE_DOUBLE:
		CHECK_AND_DECODE_BASETYPE (uint64_t, __decode_int64);
		break;
	case AOSL_TYPE_V4_IPADDR:
		CHECK_AND_DECODE_BASETYPE (aosl_in_addr_t, __decode_ipv4);
		break;
	case AOSL_TYPE_V6_IPADDR:
		CHECK_AND_DECODE_BASETYPE (aosl_in6_addr_t, __decode_ipv6);
		break;
	case AOSL_TYPE_POINTER:
		/* a 'logic bool' value is one byte value */
		helper_type.type_id = AOSL_TYPE_INT8;
		helper_type.obj_addr = &bool_val;
		ret = _____unmarshal (&helper_type, NULL, &psb);
		if (ret < 0)
			goto __err;

		len += ret;

		if (bool_val) {
			/*
				* For a 'array' type pointer, we should get the 'count' value first
				* to allocate memory;
				* For a 'non-array' pointer, the 'count' is 1;
				* Please pay attention to the followings:
				* 1. 'string', 'bytes' type may not set the 'obj_size' due to it is well known 1;
				* 2. all non-array type may not set the 'array_size' due to it is a well known 1;
				*/
			if (get_type_size (type->child, &obj_size) < 0)
				goto __err;

			pointer_val = aosl_malloc (obj_size);
			if (pointer_val == NULL) {
				ret = -AOSL_ENOMEM;
				goto __err;
			}

			/**
			 * We must set the the pointer first to avoid memory leak if encountered error,
			 * because once we set the pointer, then we can free it when we finish the typed
			 * object later.
			 **/
			*(void **)this_obj_addr = pointer_val;

			ret = _____unmarshal (type->child, pointer_val, &psb);
			if (ret < 0)
				goto __err;

			len += ret;
		} else {
			*(void **)this_obj_addr = NULL;
		}
		break;
	case AOSL_TYPE_REFERENCE:
		ret = _____unmarshal (type->child, this_obj_addr, &psb);
		if (ret < 0)
			goto __err;

		len += ret;
		break;
	case AOSL_TYPE_STRING:
		/* a 'logic bool' value is one byte value */
		helper_type.type_id = AOSL_TYPE_INT8;
		helper_type.obj_addr = &bool_val;
		ret = _____unmarshal (&helper_type, NULL, &psb);
		if (ret < 0)
			goto __err;

		len += ret;

		if (bool_val) {
			ret = SAFE_STRSIZE (psb);
			if (ret < 0)
				goto __err;

			val = (uint16_t)ret;
			pointer_val = aosl_malloc (val);
			if (pointer_val == NULL) {
				ret = -AOSL_ENOMEM;
				goto __err;
			}

			/**
			 * We must set the the pointer first to avoid memory leak if encountered error,
			 * because once we set the pointer, then we can free it when we finish the typed
			 * object later.
			 **/
			*(void **)this_obj_addr = pointer_val;

			__CHECK_AND_DECODE_BYTES (pointer_val, val);
		} else {
			*(void **)this_obj_addr = NULL;
		}
		break;
	case AOSL_TYPE_STRUCT:
		type = type->child;
		while (type->type_id != AOSL_TYPE_VOID) {
			ret = _____unmarshal (type, this_obj_addr, &psb);
			if (ret < 0)
				goto __err;

			len += ret;
			type++;
		}
		break;
	case AOSL_TYPE_FIXED_BYTES:
		CHECK_AND_DECODE_BYTES (type->array_size);
		break;
	case AOSL_TYPE_VAR_BYTES:
		__CHECK_AND_DECODE_BASETYPE (uint16_t, &val, uint16_t, __decode_int16, 0);
		if (val > type->array_size) {
			ret = -AOSL_ENOSPC;
			goto __err;
		}

		*(uint16_t *)((uint8_t *)typed_obj_p + (uintptr_t)type->count_var_addr) = val;
		CHECK_AND_DECODE_BYTES (val);
		break;
	case AOSL_TYPE_BYTES_WITH_NIL:
		ret = SAFE_STRSIZE (psb);
		if (ret < 0)
			goto __err;

		val = (uint16_t)ret;
		if (val > type->array_size) {
			ret = -AOSL_ENOSPC;
			goto __err;
		}

		CHECK_AND_DECODE_BYTES (val);
		break;
	case AOSL_TYPE_DYNAMIC_BYTES:
	/**
	 * Fall through: the dynamic string has almost the same decoding mechanism
	 * with dynamic bytes except it will allocate one more byte for holding the
	 * terminated '\0' character.
	 **/
	case AOSL_TYPE_DYNAMIC_STRING:
		__CHECK_AND_DECODE_BASETYPE (uint16_t, &val, uint16_t, __decode_int16, 0);
		((aosl_dynamic_array_t *)this_obj_addr)->count = val;
		((aosl_dynamic_array_t *)this_obj_addr)->allocated = val + (type->type_id == AOSL_TYPE_DYNAMIC_STRING);
		if (val + (type->type_id == AOSL_TYPE_DYNAMIC_STRING) > 0) {
			pointer_val = aosl_malloc (val + (type->type_id == AOSL_TYPE_DYNAMIC_STRING));
			if (pointer_val == NULL) {
				((aosl_dynamic_array_t *)this_obj_addr)->count = 0;
				((aosl_dynamic_array_t *)this_obj_addr)->allocated = 0;
				ret = -AOSL_ENOMEM;
				goto __err;
			}

			/**
			 * We must set the the pointer first to avoid memory leak if encountered error,
			 * because once we set the pointer, then we can free it when we finish the typed
			 * object later.
			 **/
			((aosl_dynamic_array_t *)this_obj_addr)->values = pointer_val;

			if (val > 0) {
				__CHECK_AND_DECODE_BYTES (pointer_val, val);
			}

			/* Terminate with a '\0' if this is a dynamic string */
			if (type->type_id == AOSL_TYPE_DYNAMIC_STRING)
				*((char *)pointer_val + val) = '\0';
		}
		break;
	case AOSL_TYPE_FIXED_ARRAY:
		if (get_type_size (type->child, &obj_size) < 0)
			goto __err;

		for (i = 0; i < type->array_size; i++) {
			ret = _____unmarshal (type->child, (uint8_t *)this_obj_addr + (obj_size * i), &psb);
			if (ret < 0)
				goto __err;

			len += ret;
		}
		break;
	case AOSL_TYPE_VAR_ARRAY:
		if (get_type_size (type->child, &obj_size) < 0)
			goto __err;

		__CHECK_AND_DECODE_BASETYPE (uint16_t, &val, uint16_t, __decode_int16, 0);
		if (val > type->array_size) {
			ret = -AOSL_ENOSPC;
			goto __err;
		}

		*(uint16_t *)((uint8_t *)typed_obj_p + (uintptr_t)type->count_var_addr) = val;
		for (i = 0; i < val; i++) {
			ret = _____unmarshal (type->child, (uint8_t *)this_obj_addr + (obj_size * i), &psb);
			if (ret < 0)
				goto __err;

			len += ret;
		}
		break;
	case AOSL_TYPE_DYNAMIC_ARRAY:
		__CHECK_AND_DECODE_BASETYPE (uint16_t, &val, uint16_t, __decode_int16, 0);
		((aosl_dynamic_array_t *)this_obj_addr)->count = val;
		((aosl_dynamic_array_t *)this_obj_addr)->allocated = val;
		if (val > 0) {
			ret = get_type_size (type->child, &obj_size);
			if (ret < 0) {
				((aosl_dynamic_array_t *)this_obj_addr)->count = 0;
				((aosl_dynamic_array_t *)this_obj_addr)->allocated = 0;
				goto __err;
			}

			pointer_val = aosl_malloc (obj_size * val);
			if (pointer_val == NULL) {
				((aosl_dynamic_array_t *)this_obj_addr)->count = 0;
				((aosl_dynamic_array_t *)this_obj_addr)->allocated = 0;
				ret = -AOSL_ENOMEM;
				goto __err;
			}

			/**
			 * We must set the the pointer first to avoid memory leak if encountered error,
			 * because once we set the pointer, then we can free it when we finish the typed
			 * object later.
			 **/
			((aosl_dynamic_array_t *)this_obj_addr)->values = pointer_val;

			for (i = 0; i < val; i++) {
				ret = _____unmarshal (type->child, (uint8_t *)pointer_val + (obj_size * i), &psb);
				if (ret < 0)
					goto __err;

				len += ret;
			}
		}
		break;
	default:
		ret = -AOSL_EINVAL;
		goto __err;
	}

__unmarshal_out:
	*psb_p = psb;
	return len;

__err:
	return ret;
}

static isize_t smart_unmarshal (const aosl_type_info_t *obj_type, void *obj_addr, const aosl_psb_t *psb)
{
	return _____unmarshal (obj_type, obj_addr, &psb);
}

/*
 * For any object, we will pass its' address to this function because
 * we can only calculate the address of a struct member through the
 * base address! Surely, for a pointer member, we also can only calculate
 * its' address, not the pointer's value!
 * You MUST NOT have the idea that just passing the pointer's value to
 * this function due to you just want to free its' memory, KEEP IT IN
 * MIND all the time that this function will call itself recursively,
 * and for a pointer as a member of a structure, we can only get it's
 * address first, and then retrieve its' value indirectly, so it is
 * impossible to implement this function via passing the pointer's value!
 */
static void smart_init_typed_obj (const aosl_type_info_t *type, const void *typed_obj_p)
{
	size_t v;
	void *this_obj_addr;
	size_t obj_size;

	this_obj_addr = (char *)typed_obj_p + (uintptr_t)type->obj_addr;
	switch (type->type_id) {
	case AOSL_TYPE_INT8:
		*(uint8_t *)this_obj_addr = 0;
		break;
	case AOSL_TYPE_INT16:
		*(uint16_t *)this_obj_addr = 0;
		break;
	case AOSL_TYPE_INT32:
		*(uint32_t *)this_obj_addr = 0;
		break;
	case AOSL_TYPE_INT64:
		*(uint64_t *)this_obj_addr = 0;
		break;
	case AOSL_TYPE_FLOAT:
		*(float *)this_obj_addr = 0.0;
		break;
	case AOSL_TYPE_DOUBLE:
		*(double *)this_obj_addr = 0.0;
		break;
	case AOSL_TYPE_V4_IPADDR:
		*(uint32_t *)this_obj_addr = 0;
		break;
	case AOSL_TYPE_V6_IPADDR:
		memset (this_obj_addr, 0, sizeof (aosl_in6_addr_t));
		break;
	case AOSL_TYPE_POINTER:
		*(void **)this_obj_addr = NULL;
		break;
	case AOSL_TYPE_REFERENCE:
		smart_init_typed_obj (type->child, this_obj_addr);
		break;
	case AOSL_TYPE_STRING:
		*(void **)this_obj_addr = NULL;
		break;
	case AOSL_TYPE_STRUCT:
		for (type = type->child; type->type_id != AOSL_TYPE_VOID; type++)
			smart_init_typed_obj (type, this_obj_addr);
		break;
	case AOSL_TYPE_FIXED_BYTES:
		memset (this_obj_addr, 0, type->array_size);
		break;
	case AOSL_TYPE_VAR_BYTES:
		*(uint16_t *)((uint8_t *)typed_obj_p + (uintptr_t)type->count_var_addr) = 0;
		break;
	case AOSL_TYPE_BYTES_WITH_NIL:
		*(char *)this_obj_addr = '\0';
		break;
	case AOSL_TYPE_DYNAMIC_BYTES:
	/* Fall through: the dynamic string has the same init mechanism with dynamic bytes. */
	case AOSL_TYPE_DYNAMIC_STRING:
		((aosl_dynamic_array_t *)this_obj_addr)->count = 0;
		((aosl_dynamic_array_t *)this_obj_addr)->allocated = 0;
		((aosl_dynamic_array_t *)this_obj_addr)->values = NULL;
		break;
	case AOSL_TYPE_FIXED_ARRAY:
		if (get_type_size (type->child, &obj_size) < 0)
			break;

		for (v = 0; v < (size_t)type->array_size; v++)
			smart_init_typed_obj (type->child, (uint8_t *)this_obj_addr + (obj_size * v));
		break;
	case AOSL_TYPE_VAR_ARRAY:
		*(uint16_t *)((uint8_t *)typed_obj_p + (uintptr_t)type->count_var_addr) = 0;
		break;
	case AOSL_TYPE_DYNAMIC_ARRAY:
		((aosl_dynamic_array_t *)this_obj_addr)->count = 0;
		((aosl_dynamic_array_t *)this_obj_addr)->allocated = 0;
		((aosl_dynamic_array_t *)this_obj_addr)->values = NULL;
		break;
	default:
		break;
	}
}

/*
 * For any object, we will pass its' address to this function because
 * we can only calculate the address of a struct member through the
 * base address! Surely, for a pointer member, we also can only calculate
 * its' address, not the pointer's value!
 * You MUST NOT have the idea that just passing the pointer's value to
 * this function due to you just want to free its' memory, KEEP IT IN
 * MIND all the time that this function will call itself recursively,
 * and for a pointer as a member of a structure, we can only get it's
 * address first, and then retrieve its' value indirectly, so it is
 * impossible to implement this function via passing the pointer's value!
 */
static void smart_fini_typed_obj (const aosl_type_info_t *type, const void *typed_obj_p)
{
	void *p;
	size_t v;
	void *this_obj_addr;
	size_t obj_size;
	uint16_t count;

	this_obj_addr = (char *)typed_obj_p + (uintptr_t)type->obj_addr;
	switch (type->type_id) {
	case AOSL_TYPE_POINTER:
		p = *(void **)this_obj_addr;
		if (p != NULL) {
			smart_fini_typed_obj (type->child, p);
			aosl_free (p);
			*(void **)this_obj_addr = NULL;
		}
		break;
	case AOSL_TYPE_REFERENCE:
		smart_fini_typed_obj (type->child, this_obj_addr);
		break;
	case AOSL_TYPE_STRING:
		p = *(void **)this_obj_addr;
		if (p != NULL) {
			aosl_free (p);
			*(void **)this_obj_addr = NULL;
		}
		break;
	case AOSL_TYPE_STRUCT:
		for (type = type->child; type->type_id != AOSL_TYPE_VOID; type++)
			smart_fini_typed_obj (type, this_obj_addr);
		break;
	case AOSL_TYPE_DYNAMIC_BYTES:
	/* Fall through: the dynamic string has the same fini mechanism with dynamic bytes. */
	case AOSL_TYPE_DYNAMIC_STRING:
		if (((aosl_dynamic_array_t *)this_obj_addr)->allocated > 0)
			aosl_free (((aosl_dynamic_array_t *)this_obj_addr)->values);

		((aosl_dynamic_array_t *)this_obj_addr)->count = 0;
		((aosl_dynamic_array_t *)this_obj_addr)->allocated = 0;
		((aosl_dynamic_array_t *)this_obj_addr)->values = NULL;
		break;
	case AOSL_TYPE_FIXED_ARRAY:
		if (get_type_size (type->child, &obj_size) < 0)
			break;

		for (v = 0; v < (size_t)type->array_size; v++)
			smart_fini_typed_obj (type->child, (uint8_t *)this_obj_addr + (obj_size * v));
		break;
	case AOSL_TYPE_VAR_ARRAY:
		if (get_type_size (type->child, &obj_size) < 0)
			break;

		for (v = 0; v < (size_t)*(uint16_t *)((uint8_t *)typed_obj_p + (uintptr_t)type->count_var_addr); v++)
			smart_fini_typed_obj (type->child, (uint8_t *)this_obj_addr + (obj_size * v));

		*(uint16_t *)((uint8_t *)typed_obj_p + (uintptr_t)type->count_var_addr) = 0;
		break;
	case AOSL_TYPE_DYNAMIC_ARRAY:
		if (((aosl_dynamic_array_t *)this_obj_addr)->allocated > 0) {
			p = ((aosl_dynamic_array_t *)this_obj_addr)->values;
			count = ((aosl_dynamic_array_t *)this_obj_addr)->count;
			if (count > 0) {
				if (get_type_size (type->child, &obj_size) == 0) {
					for (v = 0; v < (size_t)count; v++)
						smart_fini_typed_obj (type->child, (uint8_t *)p + (obj_size * v));
				}
			}

			aosl_free (p);
		}

		((aosl_dynamic_array_t *)this_obj_addr)->count = 0;
		((aosl_dynamic_array_t *)this_obj_addr)->allocated = 0;
		((aosl_dynamic_array_t *)this_obj_addr)->values = NULL;
		break;
	default:
		break;
	}
}

__export_in_so__ isize_t aosl_marshal (const aosl_type_info_t *type, const void *typed_obj_p, aosl_psb_t *psb)
{
	isize_t err;

	err = smart_marshal (type, typed_obj_p, psb);
	return_err (err);
}

__export_in_so__ isize_t aosl_unmarshal (const aosl_type_info_t *type, void *typed_obj_p, const aosl_psb_t *psb)
{
	isize_t err;

	err = smart_unmarshal (type, typed_obj_p, psb);
	return_err (err);
}

__export_in_so__ void aosl_init_typed_obj (const aosl_type_info_t *type, void *typed_obj_p)
{
	smart_init_typed_obj (type, typed_obj_p);
}

__export_in_so__ void aosl_fini_typed_obj (const aosl_type_info_t *type, const void *typed_obj_p)
{
	smart_fini_typed_obj (type, typed_obj_p);
}

__export_in_so__ uint16_t aosl_encode_int16 (uint16_t v)
{
	return __encode_int16 (v);
}

__export_in_so__ uint32_t aosl_encode_int32 (uint32_t v)
{
	return __encode_int32 (v);
}

__export_in_so__ uint64_t aosl_encode_int64 (uint64_t v)
{
	return __encode_int64 (v);
}

__export_in_so__ uint16_t aosl_decode_int16 (uint16_t v)
{
	return __encode_int16 (v);
}

__export_in_so__ uint32_t aosl_decode_int32 (uint32_t v)
{
	return __encode_int32 (v);
}

__export_in_so__ uint64_t aosl_decode_int64 (uint64_t v)
{
	return __encode_int64 (v);
}
