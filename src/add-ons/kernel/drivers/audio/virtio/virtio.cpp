/*
 *  Copyright 2024, Diego Roux, diegoroux04 at proton dot me
 *  Distributed under the terms of the MIT License.
 */

#include <virtio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver.h"
#include "virtio_sound.h"

#define ROUND_TO_PAGE_SIZE(x) (x + B_PAGE_SIZE - 1) & (~(B_PAGE_SIZE - 1))


status_t
VirtIOSoundQueryInfo(VirtIOSoundDriverInfo* info, uint32 type,
	uint32 start_id, uint32 count, uint32 size, void* response)
{
	struct virtio_snd_query_info* query;
	query = (struct virtio_snd_query_info*)info->ctrlBuf;

	query->hdr.code = type;
	query->start_id = start_id;
	query->count = count;
	query->size = size;

	uint32 responseSize = count * size;
	memset((void*)(info->ctrlBuf + sizeof(struct virtio_snd_query_info)),
		0x00, responseSize + sizeof(struct virtio_snd_hdr));

	if (!info->virtio->queue_is_empty(info->controlQueue))
		return B_ERROR;

	physical_entry entries[] = {
		{info->ctrlAddr, sizeof(struct virtio_snd_query_info)},
		{info->ctrlAddr + sizeof(struct virtio_snd_query_info), sizeof(struct virtio_snd_hdr)},
		{info->ctrlAddr + sizeof(struct virtio_snd_query_info) + sizeof(struct virtio_snd_hdr),
            responseSize},
	};

	status_t status = info->virtio->queue_request_v(info->controlQueue,
		entries, 1, 2, NULL);

	if (status != B_OK)
		return status;

	while (!info->virtio->queue_dequeue(info->controlQueue, NULL, NULL));

	struct virtio_snd_hdr* hdr =
		(struct virtio_snd_hdr*)(info->ctrlBuf + sizeof(struct virtio_snd_query_info));

	if (hdr->code != VIRTIO_SND_S_OK)
		return B_ERROR;

	memcpy(response,
		(void*)(info->ctrlBuf + sizeof(struct virtio_snd_query_info) +
            sizeof(struct virtio_snd_hdr)),
		responseSize);

	return B_OK;
}


status_t
VirtIOControlQueueInit(VirtIOSoundDriverInfo* info)
{
	info->ctrlArea = create_area("virtio_snd ctrl buffer", (void**)&info->ctrlBuf,
		B_ANY_KERNEL_BLOCK_ADDRESS, B_PAGE_SIZE, B_FULL_LOCK,
		B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA);

	status_t status = info->ctrlArea;
	if (status < 0) {
		ERROR("unable to create buffer area (%s)\n", strerror(status));
		return status;
	}

	physical_entry entry;
	status = get_memory_map((void*)info->ctrlBuf, B_PAGE_SIZE, &entry, 1);
	if (status != B_OK) {
		ERROR("unable to get memory map (%s)\n", strerror(status));
		return status;
	}

	info->ctrlAddr = entry.address;

	status = info->virtio->queue_setup_interrupt(info->controlQueue, NULL, info);
	if (status != B_OK) {
		ERROR("ctrl queue interrupt setup failed (%s)\n", strerror(status));
		delete_area(info->ctrlArea);
		return B_ERROR;
	}

	return B_OK;
}


status_t
VirtIOEventQueueInit(VirtIOSoundDriverInfo* info)
{
	info->eventArea = create_area("virtio_snd event buffer", (void**)&info->eventBuf,
		B_ANY_KERNEL_BLOCK_ADDRESS, B_PAGE_SIZE, B_FULL_LOCK,
		B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA);
	
	status_t status = info->eventArea;
	if (status < 0) {
		ERROR("unable to create buffer area (%s)\n", strerror(status));
		return status;
	}

	physical_entry entry;
	status = get_memory_map((void*)info->eventBuf, B_PAGE_SIZE, &entry, 1);
	if (status != B_OK) {
		ERROR("unable to get memory map (%s)\n", strerror(status));
		return status;
	}

	info->eventAddr = entry.address;

	memset((void*)info->eventBuf, 0x00, sizeof(struct virtio_snd_event) * 2);

	if (!info->virtio->queue_is_empty(info->controlQueue))
		return B_ERROR;

	physical_entry entries[] = {
		{info->eventAddr, sizeof(struct virtio_snd_event)},
		{info->eventAddr + sizeof(struct virtio_snd_event), sizeof(struct virtio_snd_event)}
	};

	status = info->virtio->queue_request_v(info->eventQueue, entries, 0, 2, NULL);
	if (status != B_OK)
		return status;

	status = info->virtio->queue_setup_interrupt(info->eventQueue, NULL, info);
	if (status != B_OK) {
		ERROR("event queue interrupt setup failed (%s)\n", strerror(status));
		delete_area(info->eventArea);

		return status;
	}

	return B_OK;
}


status_t
VirtIOSoundPCMControlRequest(VirtIOSoundDriverInfo* info, void* buffer, size_t size)
{
	if (!info->virtio->queue_is_empty(info->controlQueue))
		return B_ERROR;

	memcpy((void*)info->ctrlBuf, buffer, size);

	physical_entry entries[] = {
		{info->ctrlAddr, size}
	};

	status_t status = info->virtio->queue_request_v(info->controlQueue, entries, 1, 0, NULL);
	if (status != B_OK)
		return status;

	while (!info->virtio->queue_dequeue(info->controlQueue, NULL, NULL));

	return B_OK;
}


status_t
VirtIOSoundTXQueueInit(VirtIOSoundDriverInfo* info, VirtIOSoundPCMInfo* stream)
{
	uint32 tx_size = sizeof(struct virtio_snd_pcm_xfer) + (stream->period_size * 2)
		+ sizeof(struct virtio_snd_pcm_status);

	tx_size = ROUND_TO_PAGE_SIZE(tx_size);

	info->txArea = create_area("virtio_snd tx buffer", (void**)&info->txBuf,
		B_ANY_KERNEL_BLOCK_ADDRESS, tx_size, B_FULL_LOCK,
		B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA);

	status_t status = info->txArea;
	if (status < 0) {
		ERROR("unable to create tx buffer area (%s)\n", strerror(status));
		return status;
	}

	physical_entry entry;
	status = get_memory_map((void*)info->txBuf, tx_size, &entry, 1);
	if (status != B_OK) {
		ERROR("unable to get tx memory map (%s)\n", strerror(status));
		return status;
	}

	info->txAddr = entry.address;

	return B_OK;
}


status_t
VirtIOSoundRXQueueInit(VirtIOSoundDriverInfo* info, VirtIOSoundPCMInfo* stream)
{
	uint32 rx_size = sizeof(struct virtio_snd_pcm_xfer) + (stream->period_size * 2)
		+ sizeof(struct virtio_snd_pcm_status);

	rx_size = ROUND_TO_PAGE_SIZE(rx_size);

	info->rxArea = create_area("virtio_snd rx buffer", (void**)&info->rxBuf,
		B_ANY_KERNEL_BLOCK_ADDRESS, rx_size, B_FULL_LOCK,
		B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA);

	status_t status = info->rxArea;
	if (status < 0) {
		ERROR("unable to create rx buffer area (%s)\n", strerror(status));
		return status;
	}

	physical_entry entry;
	status = get_memory_map((void*)info->rxBuf, rx_size, &entry, 1);
	if (status != B_OK) {
		ERROR("unable to get rx memory map (%s)\n", strerror(status));
		return status;
	}

	info->rxAddr = entry.address;

	return B_OK;
}