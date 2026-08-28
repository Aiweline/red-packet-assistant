package com.weline.lucky_packet_assistant

object RedPacketTextMatcher {
    private val positiveMarkers = listOf("[微信红包]", "微信红包", "发来一个红包")
    private val terminalMarkers = listOf("已领取", "已被领完", "已领完", "已过期", "已退款")

    fun isActionable(parts: Iterable<CharSequence?>): Boolean {
        val text = parts.filterNotNull().joinToString(" ")
        return positiveMarkers.any(text::contains) &&
            terminalMarkers.none(text::contains)
    }
}
