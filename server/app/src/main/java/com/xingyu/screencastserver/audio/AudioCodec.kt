package com.xingyu.screencastserver.audio

import android.media.MediaFormat
import com.xingyu.screencastserver.util.Codec

enum class AudioCodec(
    override val id: Int,
    override val codecName: String,
    override val mimeType: String
) : Codec {
    OPUS(0x6f_70_75_73, "opus", MediaFormat.MIMETYPE_AUDIO_OPUS),
    AAC(0x00_61_61_63, "aac", MediaFormat.MIMETYPE_AUDIO_AAC),
    FLAC(0x66_6c_61_63, "flac", MediaFormat.MIMETYPE_AUDIO_FLAC),
    RAW(0x00_72_61_77, "raw", MediaFormat.MIMETYPE_AUDIO_RAW);

    override val type: Codec.Type
        get() = Codec.Type.AUDIO

    companion object {
        fun findByName(name: String): AudioCodec? {
            for (codec in entries) {
                if (codec.name == name) {
                    return codec
                }
            }
            return null
        }
    }
}