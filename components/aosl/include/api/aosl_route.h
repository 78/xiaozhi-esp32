/***************************************************************************
 * Module:	Network route relative functionals header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_NET_API_H__
#define __AOSL_NET_API_H__

#include <api/aosl_types.h>
#include <api/aosl_defs.h>
#include <api/aosl_mpq_net.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
	AOSL_NET_EV_NONE = 0,
	AOSL_NET_EV_NET_DOWN,
	AOSL_NET_EV_NET_UP,
	AOSL_NET_EV_NET_UP_CHANGED,
	AOSL_NET_EV_NET_SWITCH,
	AOSL_NET_EV_RT_CNT_CHANGED,
} aosl_net_ev_t;

typedef struct {
	int if_index;
	int if_type;
	char if_name [64];
} aosl_netif_t;

typedef struct {
	aosl_netif_t netif;
	int if_cellnet;
	int if_wireless;
	int def_rt_cnt;
	aosl_sk_addr_t gw;
} aosl_rt_t;

typedef struct {
	aosl_rt_t IPv4;
	aosl_rt_t IPv6;
} aosl_def_rt_t;

/**
 * @brief Initialize a default route structure to its initial state.
 * @param [out] def_rt  pointer to the default route structure
 **/
extern __aosl_api__ void aosl_init_def_rt (aosl_def_rt_t *def_rt);

/**
 * @brief Invalidate a single route entry.
 * @param [in,out] rt  pointer to the route entry
 **/
extern __aosl_api__ void aosl_invalidate_rt (aosl_rt_t *rt);

/**
 * @brief Invalidate both IPv4 and IPv6 default route entries.
 * @param [in,out] def_rt  pointer to the default route structure
 **/
extern __aosl_api__ void aosl_invalidate_def_rt (aosl_def_rt_t *def_rt);

/**
 * @brief Check if a route entry is valid.
 * @param [in] rt  pointer to the route entry
 * @return         non-zero if valid, 0 if invalid
 **/
extern __aosl_api__ int aosl_rt_valid (const aosl_rt_t *rt);

/**
 * @brief Check if at least one of the default routes (IPv4 or IPv6) is valid.
 * @param [in] def_rt  pointer to the default route structure
 * @return             non-zero if valid, 0 if both invalid
 **/
extern __aosl_api__ int aosl_def_rt_valid (const aosl_def_rt_t *def_rt);

/**
 * @brief Check if the network is currently down (no default route available).
 * @return  non-zero if network is down, 0 if up
 **/
extern __aosl_api__ int aosl_network_is_down (void);

/**
 * @brief Compare two route entries for equality.
 * @param [in] rt1  the first route entry
 * @param [in] rt2  the second route entry
 * @return          non-zero if same, 0 if different
 **/
extern __aosl_api__ int aosl_same_rt (const aosl_rt_t *rt1, const aosl_rt_t *rt2);

/**
 * @brief Compare two default route structures for equality.
 * @param [in] def_rt1  the first default route
 * @param [in] def_rt2  the second default route
 * @return              non-zero if same, 0 if different
 **/
extern __aosl_api__ int aosl_same_def_rt (const aosl_def_rt_t *def_rt1, const aosl_def_rt_t *def_rt2);

/**
 * @brief Convert a route entry to a human-readable string.
 * @param [in]  rt        the route entry
 * @param [out] buf       the output buffer
 * @param [in]  buf_size  the buffer size in bytes
 * @return                pointer to buf on success
 **/
extern __aosl_api__ const char *aosl_rt_str (const aosl_rt_t *rt, char buf [], size_t buf_size);

/**
 * @brief Convert a default route structure to a human-readable string.
 * @param [in]  def_rt    the default route structure
 * @param [out] buf       the output buffer
 * @param [in]  buf_size  the buffer size in bytes
 * @return                pointer to buf on success
 **/
extern __aosl_api__ const char *aosl_def_rt_str (const aosl_def_rt_t *def_rt, char buf [], size_t buf_size);


typedef void (*aosl_net_ev_func_t) (aosl_net_ev_t ev, void *arg, ...);

/**
 * @brief Subscribe/Unsubscribe the low level network events.
 * If f is not NULL, then subscribe the low level network events, and the callback
 * function specified by f will be invoked in the calling thread context. Please
 * note that only one subscriber allowed in the whole process.
 * If f is NULL, then unsubscribe the low level network events.
 * @param [in] f    the callback function, or NULL to unsubscribe
 * @param [in] arg  the parameter passed to the callback
 * @return          0 on success, <0 on failure with errno set
 **/
extern __aosl_api__ int aosl_subscribe_net_events (aosl_net_ev_func_t f, void *arg);

/**
 * @brief Get the current default route information.
 * @param [out] def_rt  pointer to receive the default route info
 * @return              the default route count (0: none, 1: IPv4 or IPv6 only, 2: both)
 **/
extern __aosl_api__ int aosl_get_default_rt (aosl_def_rt_t *def_rt);


/**
 * @brief Check if the current default route is a mobile/cellular network.
 * @param [in] af  the address family to check (AF_INET or AF_INET6)
 * @return         non-zero if mobile network, 0 otherwise
 **/
extern __aosl_api__ int aosl_is_mobile_net (uint16_t af);


#ifdef __cplusplus
}
#endif


#endif /* __AOSL_NET_API_H__ */
