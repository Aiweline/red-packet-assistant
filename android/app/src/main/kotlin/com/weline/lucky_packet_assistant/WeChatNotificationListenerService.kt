package com.weline.lucky_packet_assistant

import android.app.Notification
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification

class WeChatNotificationListenerService : NotificationListenerService() {
    override fun onNotificationPosted(statusBarNotification: StatusBarNotification) {
        if (statusBarNotification.packageName != AutomationState.weChatPackage) return

        val notification = statusBarNotification.notification
        val extras = notification.extras
        val parts = listOf(
            extras.getCharSequence(Notification.EXTRA_TITLE),
            extras.getCharSequence(Notification.EXTRA_TEXT),
            extras.getCharSequence(Notification.EXTRA_BIG_TEXT),
            extras.getCharSequence(Notification.EXTRA_SUB_TEXT),
        )
        if (!RedPacketTextMatcher.isActionable(parts)) return

        AutomationState.recordDetected(this, "检测到微信红包通知")
        if (!AutomationState.isEnabled(this)) return

        try {
            notification.contentIntent?.send()
            AutomationState.recordOpened(this, "已打开红包所在会话")
        } catch (_: Exception) {
            AutomationState.recordOpened(this, "红包通知已发现，但会话打开失败")
        }
    }
}
