/*
 *  Copyright 2024, Diego Roux, diegoroux04 at proton dot me
 *  Distributed under the terms of the MIT License.
 */

#include <hmulti_audio.h>

#include <string.h>

#include "virtio_sound.h"
#include "driver.h"


VirtIOSoundPCMInfo*
get_stream(VirtIOSoundDriverInfo* info, uint8 direction)
{
	VirtIOSoundPCMInfo* stream = NULL;

	for (uint32 i = 0; i < info->nStreams; i++) {
		stream = &info->streams[i];

		if (stream->direction == direction)
			break;
	}

	return stream;
}


static void
create_multi_channel_info(VirtIOSoundDriverInfo* info, multi_channel_info* channels)
{
	uint32 index = 0;

	for (uint32 i = 0; i < 2; i++) {
		VirtIOSoundPCMInfo* stream = get_stream(info, i);
		if (stream == NULL)
			continue;

		for (uint8 j = 0; j < stream->channels; j++) {
			channels[index].channel_id = index;

			channels[index].kind = (stream->direction == VIRTIO_SND_D_OUTPUT)
				? B_MULTI_OUTPUT_CHANNEL : B_MULTI_INPUT_CHANNEL;

			channels[index].designations = stream->chmap[index];

			if (stream->channels == 2) {
				channels[index].designations |= B_CHANNEL_STEREO_BUS;
			} else if (stream->channels > 2) {
				channels[index].designations |= B_CHANNEL_SURROUND_BUS;
			}

			index++;
		}
	}

	return;
}


status_t
multi_get_description(VirtIOSoundDriverInfo* info, void* buffer)
{
	multi_description* desc = (multi_description*)buffer;

	desc->interface_version = B_CURRENT_INTERFACE_VERSION;
	desc->interface_minimum = B_CURRENT_INTERFACE_VERSION;

	strcpy(desc->friendly_name, "VirtIO Sound Device");
	strcpy(desc->vendor_info, "Haiku");

	desc->input_channel_count = 0;
	desc->output_channel_count = 0;

	desc->output_bus_channel_count = 0;
	desc->input_bus_channel_count = 0;
	desc->aux_bus_channel_count = 0;

	desc->interface_flags = 0x00;

	if (info->outputStreams) {
		VirtIOSoundPCMInfo* oStream = get_stream(info, VIRTIO_SND_D_OUTPUT);

		desc->output_channel_count = oStream->channels;

		desc->output_rates = oStream->rates;
		desc->output_formats = oStream->formats;

		desc->interface_flags |= B_MULTI_INTERFACE_PLAYBACK;
	}

	if (info->inputStreams) {
		VirtIOSoundPCMInfo* iStream = get_stream(info, VIRTIO_SND_D_INPUT);
		
		desc->input_channel_count = iStream->channels;

		desc->input_rates = iStream->rates;
		desc->input_formats = iStream->formats;

		desc->interface_flags |= B_MULTI_INTERFACE_RECORD;
	}

	int32 channels = desc->output_channel_count + desc->input_channel_count;
	if (desc->request_channel_count >= channels) {
		create_multi_channel_info(info, desc->channels);
	}

	desc->max_cvsr_rate = 0;
	desc->min_cvsr_rate = 0;

	desc->lock_sources = B_MULTI_LOCK_INTERNAL;
	desc->timecode_sources = 0;

	desc->start_latency = 0;

	desc->control_panel[0] = '\0';

	return B_OK;
}


status_t
multi_get_enabled_channels(VirtIOSoundDriverInfo* info, void* buffer)
{
	multi_channel_enable* data = (multi_channel_enable*)buffer;
	uint32 channels =  0;
	
	if (info->inputStreams) {
		VirtIOSoundPCMInfo* stream = get_stream(info, VIRTIO_SND_D_INPUT);
		channels += stream->channels;
	}
	
	if (info->outputStreams) {
		VirtIOSoundPCMInfo* stream = get_stream(info, VIRTIO_SND_D_OUTPUT);
		channels += stream->channels;
	}

	for (uint32 i = 0; i < channels; i++)
		B_SET_CHANNEL(data->enable_bits, i, true);

	data->lock_source = B_MULTI_LOCK_INTERNAL;

	return B_OK;
}