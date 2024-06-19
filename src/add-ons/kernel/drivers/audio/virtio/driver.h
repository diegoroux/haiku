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
#define DEBUG(x...)		dprintf("\33[36mvirtio_sound:\33[0m " x)
#endif

#define VIRTIO_SND_CHMAP_MAX_SIZE	18

struct VirtIOSoundPCMInfo {
	uint32						stream_id;

	uint32						nid;

	uint32						features;
	uint32						formats;
	uint32						rates;

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
};

status_t
multi_get_description(VirtIOSoundDriverInfo* info, multi_description* desc);

status_t
VirtIOSoundQueryInfo(VirtIOSoundDriverInfo* info, uint32 type,
	uint32 start_id, uint32 count, uint32 size, void* response);

status_t
VirtIOSoundQueryStreamInfo(VirtIOSoundDriverInfo* info);

status_t
VirtIOSoundQueryChmapsInfo(VirtIOSoundDriverInfo* info);

#endif