/***************************************************************************
 * Module:	Time relative utilities implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#include <api/aosl_types.h>
#include <api/aosl_time.h>
#include <hal/aosl_hal_time.h>


__export_in_so__ aosl_ts_t aosl_tick_now (void)
{
	return (aosl_ts_t)(aosl_hal_get_tick_ms());
}

__export_in_so__ aosl_ts_t aosl_tick_ms (void)
{
	return (aosl_ts_t)(aosl_hal_get_tick_ms ());
}

__export_in_so__ aosl_ts_t aosl_tick_us (void)
{
	return 0;
}

__export_in_so__ aosl_ts_t aosl_time_sec (void)
{
	return (aosl_ts_t)(aosl_hal_get_time_ms () / 1000);
}

__export_in_so__ aosl_ts_t aosl_time_ms (void)
{
	return (aosl_ts_t)(aosl_hal_get_time_ms ());
}

__export_in_so__ void aosl_msleep (uint64_t ms)
{
	if (ms == 0) {
		ms = 1;
	}
	aosl_hal_msleep (ms);
}

__export_in_so__ int aosl_time_str(char *buf, int len)
{
	return aosl_hal_get_time_str(buf, len);
}
