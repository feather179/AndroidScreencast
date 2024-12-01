package com.xingyu.screencastserver.util

import android.media.MediaCodecInfo
import android.media.MediaCodecList
import android.media.MediaFormat
import com.xingyu.screencastserver.audio.AudioCodec
import com.xingyu.screencastserver.video.VideoCodec

class CodecUtils private constructor() {
    class DeviceEncoder(val codec: Codec, val codecInfo: MediaCodecInfo) {
        override fun toString(): String {
            return "DeviceEncoder{mime:${codec.mimeType} name:${codecInfo.name} alias:${codecInfo.isAlias} hw:${codecInfo.isHardwareAccelerated}"
        }
    }

    companion object {
        fun setCodecOption(format: MediaFormat, key: String, value: Any) {
            when (value) {
                is Int -> format.setInteger(key, value)
                is Long -> format.setLong(key, value)
                is Float -> format.setFloat(key, value)
                is String -> format.setString(key, value)
            }
        }

        private fun getEncoders(
            codecList: MediaCodecList,
            mimeType: String
        ): Array<MediaCodecInfo> {
            val result = ArrayList<MediaCodecInfo>()
            for (codecInfo in codecList.codecInfos) {
                if (codecInfo.isEncoder && codecInfo.supportedTypes.contains(mimeType)) {
                    result.add(codecInfo)
                }
            }
            return result.toTypedArray()
        }

        fun listVideoEncoders() : List<DeviceEncoder> {
            val encoders = ArrayList<DeviceEncoder>()
            val codecList = MediaCodecList(MediaCodecList.REGULAR_CODECS)
            for (codec in VideoCodec.entries) {
                for (codecInfo in getEncoders(codecList, codec.mimeType)) {
                    encoders.add(DeviceEncoder(codec, codecInfo))
                }
            }
            return encoders
        }

        fun listAudioEncoders() : List<DeviceEncoder> {
            val encoders = ArrayList<DeviceEncoder>()
            val codecList = MediaCodecList(MediaCodecList.REGULAR_CODECS)
            for (codec in AudioCodec.entries) {
                for (codecInfo in getEncoders(codecList, codec.mimeType)) {
                    encoders.add(DeviceEncoder(codec, codecInfo))
                }
            }
            return encoders
        }
    }
}
