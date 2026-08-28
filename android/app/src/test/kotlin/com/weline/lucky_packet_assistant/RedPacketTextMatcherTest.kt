package com.weline.lucky_packet_assistant

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RedPacketTextMatcherTest {
    @Test
    fun matchesActionableWechatPackets() {
        assertTrue(RedPacketTextMatcher.isActionable(listOf("群聊", "[微信红包] 恭喜发财")))
        assertTrue(RedPacketTextMatcher.isActionable(listOf("小明发来一个红包")))
    }

    @Test
    fun ignoresTerminalAndUnrelatedMessages() {
        assertFalse(RedPacketTextMatcher.isActionable(listOf("微信红包已被领完")))
        assertFalse(RedPacketTextMatcher.isActionable(listOf("红包封面上新")))
    }
}
