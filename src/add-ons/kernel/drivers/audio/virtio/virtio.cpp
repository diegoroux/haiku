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