/*
 *  Copyright 2024, Diego Roux, diegoroux04 at proton dot me
 *  Distributed under the terms of the MIT License.
 */

#include <hmulti_audio.h>
#include <KernelExport.h>
#include <kernel.h>

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
get_description(VirtIOSoundDriverInfo* info, multi_description* desc)
{
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

	for (uint32 i = 0; i < 2; i++) {
		VirtIOSoundPCMInfo* stream = get_stream(info, i);
		if (stream == NULL)
			continue;

		switch (i) {
			case VIRTIO_SND_D_OUTPUT:
				desc->output_channel_count = stream->channels;
				
				desc->output_rates = stream->rates;
				desc->output_formats = stream->formats;
				
				desc->interface_flags |= B_MULTI_INTERFACE_PLAYBACK;
				break;
			case VIRTIO_SND_D_INPUT:
				desc->input_channel_count = stream->channels;
				
				desc->input_rates = stream->rates;
				desc->input_formats = stream->formats;
				
				desc->interface_flags |= B_MULTI_INTERFACE_RECORD;
				break;
		}
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
get_enabled_channels(VirtIOSoundDriverInfo* info, multi_channel_enable* data)
{
	uint32 channels = 0;

	for (uint32 i = 0; i < 2; i++) {
		VirtIOSoundPCMInfo* stream = get_stream(info, i);
		if (stream == NULL)
			continue;

		channels += stream->channels;
	}

	for (uint32 i = 0; i < channels; i++)
		B_SET_CHANNEL(data->enable_bits, i, true);

	data->lock_source = B_MULTI_LOCK_INTERNAL;

	return B_OK;
}


static status_t
get_global_format(VirtIOSoundDriverInfo* info, multi_format_info* data)
{
	memset((void*)data, 0x00, sizeof(multi_format_info));

	data->info_size = sizeof(multi_format_info);

	for (uint32 i = 0; i < 2; i++) {
		VirtIOSoundPCMInfo* stream = get_stream(info, i);
		if (stream == NULL)
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
set_global_format(VirtIOSoundDriverInfo* info, multi_format_info* data)
{
	for (uint32 i = 0; i < 2; i++) {
		VirtIOSoundPCMInfo* stream = get_stream(info, i);
		if (stream == NULL)
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
			stream->period_size, stream->period_size);

		if (status != B_OK) {
			ERROR("set params failed (%s)\n", strerror(status));
			return status;
		}
	}

	return B_OK;
}


static status_t
list_mix_channels(VirtIOSoundDriverInfo* info, multi_mix_channel_info* data)
{
	return B_OK;
}


static status_t
list_mix_controls(VirtIOSoundDriverInfo* info, multi_mix_control_info* data)
{
	uint8 idx = 0;
	for (uint32 i = 0; i < 2; i++) {
		VirtIOSoundPCMInfo* stream = get_stream(info, i);
		if (stream == NULL)
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


static status_t
list_mix_connections(VirtIOSoundDriverInfo* info, multi_mix_connection_info* data)
{
	data->actual_count = 0;

	return B_OK;
}


static status_t
get_mix(VirtIOSoundDriverInfo* info, multi_mix_value_info* data)
{
	return B_ERROR;
}


static status_t
set_mix(VirtIOSoundDriverInfo* info, multi_mix_value_info* data)
{
	return B_ERROR;
}


static status_t
get_buffers(VirtIOSoundDriverInfo* info, multi_buffer_list* data)
{
	data->flags = 0x00;

	for (uint32 i = 0; i < 2; i++) {
		VirtIOSoundPCMInfo* stream = get_stream(info, i);
		if (stream == NULL)
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
			if (!IS_USER_ADDRESS(buffers[buf_id]))
				return B_BAD_ADDRESS;

			struct buffer_desc buf_desc[stream->channels];

			for (uint32 ch_id = 0; ch_id < stream->channels; ch_id++) {
				buf_desc[ch_id].base = buf_ptr + (format_size * ch_id);
				buf_desc[ch_id].stride = format_size * stream->channels;
			}

			status = user_memcpy(buffers[buf_id], buf_desc,
				sizeof(struct buffer_desc) * stream->channels);

			if (status < B_OK)
				return B_BAD_ADDRESS;

			buf_ptr += stream->period_size;
		}

		status = VirtIOSoundPCMPrepare(info, stream);
		if (status != B_OK)
			return status;
	}	

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

	struct virtio_snd_pcm_xfer xfer;
	xfer.stream_id = stream->stream_id;

	status = user_memcpy((void*)info->txBuf, (void*)&xfer, sizeof(struct virtio_snd_pcm_xfer));
	if (status < B_OK)
		return status;

	stream->entries[1].size = stream->period_size;

	stream->entries[2].address = info->txAddr + sizeof(struct virtio_snd_pcm_xfer)
		+ (stream->period_size * BUFFERS);
	stream->entries[2].size = sizeof(struct virtio_snd_pcm_status);

	return B_OK;
}


static status_t
send_playback_buffer(VirtIOSoundDriverInfo* info, VirtIOSoundPCMInfo* stream)
{
	if (!info->virtio->queue_is_empty(info->txQueue)) {
		DEBUG("%s", "queue is not empty\n");
		return B_ERROR;
	}

	stream->entries[1].address = info->txAddr + sizeof(struct virtio_snd_pcm_xfer)
		+ (stream->period_size * stream->buffer_cycle);

	status_t status = info->virtio->queue_request_v(info->txQueue, stream->entries,
		2, 1, NULL);

	if (status != B_OK) {
		DEBUG("%s", "queue request failed\n");
		return status;
	}

	while (!info->virtio->queue_dequeue(info->txQueue, NULL, NULL));

	struct virtio_snd_pcm_status hdr;
	status = user_memcpy(&hdr,
		(void*)(info->txBuf + sizeof(struct virtio_snd_pcm_xfer)
			+ (stream->period_size * BUFFERS)),
		sizeof(struct virtio_snd_pcm_status));

	if (status < B_OK)
		return status;

	if (hdr.status != VIRTIO_SND_S_OK)
		return B_ERROR;

	stream->buffer_cycle = (stream->buffer_cycle + 1) % BUFFERS;
	stream->real_time = system_time();
	stream->frames_count += FRAMES_PER_BUFFER;

	return B_OK;
}


static status_t
buffer_exchange(VirtIOSoundDriverInfo* info, multi_buffer_info* data)
{
	VirtIOSoundPCMInfo* stream = get_stream(info, VIRTIO_SND_D_OUTPUT);
	if (stream == NULL)
		return B_ERROR;

	if (stream->current_state != VIRTIO_SND_STATE_START)
		return start_playback_stream(info, stream);

	if (!IS_USER_ADDRESS(data))
		return B_BAD_ADDRESS;

	multi_buffer_info buf_info;

	status_t status = user_memcpy(&buf_info, data, sizeof(multi_buffer_info));
	if (status < B_OK)
		return B_BAD_ADDRESS;

	acquire_sem(info->txSem);
	
	status = send_playback_buffer(info, stream);
	if (status != B_OK) {
		ERROR("playback failed (%s)\n", strerror(status));
		return status;
	}

	buf_info.playback_buffer_cycle = stream->buffer_cycle;
	buf_info.played_real_time = stream->real_time;
	buf_info.played_frames_count = stream->frames_count;

	status = user_memcpy(data, &buf_info, sizeof(multi_buffer_info));
	if (status < B_OK)
		return B_BAD_ADDRESS;

	return B_OK;
}


static status_t
buffer_force_stop(VirtIOSoundDriverInfo* info)
{
	VirtIOSoundPCMInfo* stream = get_stream(info, VIRTIO_SND_D_OUTPUT);
	if (stream == NULL)
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


#define cookie_type VirtIOSoundDriverInfo
#include "../generic/multi.c"

status_t
virtio_snd_ctrl(void* cookie, uint32 op, void* buffer, size_t length)
{
	VirtIOSoundDriverInfo* info = (VirtIOSoundDriverInfo*)cookie;

	return multi_audio_control_generic(info, op, buffer, length);
}