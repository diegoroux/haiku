/*
 *  Copyright 2024, Diego Roux, diegoroux04 at proton dot me
 *  Distributed under the terms of the MIT License.
 */

#include <fs/devfs.h>
#include <virtio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hmulti_audio.h>

#include "driver.h"
#include "virtio_sound.h"

#define VIRTIO_SOUND_DRIVER_MODULE_NAME 	"drivers/audio/hmulti/virtio_sound/driver_v1"
#define VIRTIO_SOUND_DEVICE_MODULE_NAME 	"drivers/audio/hmulti/virtio_sound/device_v1"
#define VIRTIO_SOUND_DEVICE_ID_GEN 			"virtio_sound/device_id"


static device_manager_info*		sDeviceManager;


static const char*
get_feature_name(uint64_t feature)
{
	return NULL;
}


// #pragma mark - driver module API


static float
virtio_snd_supports_device(device_node* parent)
{
	uint16 deviceType;
	const char* bus;

	if (sDeviceManager->get_attr_string(parent, B_DEVICE_BUS, &bus, false) != B_OK
		|| sDeviceManager->get_attr_uint16(parent, VIRTIO_DEVICE_TYPE_ITEM,
			&deviceType, true) != B_OK) {
		return 0.0f;
	}

	if (strcmp(bus, "virtio") != 0)
		return 0.0f;

	if (deviceType != VIRTIO_DEVICE_ID_SOUND)
		return 0.0f;

	LOG("VirtIO Sound Device found!\n");

	return 0.6f;
}


static status_t
virtio_snd_register_dev(device_node* node)
{
	device_attr attrs[] = {
		{B_DEVICE_PRETTY_NAME, B_STRING_TYPE, {.string = "VirtIO Sound Device"}},
		{}
	};

	return sDeviceManager->register_node(node, VIRTIO_SOUND_DRIVER_MODULE_NAME,
		attrs, NULL, NULL);
}


static status_t
virtio_snd_register_child_dev(void* _cookie)
{
	VirtIOSoundDriverInfo* info = (VirtIOSoundDriverInfo*)_cookie;
	int32 id;

	id = sDeviceManager->create_id(VIRTIO_SOUND_DEVICE_ID_GEN);
	if (id < 0)
		return id;

	char path[64];
	snprintf(path, sizeof(path), "audio/hmulti/virtio/%" B_PRId32, id);

	status_t status = sDeviceManager->publish_device(info->node,
		path, VIRTIO_SOUND_DEVICE_MODULE_NAME);

	return status;
}


static status_t
virtio_snd_init_driver(device_node* node, void** cookie)
{
	VirtIOSoundDriverInfo* info;
	
	info = (VirtIOSoundDriverInfo*)malloc(sizeof(VirtIOSoundDriverInfo));

	if (info == NULL)
		return B_NO_MEMORY;

	memset((void*)info, 0x00, sizeof(VirtIOSoundDriverInfo));

	info->node = node;
	*cookie = info;

	return B_OK;
}


static void
virtio_snd_uninit_driver(void* cookie)
{
	free(cookie);
}


// #pragma mark - Device module API


static void
ctrl_queue_done(void* _cookie, void* __cookie)
{
	VirtIOSoundDriverInfo* info = (VirtIOSoundDriverInfo*)__cookie;

	release_sem_etc(info->ctrlSem, 1, B_DO_NOT_RESCHEDULE);
}


static void
tx_queue_done(void* _cookie, void* __cookie)
{
	VirtIOSoundDriverInfo* info = (VirtIOSoundDriverInfo*)__cookie;

	release_sem_etc(info->txSem, 1, B_DO_NOT_RESCHEDULE);
}


static void
rx_queue_done(void* _cookie, void* __cookie)
{
	VirtIOSoundDriverInfo* info = (VirtIOSoundDriverInfo*)__cookie;

	release_sem_etc(info->rxSem, 1, B_DO_NOT_RESCHEDULE);
}


