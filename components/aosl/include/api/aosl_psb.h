/***************************************************************************
 * Module:	Packet Segment Buffer relatives header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_PSB_H__
#define __AOSL_PSB_H__


#include <api/aosl_types.h>
#include <api/aosl_defs.h>


#ifdef __cplusplus
extern "C" {
#endif



#define DFLT_MAX_PSB_PKT (4 << 10) /* default max packet size is 4K */
#define MAX_PSB_PKT_SIZE (2 << 20) /* max packet size is limited to 2M */

/* psb size is 2 times of max packet, this is default */
#define DEFAULT_PSB_SIZE (DFLT_MAX_PSB_PKT << 1)

/* AOSL Packet Segment Buffer */
typedef struct _____psb {
	void *data;
	size_t len;
	struct _____psb *next;
} aosl_psb_t;


/**
 * @brief Allocate a PSB that wraps a user-provided buffer.
 * @param [in] buf    the user buffer
 * @param [in] bufsz  the buffer size in bytes
 * @return            pointer to the PSB, or NULL on failure
 **/
extern __aosl_api__ aosl_psb_t *aosl_alloc_user_psb (void *buf, size_t bufsz);

/**
 * @brief Allocate a PSB with an internal buffer of the specified size.
 * @param [in] size  the internal buffer size in bytes
 * @return           pointer to the PSB, or NULL on failure
 **/
extern __aosl_api__ aosl_psb_t *aosl_alloc_psb (size_t size);

/**
 * @brief Attach an external buffer to an existing PSB.
 * @param [in,out] psb    the PSB to attach to
 * @param [in]     buf    the buffer to attach
 * @param [in]     bufsz  the buffer size in bytes
 **/
extern __aosl_api__ void aosl_psb_attach_buf (aosl_psb_t *psb, void *buf, size_t bufsz);

/**
 * @brief Detach the buffer from a PSB without freeing it.
 * @param [in,out] psb  the PSB to detach from
 **/
extern __aosl_api__ void aosl_psb_detach_buf (aosl_psb_t *psb);

/**
 * @brief Get the available headroom (free space before data) of a PSB.
 * @param [in] psb  the PSB
 * @return          the headroom size in bytes
 **/
extern __aosl_api__ size_t aosl_psb_headroom (const aosl_psb_t *psb);

/**
 * @brief Get the available tailroom (free space after data) of a PSB.
 * @param [in] psb  the PSB
 * @return          the tailroom size in bytes
 **/
extern __aosl_api__ size_t aosl_psb_tailroom (const aosl_psb_t *psb);

/**
 * @brief Reserve headroom space in a PSB before adding data.
 * @param [in,out] psb  the PSB
 * @param [in]     len  the number of bytes to reserve
 * @return              0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_psb_reserve (aosl_psb_t *psb, size_t len);

/**
 * @brief Get the pointer to the data area of a PSB.
 * @param [in] psb  the PSB
 * @return          pointer to the data
 **/
extern __aosl_api__ void *aosl_psb_data (const aosl_psb_t *psb);

/**
 * @brief Get the length of data in a single PSB segment.
 * @param [in] psb  the PSB
 * @return          the data length in bytes
 **/
extern __aosl_api__ size_t aosl_psb_len (const aosl_psb_t *psb);

/**
 * @brief Get the total data length across all chained PSB segments.
 * @param [in] psb  the head PSB of the chain
 * @return          the total data length in bytes
 **/
extern __aosl_api__ size_t aosl_psb_total_len (const aosl_psb_t *psb);

/**
 * @brief Append data to the tail of a PSB, extending the data length.
 * @param [in,out] psb  the PSB
 * @param [in]     len  the number of bytes to append
 * @return              pointer to the start of the appended area
 **/
extern __aosl_api__ void *aosl_psb_put (aosl_psb_t *psb, size_t len);

/**
 * @brief Remove data from the head of a PSB (consume data).
 * @param [in,out] psb  the PSB
 * @param [in]     len  the number of bytes to remove
 * @return              pointer to the removed data area
 **/
extern __aosl_api__ void *aosl_psb_get (aosl_psb_t *psb, size_t len);

/**
 * @brief Peek at data from the head of a PSB without consuming it.
 * @param [in] psb  the PSB
 * @param [in] len  the number of bytes to peek
 * @return          pointer to the data, or NULL if insufficient data
 **/
extern __aosl_api__ void *aosl_psb_peek (const aosl_psb_t *psb, size_t len);

/**
 * @brief Prepend data to the head of a PSB, using headroom space.
 * @param [in,out] psb  the PSB
 * @param [in]     len  the number of bytes to prepend
 * @return              pointer to the start of the prepended area
 **/
extern __aosl_api__ void *aosl_psb_push (aosl_psb_t *psb, size_t len);

/**
 * @brief Remove data from the head of a PSB by advancing the data pointer.
 * @param [in,out] psb  the PSB
 * @param [in]     len  the number of bytes to pull
 * @return              pointer to the new data start
 **/
extern __aosl_api__ void *aosl_psb_pull (aosl_psb_t *psb, size_t len);

/**
 * @brief Check if a PSB is a single segment (not chained).
 * @param [in] psb  the PSB
 * @return          non-zero if single segment, 0 if chained
 **/
extern __aosl_api__ int aosl_psb_single (const aosl_psb_t *psb);

/**
 * @brief Reset a PSB to its initial empty state.
 * @param [in,out] psb  the PSB
 **/
extern __aosl_api__ void aosl_psb_reset (aosl_psb_t *psb);

/**
 * @brief Free a PSB and all chained segments in the list.
 * @param [in] psb  the head PSB of the chain
 **/
extern __aosl_api__ void aosl_free_psb_list (aosl_psb_t *psb);



#ifdef __cplusplus
}
#endif



#endif /* __AOSL_PSB_H__ */
