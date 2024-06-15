/*
 *  Copyright 2024, Diego Roux, diegoroux04 at proton dot me
 *  Distributed under the terms of the MIT License.
 */
#ifndef _VIRTIO_SND_H
#define _VIRTIO_SND_H


struct virtio_snd_config { 
	uint32_t jacks; 
	uint32_t streams; 
	uint32_t chmaps; 
};

enum { 
    /* jack control request types */ 
    VIRTIO_SND_R_JACK_INFO = 1, 
    VIRTIO_SND_R_JACK_REMAP, 
 
    /* PCM control request types */ 
    VIRTIO_SND_R_PCM_INFO = 0x0100, 
    VIRTIO_SND_R_PCM_SET_PARAMS, 
    VIRTIO_SND_R_PCM_PREPARE, 
    VIRTIO_SND_R_PCM_RELEASE, 
    VIRTIO_SND_R_PCM_START, 
    VIRTIO_SND_R_PCM_STOP, 
 
    /* channel map control request types */ 
    VIRTIO_SND_R_CHMAP_INFO = 0x0200, 
 
    /* jack event types */ 
    VIRTIO_SND_EVT_JACK_CONNECTED = 0x1000, 
    VIRTIO_SND_EVT_JACK_DISCONNECTED, 
 
    /* PCM event types */ 
    VIRTIO_SND_EVT_PCM_PERIOD_ELAPSED = 0x1100, 
    VIRTIO_SND_EVT_PCM_XRUN, 
 
    /* common status codes */ 
    VIRTIO_SND_S_OK = 0x8000, 
    VIRTIO_SND_S_BAD_MSG, 
    VIRTIO_SND_S_NOT_SUPP, 
    VIRTIO_SND_S_IO_ERR 
}; 
 
/* a common header */ 
struct virtio_snd_hdr { 
    uint32_t code; 
}; 
 
/* an event notification */ 
struct virtio_snd_event { 
    struct virtio_snd_hdr hdr; 
    uint32_t data; 
};

enum { 
    VIRTIO_SND_D_OUTPUT = 0, 
    VIRTIO_SND_D_INPUT 
};

struct virtio_snd_query_info { 
    struct virtio_snd_hdr   hdr;
    uint32_t                start_id;
    uint32_t                count;
    uint32_t                size;
};

struct virtio_snd_info { 
    uint32_t hda_fn_nid; 
};

struct virtio_snd_jack_hdr { 
    struct virtio_snd_hdr   hdr; 
    uint32_t                jack_id; 
};

/* supported jack features */ 
enum { 
    VIRTIO_SND_JACK_F_REMAP = 0 
}; 
 
struct virtio_snd_jack_info { 
    struct virtio_snd_info  hdr; 
    uint32_t                features; /* 1 << VIRTIO_SND_JACK_F_XXX */ 
    uint32_t                hda_reg_defconf; 
    uint32_t                hda_reg_caps; 
    uint8_t                 connected; 
 
    uint8_t                 padding[7]; 
};

struct virtio_snd_jack_remap { 
    struct virtio_snd_jack_hdr hdr; /* .code = VIRTIO_SND_R_JACK_REMAP */ 
    uint32_t association; 
    uint32_t sequence; 
};

struct virtio_snd_pcm_hdr { 
    struct virtio_snd_hdr hdr; 
    uint32_t stream_id; 
};

/* supported PCM stream features */ 
enum { 
    VIRTIO_SND_PCM_F_SHMEM_HOST = 0, 
    VIRTIO_SND_PCM_F_SHMEM_GUEST, 
    VIRTIO_SND_PCM_F_MSG_POLLING, 
    VIRTIO_SND_PCM_F_EVT_SHMEM_PERIODS, 
    VIRTIO_SND_PCM_F_EVT_XRUNS 
}; 
 
