package com.xingyu.screencastserver.util

interface Codec {
    enum class Type {
        VIDEO, AUDIO,
    }

    val type: Type
    val id: Int
    val codecName: String
    val mimeType: String
}
