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


static VirtIOSoundPCMInfo*
get_stream_by_nid(VirtIOSoundDriverInfo* info, uint8 direction, uint32 nid)
{
    VirtIOSoundPCMInfo* stream;

    for (uint32 i = 0; i < info->nStreams; i++) {
        stream = &info->streams[i];

        if ((stream->direction == direction) && (stream->nid == nid))
            return stream;
    }

    return NULL;
}


static status_t
create_multi_channel_info(struct virtio_snd_chmap_info info,
    multi_channel_info* mInfo)
{
    for (uint32 i = 0; i < info.channels; i++) {
        multi_channel_info* cInfo = &mInfo[i];

        cInfo->channel_id = i;

        cInfo->kind = (info.direction == VIRTIO_SND_D_OUTPUT) ?
            B_MULTI_OUTPUT_CHANNEL : B_MULTI_INPUT_CHANNEL;

        cInfo->designations = supportedChmaps[info.positions[i]];
        if (!cInfo->designations) {
            ERROR("unsupported channel designation (%u)\n", info.positions[i]);

            return B_ERROR;
        }

        if (info.channels == 2) {
            cInfo->designations |= B_CHANNEL_STEREO_BUS;
        } else if (info.channels > 2) {
            cInfo->designations |= B_CHANNEL_SURROUND_BUS;
        }
    }

    return B_OK;
}


status_t
VirtIOSoundQueryChmapsInfo(VirtIOSoundDriverInfo* info)
{
	struct virtio_snd_chmap_info chmap_info[info->nChmaps];

	status_t status = VirtIOSoundQueryInfo(info, VIRTIO_SND_R_CHMAP_INFO, 0,
		info->nChmaps, sizeof(struct virtio_snd_chmap_info), (void*)chmap_info);

	if (status != B_OK)
		return status;

    for (uint32 id = 0; id < info->nChmaps; id++) {
        VirtIOSoundPCMInfo* stream = get_stream_by_nid(info, chmap_info[id].direction,
            chmap_info[id].hdr.hda_fn_nid);

        if (stream == NULL) {
            ERROR("no matching stream for chmap (%u, %u)\n", chmap_info[id].direction, 
                chmap_info[id].hdr.hda_fn_nid);

            return B_ERROR;
        }

        stream->channels = chmap_info[id].channels;

        stream->chmap = (multi_channel_info*)calloc(stream->channels, sizeof(multi_channel_info));
        if (stream->chmap == NULL)
            return B_NO_MEMORY;

        status_t status = create_multi_channel_info(chmap_info[id], stream->chmap);
        if (status != B_OK)
            return status;
    }

	return B_OK;
}