/* supported PCM sample formats */ 
enum { 
    /* analog formats (width / physical width) */ 
    VIRTIO_SND_PCM_FMT_IMA_ADPCM = 0,   /*  4 /  4 bits */ 
    VIRTIO_SND_PCM_FMT_MU_LAW,          /*  8 /  8 bits */ 
    VIRTIO_SND_PCM_FMT_A_LAW,           /*  8 /  8 bits */ 
    VIRTIO_SND_PCM_FMT_S8,              /*  8 /  8 bits */ 
    VIRTIO_SND_PCM_FMT_U8,              /*  8 /  8 bits */ 
    VIRTIO_SND_PCM_FMT_S16,             /* 16 / 16 bits */ 
    VIRTIO_SND_PCM_FMT_U16,             /* 16 / 16 bits */ 
    VIRTIO_SND_PCM_FMT_S18_3,           /* 18 / 24 bits */ 
    VIRTIO_SND_PCM_FMT_U18_3,           /* 18 / 24 bits */ 
    VIRTIO_SND_PCM_FMT_S20_3,           /* 20 / 24 bits */ 
    VIRTIO_SND_PCM_FMT_U20_3,           /* 20 / 24 bits */ 
    VIRTIO_SND_PCM_FMT_S24_3,           /* 24 / 24 bits */ 
    VIRTIO_SND_PCM_FMT_U24_3,           /* 24 / 24 bits */ 
    VIRTIO_SND_PCM_FMT_S20,             /* 20 / 32 bits */ 
    VIRTIO_SND_PCM_FMT_U20,             /* 20 / 32 bits */ 
    VIRTIO_SND_PCM_FMT_S24,             /* 24 / 32 bits */ 
    VIRTIO_SND_PCM_FMT_U24,             /* 24 / 32 bits */ 
    VIRTIO_SND_PCM_FMT_S32,             /* 32 / 32 bits */ 
    VIRTIO_SND_PCM_FMT_U32,             /* 32 / 32 bits */ 
    VIRTIO_SND_PCM_FMT_FLOAT,           /* 32 / 32 bits */ 
    VIRTIO_SND_PCM_FMT_FLOAT64,         /* 64 / 64 bits */ 
    /* digital formats (width / physical width) */ 
    VIRTIO_SND_PCM_FMT_DSD_U8,          /*  8 /  8 bits */ 
    VIRTIO_SND_PCM_FMT_DSD_U16,         /* 16 / 16 bits */ 
    VIRTIO_SND_PCM_FMT_DSD_U32,         /* 32 / 32 bits */ 
    VIRTIO_SND_PCM_FMT_IEC958_SUBFRAME  /* 32 / 32 bits */ 
}; 
 
/* supported PCM frame rates */ 
enum { 
    VIRTIO_SND_PCM_RATE_5512 = 0, 
    VIRTIO_SND_PCM_RATE_8000, 
    VIRTIO_SND_PCM_RATE_11025, 
    VIRTIO_SND_PCM_RATE_16000, 
    VIRTIO_SND_PCM_RATE_22050, 
    VIRTIO_SND_PCM_RATE_32000, 
    VIRTIO_SND_PCM_RATE_44100, 
    VIRTIO_SND_PCM_RATE_48000, 
    VIRTIO_SND_PCM_RATE_64000, 
    VIRTIO_SND_PCM_RATE_88200, 
    VIRTIO_SND_PCM_RATE_96000, 
    VIRTIO_SND_PCM_RATE_176400, 
    VIRTIO_SND_PCM_RATE_192000, 
    VIRTIO_SND_PCM_RATE_384000 
}; 
 
struct virtio_snd_pcm_info { 
    struct virtio_snd_info hdr; 
    uint32_t features; /* 1 << VIRTIO_SND_PCM_F_XXX */ 
    uint64_t formats; /* 1 << VIRTIO_SND_PCM_FMT_XXX */ 
    uint64_t rates; /* 1 << VIRTIO_SND_PCM_RATE_XXX */ 
    uint8_t direction; 
    uint8_t channels_min; 
    uint8_t channels_max; 
 
    uint8_t padding[5]; 
};

