/*
 *  Copyright 2024, Diego Roux, diegoroux04 at proton dot me
 *  Distributed under the terms of the MIT License.
 */

#include <hmulti_audio.h>

#include <string.h>

#include "virtio_sound.h"
#include "driver.h"

#define VIRTIO_MULTI_CONTROL_FIRST_ID	1024
#define VIRTIO_MULTI_CONTROL_MASTER_ID	0


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

	strcpy(desc->friendly_name, "Virtio Sound Device");
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
	uint32 channels = 0;

	for (uint32 i = 0; i < 2; i++) {
		VirtIOSoundPCMInfo* stream = get_stream(info, i);
		if (!stream)
			continue;

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

	for (uint32 i = 0; i < 2; i++) {
		VirtIOSoundPCMInfo* stream = get_stream(info, i);
		if (!stream)
			continue;

		_multi_format* reply;
		switch (i) {
			case VIRTIO_SND_D_OUTPUT:
				reply = &data->output;
				break;
			case VIRTIO_SND_D_INPUT:
				reply = &data->input;
				break;
		}

		reply->format = stream->format;
		reply->rate = stream->rate;
	}

	return B_OK;
}


static uint8
format_to_size(uint32 format)
{
	switch (format) {
		case B_FMT_8BIT_S:
		case B_FMT_8BIT_U:
			return 1;
		case B_FMT_16BIT:
			return 2;
		case B_FMT_20BIT:
		case B_FMT_24BIT:
		case B_FMT_32BIT:
		case B_FMT_FLOAT:
			return 4;
		case B_FMT_DOUBLE:
			return 8;
		default:
			return 0;
	}
}


static status_t
multi_set_global_format(VirtIOSoundDriverInfo* info, void* buffer)
{
	multi_format_info* data = (multi_format_info*)buffer;

	for (uint32 i = 0; i < 2; i++) {
		VirtIOSoundPCMInfo* stream = get_stream(info, i);
		if (!stream)
			continue;

		_multi_format* request;
		switch (i) {
			case VIRTIO_SND_D_OUTPUT:
				request = &data->output;
				break;
			case VIRTIO_SND_D_INPUT:
				request = &data->input;
				break;
		}

		if (!(stream->formats & request->format)) {
			ERROR("unsupported format requested (%u)\n", request->format);
			return B_BAD_VALUE;
		}

		if (!(stream->rates & request->rate)) {
			ERROR("unsupported rate requested (%u)\n", request->rate);
			return B_BAD_VALUE;
		}

		stream->format = request->format;
		stream->rate = request->rate;

		stream->period_size = stream->channels * format_to_size(stream->format)
			* FRAMES_PER_BUFFER;

		if (stream->current_state == VIRTIO_SND_STATE_STOP)
			VirtIOSoundPCMRelease(info, stream);

		status_t status = VirtIOSoundPCMSetParams(info, stream,
			stream->period_size * BUFFERS, stream->period_size);

		if (status != B_OK) {
			ERROR("set params failed (%s)\n", strerror(status));
			return status;
		}
	}

	return B_OK;
}


