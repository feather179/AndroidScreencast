package com.xingyu.screencastserver.video

import android.media.MediaFormat
import com.xingyu.screencastserver.util.Codec

enum class VideoCodec(
    override val id: Int,
    override val codecName: String,
    override val mimeType: String
) : Codec {
    H264(0x68_32_36_34, "h264", MediaFormat.MIMETYPE_VIDEO_AVC),
    H265(0x68_32_36_35, "h265", MediaFormat.MIMETYPE_VIDEO_HEVC),
    AV1(0x00_61_76_31, "av1", MediaFormat.MIMETYPE_VIDEO_AV1);

    override val type: Codec.Type
        get() = Codec.Type.VIDEO

    companion object {
        fun findByName(name: String): VideoCodec? {
            for (codec in VideoCodec.entries) {
                if (codec.name == name) {
                    return codec
                }
            }
            return null
        }
    }
}
