/*
 *  Copyright 2024, Diego Roux, diegoroux04 at proton dot me
 *  Distributed under the terms of the MIT License.
 */
#ifndef _VIRTIO_SND_DRIVER_H
#define _VIRTIO_SND_DRIVER_H


#include <fs/devfs.h>

#include <stdlib.h>
#include <hmulti_audio.h>
#include <virtio.h>

#define ERROR(x...)		dprintf("\33[33mvirtio_sound:\33[0m " x)
#define LOG(x...)		dprintf("virtio_sound: " x)

#ifdef _VIRTIO_SND_DEBUG
#define DEBUG(x...)		dprintf("\33[36mvirtio_sound:\33[0m " __func__ ": " x)
#else
#define DEBUG(x...)		;
#endif

#define VIRTIO_SND_CHMAP_MAX_SIZE	18


struct VirtIOSoundPCMInfo {
	uint32						stream_id;

	uint32						nid;

	uint32						features;
	uint32						formats;
	uint32						rates;

	uint32						format;
	uint32						rate;

	uint32 						period_size;

	uint8						current_state;

	uint8						direction;

	uint8						channels;
	uint8						channels_min;
	uint8						channels_max;

	uint8						chmap[VIRTIO_SND_CHMAP_MAX_SIZE];
};


struct VirtIOSoundDriverInfo {
	device_node* 				node;
	::virtio_device 			virtioDev;
	virtio_device_interface*	virtio;

	uint64						features;

	::virtio_queue				controlQueue;
	::virtio_queue				eventQueue;
	::virtio_queue				txQueue;
	::virtio_queue				rxQueue;

	uint32						nJacks;
	uint32						nStreams;
	uint32						nChmaps;

	VirtIOSoundPCMInfo*			streams;
	uint32						inputStreams;
	uint32						outputStreams;

	area_id						ctrlArea;
	addr_t						ctrlBuf;
	phys_addr_t					ctrlAddr;

	area_id						eventArea;
	addr_t						eventBuf;
	phys_addr_t					eventAddr;
};

status_t
virtio_snd_ctrl(void* cookie, uint32 op, void* buffer, size_t length);

status_t
VirtIOSoundQueryInfo(VirtIOSoundDriverInfo* info, uint32 type,
	uint32 start_id, uint32 count, uint32 size, void* response);

status_t
VirtIOControlQueueInit(VirtIOSoundDriverInfo* info);

status_t
VirtIOEventQueueInit(VirtIOSoundDriverInfo* info);

status_t
VirtIOSoundQueryStreamInfo(VirtIOSoundDriverInfo* info);

status_t
VirtIOSoundQueryChmapsInfo(VirtIOSoundDriverInfo* info);

status_t
VirtIOSoundPCMControlRequest(VirtIOSoundDriverInfo* info, void* buffer, size_t size);

status_t
VirtIOSoundPCMSetParams(VirtIOSoundDriverInfo* info, uint32 stream_id,
	uint32 buffer, uint32 period);

#endif