static status_t
multi_get_buffers(VirtIOSoundDriverInfo* info, void* buffer)
{
	multi_buffer_list* data = (multi_buffer_list*)buffer;

	data->flags = 0x00;

	for (uint32 i = 0; i < 2; i++) {
		VirtIOSoundPCMInfo* stream = get_stream(info, i);
		if (!stream)
			continue;

		buffer_desc** buffers;
		status_t status;
		char* buf_ptr;

		switch (i) {
			case VIRTIO_SND_D_OUTPUT: {
				status = VirtIOSoundTXQueueInit(info, stream);

				data->flags |= B_MULTI_BUFFER_PLAYBACK;

				data->return_playback_buffers = BUFFERS;
				data->return_playback_channels = stream->channels;
				data->return_playback_buffer_size = FRAMES_PER_BUFFER;

				buffers = data->playback_buffers;

				buf_ptr = (char*)info->txBuf;
				break;
			}
			case VIRTIO_SND_D_INPUT: {
				status = VirtIOSoundRXQueueInit(info, stream);

				data->flags |= B_MULTI_BUFFER_RECORD;

				data->return_record_buffers = BUFFERS;
				data->return_record_channels = stream->channels;
				data->return_record_buffer_size = FRAMES_PER_BUFFER;

				buffers = data->record_buffers;

				buf_ptr = (char*)info->rxBuf;
				break;
			}
		}

		if (status != B_OK)
			return status;

		// Consider the header size.
		buf_ptr += sizeof(struct virtio_snd_pcm_xfer);

		uint32 format_size = format_to_size(stream->format);

		for (uint32 buf_id = 0; buf_id < BUFFERS; buf_id++) {
			for (uint32 ch_id = 0; ch_id < stream->channels; ch_id++) {
				buffers[buf_id][ch_id].base = buf_ptr + (format_size * ch_id);
				buffers[buf_id][ch_id].stride = format_size * stream->channels;
			}

			buf_ptr += stream->period_size;
		}

		status = VirtIOSoundPCMPrepare(info, stream);
		if (status != B_OK)
			return status;
	}	

	return B_OK;
}


static status_t
multi_list_mix_channels(VirtIOSoundDriverInfo* info, void* buffer)
{
	return B_OK;
}


static status_t
multi_list_mix_controls(VirtIOSoundDriverInfo* info, void* buffer)
{
	multi_mix_control_info* data = (multi_mix_control_info*)buffer;

	uint8 idx = 0;
	for (uint32 i = 0; i < 2; i++) {
		VirtIOSoundPCMInfo* stream = get_stream(info, i);
		if (!stream)
			continue;

		multi_mix_control* controls = &data->controls[idx];

		controls->id = VIRTIO_MULTI_CONTROL_FIRST_ID + idx;
		controls->parent = 0;
		controls->flags = B_MULTI_MIX_GROUP;
		controls->master = VIRTIO_MULTI_CONTROL_MASTER_ID;
		controls->string = S_null;
		
		switch (i) {
			case VIRTIO_SND_D_OUTPUT:
				strcpy(controls->name, "Playback");
				break;
			case VIRTIO_SND_D_INPUT:
				strcpy(controls->name, "Record");
				break;
		}

		idx++;
	}

	data->control_count = 0;

	return B_OK;
}


status_t
multi_list_mix_connections(VirtIOSoundDriverInfo* info, void* buffer)
{
	multi_mix_connection_info* data = (multi_mix_connection_info*)buffer;

	data->actual_count = 0;

	return B_OK;
}


static status_t
start_playback_stream(VirtIOSoundDriverInfo* info, VirtIOSoundPCMInfo* stream)
{
	status_t status = VirtIOSoundPCMStart(info, stream);
	if (status != B_OK)
		return status;

	stream->buffer_cycle = 0;
	stream->real_time = 0;
	stream->frames_count = 0;

	stream->entries[0].address = info->txAddr;
	stream->entries[0].size = sizeof(struct virtio_snd_pcm_xfer);

	struct virtio_snd_pcm_xfer* xfer = (struct virtio_snd_pcm_xfer*)info->txBuf;

	xfer->stream_id = stream->stream_id;

	for (uint8 buf_id = 0; buf_id < BUFFERS; buf_id++) {
		stream->entries[1 + buf_id].address = info->txAddr + sizeof(struct virtio_snd_pcm_xfer)
			+ (stream->period_size * buf_id);

		stream->entries[1 + buf_id].size = stream->period_size;
	}

	stream->entries[BUFFERS + 1].address = info->txAddr + sizeof(struct virtio_snd_pcm_xfer)
		+ (stream->period_size * BUFFERS);
	stream->entries[BUFFERS + 1].size = sizeof(struct virtio_snd_pcm_status);

	return B_OK;
}


