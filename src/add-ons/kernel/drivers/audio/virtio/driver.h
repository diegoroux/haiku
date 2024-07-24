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
#define DEBUG(x, y...)		dprintf("\33[36mvirtio_sound:\33[0m %s: " x, __func__, y)
#else
#define DEBUG(x...)		;
#endif

#define VIRTIO_SND_CHMAP_MAX_SIZE	18

#define FRAMES_PER_BUFFER	(1024 * 16)
#define BUFFERS				2


struct VirtIOSoundPCMInfo {
	uint32						stream_id;

	uint32						nid;

	uint32						features;
	uint32						formats;
	uint32						rates;

	uint32						format;
	uint32						rate;
	uint32						cvsr;

	uint32 						period_size;

	uint8						current_state;

	uint8						direction;

	uint8						channels;
	uint8						channels_min;
	uint8						channels_max;

	uint32						buffer_cycle;
	uint32						real_time;
	uint32						frames_count;

	uint8						chmap[VIRTIO_SND_CHMAP_MAX_SIZE];

	area_id						xferArea;
	addr_t						xferBuf;
	phys_addr_t					xferAddr;
	spinlock					xferLock;
};


struct VirtIOSoundDriverInfo {
	device_node* 				node;
	::virtio_device 			virtioDev;
	virtio_device_interface*	virtio;

	bool						opened;

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
	sem_id 						ctrlSem;

	area_id						eventArea;
	addr_t						eventBuf;
	phys_addr_t					eventAddr;

	area_id						txArea;
	addr_t						txBuf;
	phys_addr_t					txAddr;
	sem_id						txSem;
	thread_id					txThread;

	area_id						rxArea;
	addr_t						rxBuf;
	phys_addr_t					rxAddr;
	sem_id						rxSem;
	thread_id					rxThread;
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
VirtIOSoundPCMSetParams(VirtIOSoundDriverInfo* info, VirtIOSoundPCMInfo* stream,
	uint32 buffer, uint32 period);

status_t
VirtIOSoundPCMPrepare(VirtIOSoundDriverInfo* info, VirtIOSoundPCMInfo* stream);

status_t
VirtIOSoundPCMStart(VirtIOSoundDriverInfo* info, VirtIOSoundPCMInfo* stream);

status_t
VirtIOSoundPCMStop(VirtIOSoundDriverInfo* info, VirtIOSoundPCMInfo* stream);

status_t
VirtIOSoundPCMRelease(VirtIOSoundDriverInfo* info, VirtIOSoundPCMInfo* stream);

status_t
VirtIOSoundTXQueueInit(VirtIOSoundDriverInfo* info, VirtIOSoundPCMInfo* stream);

status_t
VirtIOSoundRXQueueInit(VirtIOSoundDriverInfo* info, VirtIOSoundPCMInfo* stream);

#endif