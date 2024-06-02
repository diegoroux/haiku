/*
 *  Copyright 2024, Diego Roux, diegoroux04 at proton dot me
 *  Distributed under the terms of the MIT License.
 */

#include <hmulti_audio.h>

#include <string.h>

#include "driver.h"


status_t
multi_get_description(VirtIOSoundDriverInfo* info, multi_description* desc)
{
	desc->interface_version = B_CURRENT_INTERFACE_VERSION;
	desc->interface_minimum = B_CURRENT_INTERFACE_VERSION;

	strcpy(desc->friendly_name, "VirtIO Sound Device");
	strcpy(desc->vendor_info, "Haiku");

	desc->output_bus_channel_count = 0;
	desc->input_bus_channel_count = 0;
	desc->aux_bus_channel_count = 0;

	memset((void*)desc->channels, 0x00, sizeof(multi_channel_info) * desc->request_channel_count);

	desc->interface_flags = 0x00;

	if (info->outputStream != NULL) {
		desc->output_channel_count = info->outputStream->channels_min;

		desc->output_rates = info->outputStream->best_rate;
		desc->output_formats = info->outputStream->best_format;

		desc->interface_flags |= B_MULTI_INTERFACE_PLAYBACK;
	}

	if (info->inputStream != NULL) {
		desc->input_channel_count = info->inputStream->channels_min;

		desc->input_rates = info->inputStream->best_rate;
		desc->input_formats = info->inputStream->best_format;

		desc->interface_flags |= B_MULTI_INTERFACE_RECORD;
	}

	desc->max_cvsr_rate = 0;
	desc->min_cvsr_rate = 0;

	desc->lock_sources = B_MULTI_LOCK_INTERNAL;
	desc->timecode_sources = 0;

	desc->start_latency = 0;

	desc->control_panel[0] = '\0';

	return B_OK;
}