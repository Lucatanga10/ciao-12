package com.ojbs.overlay

data class Box(
    val left: Float,
    val top: Float,
    val right: Float,
    val bottom: Float,
    val argb: Int,
    val health: Float,
    val name: String
) {
    val width: Float
        get() = right - left

    val height: Float
        get() = bottom - top
}