static status_t
send_playback_buffer(VirtIOSoundDriverInfo* info, VirtIOSoundPCMInfo* stream)
{
	if (!info->virtio->queue_is_empty(info->txQueue)) {
		DEBUG("%s", "queue is not empty\n");
		return B_ERROR;
	}

	if (stream->buffer_cycle != (BUFFERS - 1)) {
		stream->buffer_cycle = (stream->buffer_cycle + 1) % BUFFERS;
		stream->real_time = system_time();
		stream->frames_count += FRAMES_PER_BUFFER;

		release_sem_etc(info->txSem, 1, B_DO_NOT_RESCHEDULE);

		return B_OK;
	}

	status_t status = info->virtio->queue_request_v(info->txQueue, stream->entries,
		BUFFERS + 1, 1, NULL);

	if (status != B_OK) {
		DEBUG("%s", "queue request failed\n");
		return status;
	}

	while (!info->virtio->queue_dequeue(info->txQueue, NULL, NULL));

	struct virtio_snd_pcm_status* hdr = (struct virtio_snd_pcm_status*)(info->txBuf
		+ sizeof(struct virtio_snd_pcm_xfer) + (stream->period_size * BUFFERS));

	if (hdr->status != VIRTIO_SND_S_OK)
		return B_ERROR;

	stream->buffer_cycle = (stream->buffer_cycle + 1) % BUFFERS;
	stream->real_time = system_time();
	stream->frames_count += FRAMES_PER_BUFFER;

	return B_OK;
}


static status_t
multi_buffer_exchange(VirtIOSoundDriverInfo* info, void* buffer)
{
	multi_buffer_info* data = (multi_buffer_info*)buffer;

	VirtIOSoundPCMInfo* stream = get_stream(info, VIRTIO_SND_D_OUTPUT);
	if (!stream)
		return B_ERROR;

	if (stream->current_state != VIRTIO_SND_STATE_START)
		return start_playback_stream(info, stream);

	acquire_sem(info->txSem);
	
	status_t status = send_playback_buffer(info, stream);
	if (status != B_OK) {
		ERROR("playback failed (%s)\n", strerror(status));
		return status;
	}

	data->playback_buffer_cycle = stream->buffer_cycle;
	data->played_real_time = stream->real_time;
	data->played_frames_count = stream->frames_count;

	return B_OK;
}


static status_t
multi_buffer_stop(VirtIOSoundDriverInfo* info)
{
	VirtIOSoundPCMInfo* stream = get_stream(info, VIRTIO_SND_D_OUTPUT);
	if (!stream)
		return B_ERROR;

	if (stream->current_state == VIRTIO_SND_STATE_START) {
		status_t status = VirtIOSoundPCMStop(info, get_stream(info, VIRTIO_SND_D_OUTPUT));

		if (status != B_OK)
			return status;
	}

	delete_area(info->txArea);
	delete_area(info->rxArea);

	info->txBuf = (addr_t)NULL;
	info->rxBuf = (addr_t)NULL;

	delete_sem(info->txSem);

	return B_OK;
}


status_t
virtio_snd_ctrl(void* cookie, uint32 op, void* buffer, size_t length)
{
	VirtIOSoundDriverInfo* info = (VirtIOSoundDriverInfo*)cookie;

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
		case B_MULTI_LIST_MIX_CHANNELS:			return multi_list_mix_channels(info, buffer);
		case B_MULTI_LIST_MIX_CONTROLS:			return multi_list_mix_controls(info, buffer);
		case B_MULTI_LIST_MIX_CONNECTIONS:		return multi_list_mix_connections(info, buffer);
		case B_MULTI_GET_BUFFERS:				return multi_get_buffers(info, buffer);
		case B_MULTI_SET_BUFFERS:				return B_ERROR;
		case B_MULTI_SET_START_TIME:			return B_ERROR;
		case B_MULTI_BUFFER_EXCHANGE:			return multi_buffer_exchange(info, buffer);
		case B_MULTI_BUFFER_FORCE_STOP:			return multi_buffer_stop(info);
		case B_MULTI_LIST_EXTENSIONS:			return B_ERROR;
		case B_MULTI_GET_EXTENSION:				return B_ERROR;
		case B_MULTI_SET_EXTENSION:				return B_ERROR;
		case B_MULTI_LIST_MODES:				return B_ERROR;
		case B_MULTI_GET_MODE:					return B_ERROR;
		case B_MULTI_SET_MODE:					return B_ERROR;
	}

	return B_BAD_VALUE;
}