struct virtio_snd_pcm_set_params { 
    struct virtio_snd_pcm_hdr hdr; /* .code = VIRTIO_SND_R_PCM_SET_PARAMS */ 
    uint32 buffer_bytes; 
    uint32 period_bytes; 
    uint32 features; /* 1 << VIRTIO_SND_PCM_F_XXX */ 
    uint8 channels; 
    uint8 format; 
    uint8 rate; 
 
    uint8 padding; 
};

/* an I/O header */ 
struct virtio_snd_pcm_xfer { 
    uint32 stream_id; 
}; 
 
/* an I/O status */ 
struct virtio_snd_pcm_status { 
    uint32 status; 
    uint32 latency_bytes; 
};

/* standard channel position definition */ 
enum { 
    VIRTIO_SND_CHMAP_NONE = 0,  /* undefined */ 
    VIRTIO_SND_CHMAP_NA,        /* silent */ 
    VIRTIO_SND_CHMAP_MONO,      /* mono stream */ 
    VIRTIO_SND_CHMAP_FL,        /* front left */ 
    VIRTIO_SND_CHMAP_FR,        /* front right */ 
    VIRTIO_SND_CHMAP_RL,        /* rear left */ 
    VIRTIO_SND_CHMAP_RR,        /* rear right */ 
    VIRTIO_SND_CHMAP_FC,        /* front center */ 
    VIRTIO_SND_CHMAP_LFE,       /* low frequency (LFE) */ 
    VIRTIO_SND_CHMAP_SL,        /* side left */ 
    VIRTIO_SND_CHMAP_SR,        /* side right */ 
    VIRTIO_SND_CHMAP_RC,        /* rear center */ 
    VIRTIO_SND_CHMAP_FLC,       /* front left center */ 
    VIRTIO_SND_CHMAP_FRC,       /* front right center */ 
    VIRTIO_SND_CHMAP_RLC,       /* rear left center */ 
    VIRTIO_SND_CHMAP_RRC,       /* rear right center */ 
    VIRTIO_SND_CHMAP_FLW,       /* front left wide */ 
    VIRTIO_SND_CHMAP_FRW,       /* front right wide */ 
    VIRTIO_SND_CHMAP_FLH,       /* front left high */ 
    VIRTIO_SND_CHMAP_FCH,       /* front center high */ 
    VIRTIO_SND_CHMAP_FRH,       /* front right high */ 
    VIRTIO_SND_CHMAP_TC,        /* top center */ 
    VIRTIO_SND_CHMAP_TFL,       /* top front left */ 
    VIRTIO_SND_CHMAP_TFR,       /* top front right */ 
    VIRTIO_SND_CHMAP_TFC,       /* top front center */ 
    VIRTIO_SND_CHMAP_TRL,       /* top rear left */ 
    VIRTIO_SND_CHMAP_TRR,       /* top rear right */ 
    VIRTIO_SND_CHMAP_TRC,       /* top rear center */ 
    VIRTIO_SND_CHMAP_TFLC,      /* top front left center */ 
    VIRTIO_SND_CHMAP_TFRC,      /* top front right center */ 
    VIRTIO_SND_CHMAP_TSL,       /* top side left */ 
    VIRTIO_SND_CHMAP_TSR,       /* top side right */ 
    VIRTIO_SND_CHMAP_LLFE,      /* left LFE */ 
    VIRTIO_SND_CHMAP_RLFE,      /* right LFE */ 
    VIRTIO_SND_CHMAP_BC,        /* bottom center */ 
    VIRTIO_SND_CHMAP_BLC,       /* bottom left center */ 
    VIRTIO_SND_CHMAP_BRC        /* bottom right center */ 
}; 
 
/* maximum possible number of channels */ 
#define VIRTIO_SND_CHMAP_MAX_SIZE 18
 
struct virtio_snd_chmap_info { 
    struct virtio_snd_info hdr; 
    uint8 direction; 
    uint8 channels; 
    uint8 positions[VIRTIO_SND_CHMAP_MAX_SIZE]; 
};

#endif