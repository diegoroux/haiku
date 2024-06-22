/*
 *  Copyright 2024, Diego Roux, diegoroux04 at proton dot me
 *  Distributed under the terms of the MIT License.
 */

#include <hmulti_audio.h>

#include <string.h>

#include "virtio_sound.h"
#include "driver.h"


static VirtIOSoundPCMInfo*
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


static status_t
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
		VirtIOSoundPCMInfo* stream = get_stream(info, VIRTIO_SND_D_OUTPUT);

		desc->output_channel_count = stream->channels;

		desc->output_rates = stream->rates;
		desc->output_formats = stream->formats;

		desc->interface_flags |= B_MULTI_INTERFACE_PLAYBACK;
	}

	if (info->inputStreams) {
		VirtIOSoundPCMInfo* stream = get_stream(info, VIRTIO_SND_D_INPUT);
		
		desc->input_channel_count = stream->channels;

		desc->input_rates = stream->rates;
		desc->input_formats = stream->formats;

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


static status_t
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


static status_t
multi_get_global_format(VirtIOSoundDriverInfo* info, void* buffer)
{
	multi_format_info* data = (multi_format_info*)buffer;

	memset(buffer, 0x00, sizeof(multi_format_info));

	data->info_size = sizeof(multi_format_info);

	if (info->inputStreams) {
		VirtIOSoundPCMInfo* stream = get_stream(info, VIRTIO_SND_D_INPUT);

		data->input.format = stream->format;
		data->input.rate = stream->rate;
	}
	
	if (info->outputStreams) {
		VirtIOSoundPCMInfo* stream = get_stream(info, VIRTIO_SND_D_OUTPUT);

		data->output.format = stream->format;
		data->output.rate = stream->rate;
	}

	return B_OK;
}


static status_t
multi_set_global_format(VirtIOSoundDriverInfo* info, void* buffer)
{
	multi_format_info* data = (multi_format_info*)buffer;

	for (uint32 i = 0; i < 2; i++) {
		VirtIOSoundPCMInfo* stream = get_stream(info, i);
		if (!stream)
			continue;

		_multi_format *request;
		switch (i) {
			case VIRTIO_SND_D_OUTPUT:
				request = &data->input;
				break;
			case VIRTIO_SND_D_INPUT:
				request = &data->output;
				break;
		}

		if (!(stream->formats & request->format)) {
			ERROR("unsupported format requested (%u)\n", request->format);
			return B_BAD_VALUE;
		}

		stream->format = request->format;

		if (!(stream->rates & request->rate)) {
			ERROR("unsupported rate requested (%u)\n", request->rate);
			return B_BAD_VALUE;
		}

		stream->rate = request->rate;

		// TODO: Determine buffer & period size.
		status_t status = VirtIOSoundPCMSetParams(info, stream->stream_id, 0, 0);
		if (status != B_OK)
			return status;
	}

	return B_OK;
}


static status_t
multi_get_buffers(VirtIOSoundDriverInfo* info, void* buffer)
{
	return B_ERROR; // B_OK;
}


status_t
virtio_snd_ctrl(void* cookie, uint32 op, void* buffer, size_t length)
{
	VirtIOSoundDriverInfo* info = (VirtIOSoundDriverInfo*)cookie;

	DEBUG("op: %u\n", op);

	switch (op) {
		case B_MULTI_GET_DESCRIPTION: 			return multi_get_description(info, buffer);
		case B_MULTI_GET_EVENT_INFO:			return B_ERROR;
		case B_MULTI_SET_EVENT_INFO:			return B_ERROR;
		case B_MULTI_GET_EVENT:					return B_ERROR;
		case B_MULTI_GET_ENABLED_CHANNELS:		return multi_get_enabled_channels(info, buffer);
		case B_MULTI_SET_ENABLED_CHANNELS:		return B_OK;
		case B_MULTI_GET_GLOBAL_FORMAT:			return multi_get_global_format(info, buffer);
		case B_MULTI_SET_GLOBAL_FORMAT:			return multi_set_global_format(info, buffer);
		case B_MULTI_GET_CHANNEL_FORMATS:		return B_ERROR;
		case B_MULTI_SET_CHANNEL_FORMATS:		return B_ERROR;
		case B_MULTI_GET_MIX:					return B_ERROR;
		case B_MULTI_SET_MIX:					return B_ERROR;
		case B_MULTI_LIST_MIX_CHANNELS:			return B_ERROR;
		case B_MULTI_LIST_MIX_CONTROLS:			return B_ERROR;
		case B_MULTI_LIST_MIX_CONNECTIONS:		return B_ERROR;
		case B_MULTI_GET_BUFFERS:				return multi_get_buffers(info, buffer);
		case B_MULTI_SET_BUFFERS:				return B_ERROR;
		case B_MULTI_SET_START_TIME:			return B_ERROR;
		case B_MULTI_BUFFER_EXCHANGE:			return B_ERROR;
		case B_MULTI_BUFFER_FORCE_STOP:			return B_ERROR;
		case B_MULTI_LIST_EXTENSIONS:			return B_ERROR;
		case B_MULTI_GET_EXTENSION:				return B_ERROR;
		case B_MULTI_SET_EXTENSION:				return B_ERROR;
		case B_MULTI_LIST_MODES:				return B_ERROR;
		case B_MULTI_GET_MODE:					return B_ERROR;
		case B_MULTI_SET_MODE:					return B_ERROR;
	}

	return B_BAD_VALUE;
}