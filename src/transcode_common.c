/* 
 * MuMuDVB - UDP-ize a DVB transport stream.
 * Code for transcoding
 * 
 * Code written by Utelisys Communications B.V.
 * Copyright (C) 2009 Utelisys Communications B.V.
 *
 * The latest version can be found at http://mumudvb.braice.net
 * 
 * Copyright notice:
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include "transcode_common.h"

#include <stdlib.h>

#define FREE_TRANSOCDE_OPTION(option_name)\
if (transcode_options->option_name) {\
    free(transcode_options->option_name);\
}

void free_transcode_options(transcode_options_t *transcode_options)
{
    FREE_TRANSOCDE_OPTION(enable)
    FREE_TRANSOCDE_OPTION(video_bitrate)
    FREE_TRANSOCDE_OPTION(audio_bitrate)
    FREE_TRANSOCDE_OPTION(gop)
    FREE_TRANSOCDE_OPTION(b_frames)
    FREE_TRANSOCDE_OPTION(mbd)
    FREE_TRANSOCDE_OPTION(cmp)
    FREE_TRANSOCDE_OPTION(subcmp)
    FREE_TRANSOCDE_OPTION(video_codec)
    FREE_TRANSOCDE_OPTION(audio_codec)
    FREE_TRANSOCDE_OPTION(crf)
    FREE_TRANSOCDE_OPTION(refs)
    FREE_TRANSOCDE_OPTION(b_strategy)
    FREE_TRANSOCDE_OPTION(coder_type)
    FREE_TRANSOCDE_OPTION(me_method)
    FREE_TRANSOCDE_OPTION(me_range)
    FREE_TRANSOCDE_OPTION(subq)
    FREE_TRANSOCDE_OPTION(trellis)
    FREE_TRANSOCDE_OPTION(sc_threshold)
    FREE_TRANSOCDE_OPTION(rc_eq)
    FREE_TRANSOCDE_OPTION(qcomp)
    FREE_TRANSOCDE_OPTION(qmin)
    FREE_TRANSOCDE_OPTION(qmax)
    FREE_TRANSOCDE_OPTION(qdiff)
    FREE_TRANSOCDE_OPTION(loop_filter)
    FREE_TRANSOCDE_OPTION(mixed_refs)
    FREE_TRANSOCDE_OPTION(enable_8x8dct)
    FREE_TRANSOCDE_OPTION(x264_partitions);
    FREE_TRANSOCDE_OPTION(level)
    FREE_TRANSOCDE_OPTION(streaming_type)
    FREE_TRANSOCDE_OPTION(sdp_filename)
    FREE_TRANSOCDE_OPTION(aac_profile)
    FREE_TRANSOCDE_OPTION(aac_latm)
    FREE_TRANSOCDE_OPTION(video_scale)
    FREE_TRANSOCDE_OPTION(ffm_url)
    FREE_TRANSOCDE_OPTION(audio_channels)
    FREE_TRANSOCDE_OPTION(audio_sample_rate)
    FREE_TRANSOCDE_OPTION(video_frames_per_second)
    FREE_TRANSOCDE_OPTION(s_rtp_port)
    FREE_TRANSOCDE_OPTION(rtp_port)
    FREE_TRANSOCDE_OPTION(keyint_min)
    FREE_TRANSOCDE_OPTION(send_transcoded_only)
}
