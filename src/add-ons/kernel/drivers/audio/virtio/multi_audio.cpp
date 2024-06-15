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

	// TODO: Return a real channel map.
	memset((void*)desc->channels, 0x00,
		sizeof(multi_channel_info) * desc->request_channel_count);

	desc->interface_flags = 0x00;

	// TODO: manage I/O streams, no hardcoding.
	if (info->outputStreams) {
		desc->output_channel_count = info->streams[0].channels_min;

		desc->output_rates = info->streams[0].rates;
		desc->output_formats = info->streams[0].formats;

		desc->interface_flags |= B_MULTI_INTERFACE_PLAYBACK;
	}

	if (info->inputStreams) {
		desc->input_channel_count = info->streams[1].channels_min;

		desc->input_rates = info->streams[1].rates;
		desc->input_formats = info->streams[1].formats;

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