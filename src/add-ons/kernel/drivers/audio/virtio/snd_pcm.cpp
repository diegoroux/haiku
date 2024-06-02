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

#define	B_SR_NA		0x00
#define B_FMT_NA	0x00

static const uint32 supportedRates[] = {
	B_SR_NA,		// VIRTIO_SND_PCM_RATE_5512
	B_SR_8000, 		// VIRTIO_SND_PCM_RATE_8000
	B_SR_11025, 	// VIRTIO_SND_PCM_RATE_11025
	B_SR_16000,		// VIRTIO_SND_PCM_RATE_16000
	B_SR_22050,		// VIRTIO_SND_PCM_RATE_22050
	B_SR_32000,		// VIRTIO_SND_PCM_RATE_32000
	B_SR_44100,		// VIRTIO_SND_PCM_RATE_44100
	B_SR_48000,		// VIRTIO_SND_PCM_RATE_48000
	B_SR_64000,		// VIRTIO_SND_PCM_RATE_64000
	B_SR_88200,		// VIRTIO_SND_PCM_RATE_88200
	B_SR_96000,		// VIRTIO_SND_PCM_RATE_96000
	B_SR_176400,	// VIRTIO_SND_PCM_RATE_176400
	B_SR_192000,	// VIRTIO_SND_PCM_RATE_192000
	B_SR_384000,	// VIRTIO_SND_PCM_RATE_384000
};


static const uint32 supportedFormats[] = {
	B_FMT_NA, 		// VIRTIO_SND_PCM_FMT_IMA_ADPCM
	B_FMT_NA,		// VIRTIO_SND_PCM_FMT_MU_LAW
	B_FMT_NA,		// VIRTIO_SND_PCM_FMT_A_LAW
	B_FMT_8BIT_S,	// VIRTIO_SND_PCM_FMT_S8
	B_FMT_8BIT_U,	// VIRTIO_SND_PCM_FMT_U8
	B_FMT_16BIT,	// VIRTIO_SND_PCM_FMT_S16
	B_FMT_NA,		// VIRTIO_SND_PCM_FMT_U16
	B_FMT_NA,		// VIRTIO_SND_PCM_FMT_S18_3
	B_FMT_NA,		// VIRTIO_SND_PCM_FMT_U18_3
	B_FMT_NA,		// VIRTIO_SND_PCM_FMT_S20_3
	B_FMT_NA,		// VIRTIO_SND_PCM_FMT_U20_3
	B_FMT_NA,		// VIRTIO_SND_PCM_FMT_S24_3
	B_FMT_NA,		// VIRTIO_SND_PCM_FMT_U24_3
	B_FMT_20BIT,	// VIRTIO_SND_PCM_FMT_S20
	B_FMT_NA,		// VIRTIO_SND_PCM_FMT_U20
	B_FMT_24BIT,	// VIRTIO_SND_PCM_FMT_S24
	B_FMT_NA,		// VIRTIO_SND_PCM_FMT_U24
	B_FMT_32BIT,	// VIRTIO_SND_PCM_FMT_S32
	B_FMT_NA,		// VIRTIO_SND_PCM_FMT_U32
	B_FMT_FLOAT, 	// VIRTIO_SND_PCM_FMT_FLOAT
	B_FMT_DOUBLE,	// VIRTIO_SND_PCM_FMT_FLOAT64
	B_FMT_NA,		// VIRTIO_SND_PCM_FMT_DSD_U8
	B_FMT_NA,		// VIRTIO_SND_PCM_FMT_DSD_U16
	B_FMT_NA,		// VIRTIO_SND_PCM_FMT_DSD_U32
	B_FMT_NA,		// VIRTIO_SND_PCM_FMT_IEC958_SUBFRAME
};

static uint32
get_best_rate(struct virtio_snd_pcm_info info)
{
	uint64 rate = (1 << VIRTIO_SND_PCM_RATE_384000);
	uint8 i = VIRTIO_SND_PCM_RATE_384000;

	while (rate != 0) {
		if (info.rates & rate)
			return supportedRates[i];

		rate = rate >> 1;
		i--;
	}

	return B_SR_NA;
}


static uint32
rates_to_multiaudio(struct virtio_snd_pcm_info info)
{
	uint64 rate = (1 << VIRTIO_SND_PCM_RATE_384000);
	uint8 i = VIRTIO_SND_PCM_RATE_384000;

    uint64 rates = B_SR_NA;
	while (rate != 0) {
		if (info.rates & rate)
			rates |= supportedRates[i];

		rate = rate >> 1;
		i--;
	}

	return rates;
}


static uint32
get_best_fmt(struct virtio_snd_pcm_info info)
{
	uint64 fmt = (1 << VIRTIO_SND_PCM_FMT_S32);
	uint8 i = VIRTIO_SND_PCM_FMT_S32;

	while (fmt != 0) {
		if (info.formats & fmt)
			return supportedFormats[i];

		fmt = fmt >> 1;
		i--;
	}

	return B_FMT_NA;
}

status_t
VirtIOSoundQueryStreamInfo(VirtIOSoundDriverInfo* info)
{
	struct virtio_snd_pcm_info stream_info[info->nStreams];

	status_t status = VirtIOSoundQueryInfo(info, VIRTIO_SND_R_PCM_INFO, 0,
		info->nStreams, sizeof(struct virtio_snd_pcm_info), (void*)stream_info);

	if (status != B_OK)
		return status;

	VirtIOSoundPCMInfo stream;

	for (uint32 id = 0; id < info->nStreams; id++) {
		stream.stream_id = id;

		stream.features = stream_info[id].features;
		stream.formats = stream_info[id].formats;
		stream.rates = rates_to_multiaudio(stream_info[id].rates);

		stream.best_format = get_best_fmt(stream_info[id]);
		stream.best_rate = get_best_rate(stream_info[id]);

		stream.channels_min = stream_info[id].channels_min;
		stream.channels_max = stream_info[id].channels_max;

		if ((stream.best_format == B_FMT_NA) || (stream.best_rate == B_SR_NA))
			continue;

		switch (stream_info[id].direction) {
			case VIRTIO_SND_D_INPUT:
				break;
			case VIRTIO_SND_D_OUTPUT: {
				if (info->outputStream == NULL) {
					info->outputStream = (VirtIOSoundPCMInfo*)malloc(sizeof(VirtIOSoundPCMInfo));
					*info->outputStream = stream;

					break;
				}

				if (stream.best_rate > info->outputStream->best_rate)
					*info->outputStream = stream;

				break;
			}
			default:
				ERROR("unknown direction (%u)\n", stream_info[id].direction);
				status = B_ERROR;
				goto err1;
		}
	}

	if ((info->inputStream == NULL) && (info->outputStream == NULL)) {
		ERROR("unsupported PCM streams\n");
		return B_ERROR;
	}

	return B_OK;

err1:
	if (info->inputStream != NULL) free(info->inputStream);
	if (info->outputStream != NULL) free(info->outputStream);

	return status;
}