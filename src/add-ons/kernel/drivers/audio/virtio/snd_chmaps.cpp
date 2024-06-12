/*
 *  Copyright 2024, Diego Roux, diegoroux04 at proton dot me
 *  Distributed under the terms of the MIT License.
 */

#include <virtio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hmulti_audio.h>

#include "driver.h"
#include "virtio_sound.h"

#define B_CHANNEL_NA	0x00

static const uint32 supportedChmaps[] = {
	B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_NONE 
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_NA
    B_CHANNEL_MONO_BUS,				// VIRTIO_SND_CHMAP_MONO
    B_CHANNEL_LEFT,					// VIRTIO_SND_CHMAP_FL
    B_CHANNEL_RIGHT,				// VIRTIO_SND_CHMAP_FR
    B_CHANNEL_REARLEFT,				// VIRTIO_SND_CHMAP_RL
    B_CHANNEL_REARRIGHT,			// VIRTIO_SND_CHMAP_RR
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_FC
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_LFE 
    B_CHANNEL_SIDE_LEFT,			// VIRTIO_SND_CHMAP_SL
    B_CHANNEL_SIDE_RIGHT,			// VIRTIO_SND_CHMAP_SR
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_RC
    B_CHANNEL_FRONT_LEFT_CENTER,	// VIRTIO_SND_CHMAP_FLC
    B_CHANNEL_FRONT_RIGHT_CENTER,	// VIRTIO_SND_CHMAP_FRC
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_RLC
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_RRC
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_FLW
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_FRW 
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_FLH
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_FCH
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_FRH
    B_CHANNEL_TOP_CENTER,			// VIRTIO_SND_CHMAP_TC
    B_CHANNEL_TOP_FRONT_LEFT,		// VIRTIO_SND_CHMAP_TFL
    B_CHANNEL_TOP_FRONT_RIGHT,		// VIRTIO_SND_CHMAP_TFR
    B_CHANNEL_TOP_FRONT_CENTER,		// VIRTIO_SND_CHMAP_TFC
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_TRL
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_TRR
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_TRC
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_TFLC
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_TFRC 
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_TSL
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_TSR
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_LLFE
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_RLFE
    B_CHANNEL_BACK_CENTER,			// VIRTIO_SND_CHMAP_BC
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_BLC
    B_CHANNEL_NA,					// VIRTIO_SND_CHMAP_BRC
};


status_t
VirtIOSoundQueryChmapsInfo(VirtIOSoundDriverInfo* info)
{
	struct virtio_snd_chmap_info chmap_info[info->nChmaps];

	status_t status = VirtIOSoundQueryInfo(info, VIRTIO_SND_R_CHMAP_INFO, 0,
		info->nChmaps, sizeof(struct virtio_snd_chmap_info), (void*)chmap_info);

	if (status != B_OK)
		return status;

	return B_OK;
}