static status_t
virtio_snd_init_device(void* _info, void** cookie)
{
	VirtIOSoundDriverInfo* info = (VirtIOSoundDriverInfo*)_info;

	device_node *parent = sDeviceManager->get_parent_node(info->node);
	
	sDeviceManager->get_driver(parent, (driver_module_info**)&info->virtio,
		(void**)&info->virtioDev);
	
	sDeviceManager->put_node(parent);

	info->virtio->negotiate_features(info->virtioDev, 0,
		&info->features, &get_feature_name);

	::virtio_queue queues[4];
	status_t status = info->virtio->alloc_queues(info->virtioDev, 4, queues);
	if (status != B_OK) {
		ERROR("queue allocation failed (%s)\n", strerror(status));
		return status;
	}

	info->controlQueue = queues[0];
	info->eventQueue = queues[1];
	info->txQueue = queues[2];
	info->rxQueue = queues[3];

	status = info->virtio->setup_interrupt(info->virtioDev, NULL, info);
	if (status != B_OK) {
		ERROR("interrupt setup failed (%s)\n", strerror(status));
		goto err1;
	}

	status = info->virtio->queue_setup_interrupt(info->controlQueue,
		ctrl_queue_done, info);
	if (status != B_OK) {
		ERROR("ctrl queue interrupt setup failed (%s)\n", strerror(status));
		goto err1;
	}

	status = info->virtio->queue_setup_interrupt(info->txQueue, tx_queue_done, info);
	if (status != B_OK) {
		ERROR("tx queue interrupt setup failed (%s)\n", strerror(status));
		goto err1;
	}

	status = info->virtio->queue_setup_interrupt(info->rxQueue, rx_queue_done, info);
	if (status != B_OK) {
		ERROR("rx queue interrupt setup failed (%s)\n", strerror(status));
		goto err1;
	}

	status = info->virtio->queue_setup_interrupt(info->eventQueue, NULL, info);
	if (status != B_OK) {
		ERROR("event queue interrupt setup failed (%s)\n", strerror(status));
		goto err1;
	}

	status = VirtIOControlQueueInit(info);
	if (status != B_OK) {
		ERROR("unable to initialize the control queue\n");
		goto err1;
	}

	status = info->virtio->read_device_config(info->virtioDev,
		offsetof(struct virtio_snd_config, jacks), &info->nJacks,
		sizeof(uint32_t));
	
	if (status != B_OK) {
		ERROR("device config read failed (%s)\n", strerror(status));
		goto err2;
	}

	// TODO: Query about jack info.

	status = info->virtio->read_device_config(info->virtioDev,
		offsetof(struct virtio_snd_config, streams), &info->nStreams,
		sizeof(uint32_t));

	if (status != B_OK) {
		ERROR("device config read failed (%s)\n", strerror(status));
		goto err2;
	}

	if (info->nStreams == 0) {
		ERROR("no PCM streams found\n");
		goto err2;
	}

	status = VirtIOSoundQueryStreamInfo(info);
	if (status != B_OK) {
		ERROR("stream info query failed (%s)\n", strerror(status));
		goto err2;
	}

	status = info->virtio->read_device_config(info->virtioDev,
		offsetof(struct virtio_snd_config, chmaps), &info->nChmaps,
		sizeof(uint32_t));

	if (status != B_OK) {
		ERROR("device config read failed (%s)\n", strerror(status));
		goto err2;
	}

	status = VirtIOSoundQueryChmapsInfo(info);
	if (status != B_OK) {
		ERROR("stream info query failed (%s)\n", strerror(status));
		goto err2;
	}

	status = VirtIOEventQueueInit(info);
	if (status != B_OK) {
		ERROR("unable to initialize the event queue\n");
		goto err2;
	}

	*cookie = info;
	return B_OK;

err2:
	delete_area(info->ctrlArea);
err1:
	info->virtio->free_queues(info->virtioDev);
	return status;
}


static void
virtio_snd_uninit_device(void *_info)
{
	VirtIOSoundDriverInfo* info = (VirtIOSoundDriverInfo*)_info;

	info->virtio->free_queues(info->virtioDev);

	delete_area(info->ctrlArea);
	delete_area(info->eventArea);

	delete_area(info->txArea);
	delete_area(info->rxArea);

	for (uint32 i = 0; i < info->nStreams; i++) {
		VirtIOSoundPCMInfo* stream = &info->streams[i];

		if ((void*)stream->xferBuf != NULL)
			delete_area(stream->xferArea);
	}

	free(info->streams);

	return;
}


static status_t
virtio_snd_open(void *deviceCookie, const char *path, int openMode,
	void **_cookie)
{
	VirtIOSoundDriverInfo* info = (VirtIOSoundDriverInfo*)deviceCookie;

	if (info->opened)
		return B_BUSY;

	info->opened = true;

	*_cookie = deviceCookie;
	return B_OK;
}


static status_t
virtio_snd_read(void *cookie, off_t pos, void *buffer, size_t *_length)
{
	*_length = 0;
	return B_IO_ERROR;
}


static status_t
virtio_snd_write(void *cookie, off_t pos, const void *buffer, size_t *_length)
{
	*_length = 0;
	return B_IO_ERROR;
}


static status_t
virtio_snd_close(void* cookie)
{
	VirtIOSoundDriverInfo* info = (VirtIOSoundDriverInfo*)cookie;

	info->opened = false;

	for (uint32 i = 0; i < info->nStreams; i++) {
		VirtIOSoundPCMInfo* stream = &info->streams[i];

		if (stream->current_state == VIRTIO_SND_STATE_START) {
			status_t status = VirtIOSoundPCMStop(info, stream);

			if (status != B_OK)
				return status;

			status = VirtIOSoundPCMRelease(info, stream);
			if (status != B_OK)
				return status;
		}
	}

	return B_OK;
}


static status_t
virtio_snd_free_dev(void* cookie)
{
	return B_OK;
}


struct driver_module_info sVirtioSoundDriver = {
	{
		VIRTIO_SOUND_DRIVER_MODULE_NAME,
		0,
		NULL
	},

	.supports_device = virtio_snd_supports_device,
	.register_device = virtio_snd_register_dev,

	.init_driver = virtio_snd_init_driver,
	.uninit_driver = virtio_snd_uninit_driver,

	.register_child_devices = virtio_snd_register_child_dev,
};


struct device_module_info sVirtioSoundDevice = {
	{
		VIRTIO_SOUND_DEVICE_MODULE_NAME,
		0,
		NULL
	},

	.init_device = virtio_snd_init_device,
	.uninit_device = virtio_snd_uninit_device,

	.open = virtio_snd_open,
	.close = virtio_snd_close,

	.free = virtio_snd_free_dev,

	.read = virtio_snd_read,
	.write = virtio_snd_write,

	.control = virtio_snd_ctrl,
};


module_info* modules[] = {
	(module_info*)&sVirtioSoundDriver,
	(module_info*)&sVirtioSoundDevice,
	NULL
};


module_dependency module_dependencies[] = {
	{
		B_DEVICE_MANAGER_MODULE_NAME,
		(module_info**)&sDeviceManager
	},
	{}
};
